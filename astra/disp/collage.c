#include "main.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static draw_state st;

typedef struct collage_img {
  GLuint tex;
  int order;
  double c_ra, c_dec;
  float *coeff;
} collage_img;
collage_img *imgs = NULL;
size_t n_imgs = 0, cap_imgs = 0;

static vec3 *imgpos;

static int *seq;
static inline void find_seq();

// Tangent directions
// [i N + j]: starting from i, heading towards j along the great circle
static vec3 *tg_gc = NULL;
static float *dist_gc = NULL;
// Cached scores
// [i N^2 + i N + j]: dist_score(i, j)^2
// [i N^2 + j N + k]: sphere_score(i, j, k)^2
static float *sphere_scores = NULL;

static quat *waypts = NULL;

void setup_collage()
{
  st = state_new();
  state_shader_files(&st, "collage.vert", "collage.frag");

  // List images
  const char *const img_path = "../img-processed";
  size_t img_path_l = strlen(img_path);

  DIR *dir = opendir(img_path);
  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL) {
    size_t l = strlen(ent->d_name);
    if (memcmp(ent->d_name + l - 6, ".coeff", 6) == 0) {
      char *coeff_file = (char *)malloc(img_path_l + 1 + l + 1);
      memcpy(coeff_file, img_path, img_path_l);
      coeff_file[img_path_l] = '/';
      memcpy(coeff_file + img_path_l + 1, ent->d_name, l + 1);
      char *img_file = (char *)malloc(img_path_l + 1 + l - 6 + 4 + 1);
      memcpy(img_file, img_path, img_path_l);
      img_file[img_path_l] = '/';
      memcpy(img_file + img_path_l + 1, ent->d_name, l - 6);
      memcpy(img_file + img_path_l + 1 + l - 6, ".png", 4 + 1);
      printf("Load %s\n", img_file);

      if (n_imgs >= cap_imgs) {
        cap_imgs = (cap_imgs == 0 ? 8 : cap_imgs * 2);
        imgs = (collage_img *)realloc(imgs, sizeof(collage_img) * cap_imgs);
      }
      imgs[n_imgs].tex = texture_loadfile(img_file);

      // Load coefficients
      FILE *fp = fopen(coeff_file, "r");
      assert(fp != NULL);
      fscanf(fp, "%d%lf%lf", &imgs[n_imgs].order,
        &imgs[n_imgs].c_ra, &imgs[n_imgs].c_dec);
      imgs[n_imgs].c_ra *= (M_PI / 180);
      imgs[n_imgs].c_dec *= (M_PI / 180);
      int n_coeffs = (imgs[n_imgs].order + 1) * (imgs[n_imgs].order + 2);
      imgs[n_imgs].coeff = (float *)malloc(sizeof(float) * n_coeffs);
      for (int i = 0; i < n_coeffs; i++)
        fscanf(fp, "%f", &imgs[n_imgs].coeff[i]);
      fclose(fp);

      n_imgs++;

      free(coeff_file);
      free(img_file);
      //if (n_imgs >= 16) break;
    }
  }

  // 3D positions of images
  imgpos = (vec3 *)malloc(sizeof(vec3) * n_imgs);
  for (int i = 0; i < n_imgs; i++) {
    imgpos[i] = (vec3){
      cos(imgs[i].c_dec) * cos(imgs[i].c_ra),
      cos(imgs[i].c_dec) * sin(imgs[i].c_ra),
      sin(imgs[i].c_dec)
    };
  }

  // Order
  find_seq();
  for (int i = 0; i < n_imgs; i++) {
    int j = seq[i];
    printf("(%8.5f,%8.5f,%8.5f) %3d",
      imgpos[j].x, imgpos[j].y, imgpos[j].z, j);
    if (i >= 2)
      printf(" %8.5f", sphere_scores[(seq[i - 2] * n_imgs + seq[i - 1]) * n_imgs + seq[i]]);
    putchar('\n');
  }
  // Rotation waypoints
  waypts = (quat *)malloc(sizeof(quat) * n_imgs * 3);
  for (int i = 0; i < n_imgs; i++) {
    vec3 right = (vec3){0, 0, 0};
    if (i > 0) right = vec3_mul(tg_gc[seq[i] * n_imgs + seq[i - 1]], -1);
    if (i < n_imgs - 1)
      right = vec3_normalize(vec3_add(right, tg_gc[seq[i] * n_imgs + seq[i + 1]]));
    waypts[i * 3] = rot_from_view(imgpos[seq[i]], right);
  }
  for (int i = 0; i < n_imgs; i++) {
    quat q_in = (quat){0, 0, 0, 1};
    quat q_ou = (quat){0, 0, 0, 1};
    if (i > 0)
      q_in = quat_minorarc(waypts[(i - 1) * 3], waypts[i * 3]);
    if (i < n_imgs - 1)
      q_ou = quat_minorarc(waypts[i * 3], waypts[(i + 1) * 3]);
    if (i == 0) q_in = q_ou;
    if (i == n_imgs - 1) q_ou = q_in;
    quat log_q_in = quat_log(q_in);
    quat log_q_ou = quat_log(q_ou);
    quat q_offs = quat_exp((quat){
      (log_q_in.x + log_q_ou.x) / 2,
      (log_q_in.y + log_q_ou.y) / 2,
      (log_q_in.z + log_q_ou.z) / 2,
      (log_q_in.w + log_q_ou.w) / 2
    });
    q_offs = quat_pow(q_offs, 1/3.f);
    if (i < n_imgs - 1)
      waypts[i * 3 + 1] = quat_mul(q_offs, waypts[i * 3]);
    if (i > 0)
      waypts[i * 3 - 1] = quat_mul(quat_inv(q_offs), waypts[i * 3]);
  }
  // exit(0);

  // Entire screen
  st.stride = 2;
  state_attr(st, 0, 0, 2);
  const float fullscreen_coords[12] = {
    -1.0, -1.0, -1.0,  1.0,  1.0,  1.0,
    -1.0, -1.0,  1.0, -1.0,  1.0,  1.0,
  };
  state_buffer(&st, 6, fullscreen_coords);
}

static int T = 0, start = 0;

static inline quat de_casteljau_cubic(
  quat q0, quat q1, quat q2, quat q3,
  float t)
{
  quat i10 = quat_slerp(q0, q1, t);
  quat i11 = quat_slerp(q1, q2, t);
  quat i12 = quat_slerp(q2, q3, t);
  quat i20 = quat_slerp(i10, i11, t);
  quat i21 = quat_slerp(i11, i12, t);
  quat i30 = quat_slerp(i20, i21, t);
  return i30;
}

#define INTERVAL 1200
void update_collage()
{
  if (++T == INTERVAL) {
    start = (start + 1) % n_imgs;
    T = 0;
    printf("%d (%.4f,%.4f,%.4f)\n", seq[start],
      imgpos[seq[start]].x, imgpos[seq[start]].y, imgpos[seq[start]].z);
  }
  float t = (float)T / INTERVAL;
  if (start < n_imgs - 1)
    view_ori = de_casteljau_cubic(
      waypts[start * 3 + 0],
      waypts[start * 3 + 1],
      waypts[start * 3 + 2],
      waypts[start * 3 + 3],
      t
    );
}

void draw_collage()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  for (int _i = 0; _i <= 10; _i++) {
    int i = seq[(start + n_imgs - 9 + _i) % n_imgs];
    int n_coeffs = (imgs[i].order + 1) * (imgs[i].order + 2);
    state_uniform2f(st, "projCen", imgs[i].c_ra, imgs[i].c_dec);
    state_uniform1i(st, "ord", imgs[i].order);
    state_uniform2fv(st, "coeff", n_coeffs / 2, imgs[i].coeff);
    texture_bind(imgs[i].tex, 0);
    state_uniform1f(st, "seed", i);
    state_uniform1f(st, "entertime", (float)(T + (9.3f - _i) * INTERVAL) / 240);
    float exittime = (float)(T + (4.3f - _i) * INTERVAL) / 240;
    state_uniform1f(st, "exittime", exittime);
    if (exittime >= -2) {
     // Draw semi-transparent
      state_uniform1i(st, "transp", 1);
      state_draw(st);
    }
    state_uniform1i(st, "transp", 0);
    state_draw(st);
  }
  glDisable(GL_BLEND);
}

// Sequence determination by genetic algorithm

static inline float dist_score(float o)
{
  if (o < 0.5) return (0.5 - o) * (0.5 - o) * 0.64;
  if (o > 0.5) return (o - 0.5) * (o - 0.5) * 0.49;
  return 0;
}
static inline float sphere_score(int a, int b, int c)
{
  // The angle between the two great circles
  // passing between A-B and B-C, respectively
  // Equal to: arccos of dot product of derivatives of
  // -slerp(B, A, t) and slerp(B, C, t) at t = 0
  float cos_angle = -vec3_dot(
    tg_gc[b * n_imgs + a],
    tg_gc[b * n_imgs + c]);
  float angle = acosf(cos_angle);
  float angle_score = (angle / (0.5*M_PI)) * (angle / (0.5*M_PI));
  float o = dist_gc[b * n_imgs + c];
/*
  printf("- tan A-B: %8.5f,%8.5f,%8.5f\n", tg_gc[b * n_imgs + a].x, tg_gc[b * n_imgs + a].y, tg_gc[b * n_imgs + a].z);
  printf("- tan B-C: %8.5f,%8.5f,%8.5f\n", tg_gc[b * n_imgs + c].x, tg_gc[b * n_imgs + c].y, tg_gc[b * n_imgs + c].z);
  printf("- angle: %.6f, dist: %.6f, dist-score: %.6f\n", angle, o, dist_score(o));
*/
  return angle_score + dist_score(o);
}

static inline float eval_seq(int *seq)
{
  float score = sphere_scores[seq[0] * (n_imgs + 1) * n_imgs + seq[1]];
  for (int i = 2; i < n_imgs; i++)
    score += sphere_scores[(seq[i - 2] * n_imgs + seq[i - 1]) * n_imgs + seq[i]];
  return score;
}

// Beginning of xoshiro256starstar.c

#include <stdint.h>

/* This is xoshiro256** 1.0, one of our all-purpose, rock-solid
   generators. It has excellent (sub-ns) speed, a state (256 bits) that is
   large enough for any parallel application, and it passes all tests we
   are aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

static inline uint64_t rotl(const uint64_t x, int k) {
  return (x << k) | (x >> (64 - k));
}
static uint64_t s[4];
static uint64_t rand_next(void) {
  const uint64_t result = rotl(s[1] * 5, 7) * 9;
  const uint64_t t = s[1] << 17;
  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];
  s[2] ^= t;
  s[3] = rotl(s[3], 45);
  return result;
}

// End of xoshiro256starstar.c

static inline void pmx(
  const int *restrict a,
  const int *restrict b,
  int *restrict o,
  int *restrict map)
{
  int n = n_imgs;
  int l = rand_next() % n, r = rand_next() % (n - 1);
  if (l == r) r = n - 1;
  if (l > r) { int t = l; l = r; r = t; }

  memset(map, -1, sizeof(int) * n);
  for (int i = l; i < r; i++) {
    o[i] = a[i];
    map[a[i]] = b[i];
  }
  for (int i = 0; i < l; i++)
    for (o[i] = b[i]; map[o[i]] != -1; o[i] = map[o[i]]) { }
  for (int i = r; i < n; i++)
    for (o[i] = b[i]; map[o[i]] != -1; o[i] = map[o[i]]) { }
}

static inline void mut(int *a)
{
  int n = n_imgs;
  int l = rand_next() % n, r = rand_next() % (n - 1);
  if (l == r) r = n - 1;
  if (rand_next() % 2 == 0) {
    int t = a[l]; a[l] = a[r]; a[r] = t;
  } else {
    int t = a[l];
    if (l < r) for (int i = l + 1; i <= r; i++) a[i - 1] = a[i];
    else for (int i = l - 1; i >= r; i--) a[i + 1] = a[i];
    a[r] = t;
  }
}

#define RADIX_BITS  8
#define RADIX_SIZE  (1 << RADIX_BITS)
#define RADIX_MASK  (RADIX_SIZE - 1)
#define RADIX_ITS   4
static inline void radixsort_inner(
  int n, uint32_t *restrict a, uint32_t *restrict b, int digit)
{
  // Stride = 2
#define A(_i) a[(_i) * 2]

  int c[RADIX_SIZE] = { 0 };
  for (int i = 0; i < n; i++)
    c[(A(i) >> (RADIX_BITS * digit)) & RADIX_MASK] += 1;
  for (int i = 0, s = 0; i < RADIX_SIZE; i++) {
    int t = c[i]; c[i] = s; s += t;
  }
  for (int i = 0; i < n; i++) {
    int index = c[(A(i) >> (RADIX_BITS * digit)) & RADIX_MASK]++;
    b[index * 2 + 0] = a[i * 2 + 0];
    b[index * 2 + 1] = a[i * 2 + 1];
  }

#undef A
}
// Scratch space size is n * 16 bytes
static inline void sort_genomes(
  char *restrict p, int n, int elmsz, char *restrict p_out, int n_out, char *restrict scratch)
{
  // Sorts each element according to the first two bytes bitcast to FP32/U32
  uint32_t *a = (uint32_t *)scratch;
  uint32_t *b = ((uint32_t *)scratch + n * 2);

  for (int i = 0; i < n; i++) {
    a[i * 2 + 0] = *(uint32_t *)(p + i * elmsz);
    a[i * 2 + 1] = i;
  }

  // Extra care should be taken if RADIX_ITS is changed to an odd number
  for (int d = 0; d < RADIX_ITS; d += 2) {
    radixsort_inner(n, a, b, d);
    radixsort_inner(n, b, a, d + 1);
  }
  for (int i = 0; i < n_out; i++)
    memcpy(p_out + i * elmsz, p + a[i * 2 + 1] * elmsz, elmsz);
}
#undef RADIX_BITS
#undef RADIX_SIZE
#undef RADIX_MASK
#undef RADIX_ITS

static inline void find_seq()
{
  s[0] = 0x8ff333dac9f8d20bULL;
  s[1] = 0x1b2f7f552f1bca4eULL;
  s[2] = 0xefbc429aee43484eULL;
  s[3] = 0x8d600265e6b6a70dULL;

  seq = (int *)malloc(sizeof(int) * n_imgs);

  // Tangent along great circles
  tg_gc = (vec3 *)realloc(tg_gc, sizeof(vec3) * n_imgs * n_imgs);
  dist_gc = (float *)realloc(dist_gc, sizeof(float) * n_imgs * n_imgs);
  for (int i = 0; i < n_imgs; i++) {
    vec3 a = imgpos[i];
    for (int j = 0; j < n_imgs; j++) {
      vec3 b = imgpos[j];
      float oAB = acosf(vec3_dot(a, b));
      dist_gc[i * n_imgs + j] = oAB;
      vec3 dAB = vec3_normalize(vec3_add(
        vec3_mul(a, -oAB * cosf(oAB) / sinf(oAB)),
        vec3_mul(b, oAB / sinf(oAB))));
      tg_gc[i * n_imgs + j] = dAB;
    }
  }

  sphere_scores = (float *)realloc(sphere_scores,
    sizeof(float) * n_imgs * n_imgs * n_imgs);
  for (int i = 0; i < n_imgs; i++)
    for (int j = 0; j < n_imgs; j++) if (j != i) {
      float s2 = dist_score(dist_gc[i * n_imgs + j]);
      sphere_scores[(i * n_imgs + i) * n_imgs + j] = s2 * s2;
      for (int k = 0; k < n_imgs; k++) if (k != i && k != j) {
        float s3 = sphere_score(i, j, k);
        sphere_scores[(i * n_imgs + j) * n_imgs + k] = s3 * s3;
      }
    }
  /*
  int a[] = {
55,174,35,165,122,100,150,95,11,61,19,153,18,112,180,99,145,78,0,16,21,137,5,172,164,38,105,94,54,116,140,73,42,177,79,51,24,89,36,146,167,52,141,127,12,50,173,30,115,163,88,108,148,176,91,64,166,77,149,103,147,98,6,128,155,175,168,29,179,138,85,39,41,71,45,171,47,13,126,113,53,22,49,102,136,93,119,107,1,118,96,83,90,80,162,66,60,43,111,139,169,120,3,32,20,7,158,142,44,10,178,58,125,57,104,97,160,15,114,62,132,92,4,170,33,152,68,87,72,46,82,110,106,28,76,161,9,117,63,17,134,74,144,26,23,154,131,67,69,143,156,121,124,25,65,40,133,59,101,135,123,81,2,84,129,130,86,48,8,151,159,14,27,75,56,34,109,157,31,70,37,181
  };
  memcpy(seq, a, sizeof a);
  return;
  */

#define DEDUP 0
#if DEDUP
  // Hash table for deduplication
  #define HASH_SIZE 100003
  typedef uint32_t hash_t;
  static hash_t ht[HASH_SIZE];
  memset(ht, -1, sizeof ht);
#endif

  const int N_ROUNDS = 1000;
  const int N_POP = 10000;
  const int N_REP = 15000;
#if DEDUP
  #define size (sizeof(float) + sizeof(int) * n_imgs + sizeof(hash_t))
  #define hash(_i) (*(hash_t *)(_pop + (_i) * size + sizeof(float) + sizeof(int) * n_imgs))
#else
  #define size (sizeof(float) + sizeof(int) * n_imgs)
#endif
  #define start(_i) (_pop + (_i) * size)
  #define perm(_i) ((int *)(_pop + (_i) * size + sizeof(float)))
  #define val(_i) (*(float *)(_pop + (_i) * size))
  char *_poppool = (char *)malloc((N_POP + N_REP) * size * 2);
  char *_pop = _poppool, *_npop = _poppool + (N_POP + N_REP) * size;

  int *parents = (int *)malloc(sizeof(int) * N_REP * 2);
  char *sort_scratch = (char *)malloc(16 * (N_POP + N_REP));
  int *pmx_scratch = (int *)malloc(sizeof(int) * n_imgs);

  for (int i = 0; i < N_POP; i++) {
    int *p = perm(i);
    for (int j = 0; j < n_imgs; j++) {
      int k = rand_next() % (j + 1);
      p[j] = p[k];
      p[k] = j;
    }
    val(i) = eval_seq(p);
  #if DEDUP
    hash_t h = 0;
    for (int k = 0; k < n_imgs; k++) h = h * n_imgs + perm(i)[k];
    hash(i) = h;
  #endif
  }
  clock_t lastclk = clock();
  for (int it = 0; it < N_ROUNDS; it++) {
  #if DEDUP
    // Add population to hash table
    for (int i = 0; i < N_POP; i++) {
      int id = hash(i) % HASH_SIZE;
      while (ht[id] != -1) id = (id + 1) % HASH_SIZE;
      ht[id] = hash(i);
    }
  #endif
    // Offsprings
    for (int r = 0; r < N_REP; r++) {
      int i = rand_next() % N_POP;
      int j = rand_next() % (N_POP - 1);
      if (i == j) j = N_POP - 1;
      parents[r * 2 + 0] = i;
      parents[r * 2 + 1] = j;
    }
    for (int r = 0; r < N_REP; r++) {
      pmx(perm(parents[r * 2 + 0]), perm(parents[r * 2 + 1]), perm(N_POP + r), pmx_scratch);
      mut(perm(N_POP + r));
      val(N_POP + r) = eval_seq(perm(N_POP + r));
    #if DEDUP
      // Deduplicate
      hash_t h = 0;
      for (int k = 0; k < n_imgs; k++) h = h * n_imgs + perm(N_POP + r)[k];
      hash(N_POP + r) = h;
      int id = h % HASH_SIZE;
      int dup = 0;
      for (int id = h % HASH_SIZE; ht[id] != -1; id = (id + 1) % HASH_SIZE)
        if (ht[id] == h) { dup++; if (dup >= 8) break; }
      if (dup && rand_next() % (1 << (dup * 2)) != 0) { r--; continue; }
    #endif
    }
    sort_genomes(_pop, N_POP + N_REP, size, _npop, N_POP, sort_scratch);
    char *_t = _pop; _pop = _npop; _npop = _t;
    if ((it + 1) * 100 / N_ROUNDS != it * 100 / N_ROUNDS) {
      clock_t now = clock();
      printf("== It. %d ==  (%.8lf)\n", it + 1, (double)(now - lastclk) / CLOCKS_PER_SEC);
      for (int i = 0; i < 5; i++) {
        printf("%9.5f |", val(i));
        for (int k = 0; k < n_imgs; k++) printf(" %2d", perm(i)[k]); putchar('\n');
      }
      lastclk = now;
    }
  #if DEDUP
    // Remove population from hash table
    for (int i = 0; i < N_POP; i++) {
      int id = hash(i) % HASH_SIZE;
      while (ht[id] != -1) { ht[id] = -1; id = (id + 1) % HASH_SIZE; }
    }
  #endif
  }

  memcpy(seq, perm(0), sizeof(int) * n_imgs);

  free(parents);
  free(sort_scratch);
  free(pmx_scratch);

  #undef start
  #undef perm
  #undef val
  free(_poppool);
}

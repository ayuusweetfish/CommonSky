#include "main.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

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

extern float *sphere_scores;

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

void draw_collage()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  static int T = 0, start = 0;
  if (++T == 8) { start = (start + 1) % n_imgs; T = 0; printf("%d\n", seq[start]); }
  for (int _i = 0; _i < 10; _i++) {
    int i = seq[(start + n_imgs - 9 + _i) % n_imgs];
    int n_coeffs = (imgs[i].order + 1) * (imgs[i].order + 2);
    state_uniform2f(st, "projCen", imgs[i].c_ra, imgs[i].c_dec);
    state_uniform1i(st, "ord", imgs[i].order);
    state_uniform2fv(st, "coeff", n_coeffs / 2, imgs[i].coeff);
    texture_bind(imgs[i].tex, 0);
    state_draw(st);
  }
  glDisable(GL_BLEND);
}

// Sequence determination by genetic algorithm

vec3 *tg_gc = NULL;
float *dist_gc = NULL;
// Cached scores
// [i N^2 + i N + j]: dist_score(i, j)^2
// [i N^2 + j N + k]: sphere_score(i, j, k)^2
float *sphere_scores = NULL;

static inline float dist_score(float o)
{
  // A distance of zero contributes equally with a turn of 67.5 degrees
  if (o < 0.5) return (0.5 - o) * (0.5 - o) * 2.25;
  // A distance of a hemicircle contributes equally with a turn of 72 degrees
  if (o > 2.0) return (o - 2.0) * (o - 2.0) * 0.49;
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
  float angle = acosf(fabsf(cos_angle));
  float angle_score = (angle / (0.5*M_PI)) * (angle / (0.5*M_PI));
  float o = dist_gc[b * n_imgs + c];
  if (cos_angle < 0) o = 2*M_PI - o;
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
  return -score;
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

static int *pmx_map = NULL;
static inline void pmx(
  const int *restrict a,
  const int *restrict b,
  int *restrict o)
{
  int n = n_imgs;
  int l = rand_next() % n, r = rand_next() % (n - 1);
  if (l == r) r = n - 1;
  if (l > r) { int t = l; l = r; r = t; }

  memset(pmx_map, -1, sizeof(int) * n);
  for (int i = l; i < r; i++) {
    o[i] = a[i];
    pmx_map[a[i]] = b[i];
  }
  for (int i = 0; i < l; i++)
    for (o[i] = b[i]; pmx_map[o[i]] != -1; o[i] = pmx_map[o[i]]) { }
  for (int i = r; i < n; i++)
    for (o[i] = b[i]; pmx_map[o[i]] != -1; o[i] = pmx_map[o[i]]) { }
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

static inline int genome_cmp(const void *a, const void *b)
{
  float diff = *(float *)a - *(float *)b;
  return (diff < 0 ? +1 : diff > 0 ? -1 : 0);
}

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
  char *_pop = (char *)malloc((N_POP + N_REP) * size);

  pmx_map = (int *)realloc(pmx_map, sizeof(int) * n_imgs);

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
      pmx(perm(i), perm(j), perm(N_POP + r));
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
    qsort(_pop, N_POP + N_REP, size, genome_cmp);
    if ((it + 1) * 100 / N_ROUNDS != it * 100 / N_ROUNDS) {
      printf("== It. %d ==\n", it + 1);
      for (int i = 0; i < 5; i++) {
        printf("%9.5f |", val(i));
        for (int k = 0; k < n_imgs; k++) printf("%3d", perm(i)[k]); putchar('\n');
      }
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

  #undef start
  #undef perm
  #undef val
  free(_pop);
}

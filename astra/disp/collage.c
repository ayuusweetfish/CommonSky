#include "main.h"

#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static draw_state st;

static canvas can_transp;
static draw_state st_cubetocube;

static canvas can_frozen;
static draw_state st_cubetoscr;

typedef struct collage_img {
  char *img_path;
  GLuint tex;
  int order;
  double c_ra, c_dec;
  float *coeff;
} collage_img;
collage_img *imgs = NULL;
size_t n_imgs = 0, cap_imgs = 0;

static vec3 *imgpos;

static int *seq;
static inline void evo_(bool does_evo);

#define N_CPTS  320
#define CHRO_LEN  (N_CPTS * 2)
#define CHRO_SZ   (CHRO_LEN / 8)

static const float ROTA_STEP = 2*M_PI / 30;
static const float ROTA_TILT = 2*M_PI / 120;

static vec3 trace[N_CPTS];
static quat *waypts = NULL;
static inline quat trace_at(float t);

typedef struct fade_in_pt {
  int id;
  float time;
} fade_in_pt;
fade_in_pt *ins = NULL;

static int cmp_img(const void *a, const void *b)
{
  return strcmp(((const collage_img *)a)->img_path, ((const collage_img *)b)->img_path);
}

static int cmp_fade_in_pt(const void *a, const void *b)
{
  float ta = ((const fade_in_pt *)a)->time;
  float tb = ((const fade_in_pt *)b)->time;
  return (ta < tb ? -1 : ta > tb ? +1 : 0);
}

static inline void load_collage_files();
void setup_collage()
{
  load_collage_files();

  st = state_new();
  state_shader_files(&st, "collage.vert", "collage.frag");
  st.stride = 2;
  state_attr(st, 0, 0, 2);
  const float fullscreen_coords[12] = {
    -1.0, -1.0, -1.0,  1.0,  1.0,  1.0,
    -1.0, -1.0,  1.0, -1.0,  1.0,  1.0,
  };
  state_buffer(&st, 6, fullscreen_coords);

  can_transp = canvas_new(3200, 2400);

  st_cubetocube = state_new();
  state_shader_files(&st_cubetocube, "plain.vert", "texblit.frag");
  state_uniform1i(st_cubetocube, "tex", 0);
  st_cubetocube.stride = 2;
  state_attr(st_cubetocube, 0, 0, 2);
  state_buffer(&st_cubetocube, 6, fullscreen_coords);

  can_frozen = canvas_new(3200, 2400);

  st_cubetoscr = state_new();
  state_shader_files(&st_cubetoscr, "plain.vert", "cubemap.frag");
  state_uniform1i(st_cubetoscr, "cubemap", 0);
  st_cubetoscr.stride = 2;
  state_attr(st_cubetoscr, 0, 0, 2);
  state_buffer(&st_cubetoscr, 6, fullscreen_coords);
}

static inline void load_collage_files()
{
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
      // printf("Load %s\n", img_file);

      if (n_imgs >= cap_imgs) {
        cap_imgs = (cap_imgs == 0 ? 8 : cap_imgs * 2);
        imgs = (collage_img *)realloc(imgs, sizeof(collage_img) * cap_imgs);
      }
      imgs[n_imgs].img_path = img_file;
      imgs[n_imgs].tex = 0;

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
      //if (n_imgs >= 16) break;
    }
  }
  qsort(imgs, n_imgs, sizeof(collage_img), cmp_img);

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
  evo_(false);
  // Rotation waypoints
  waypts = (quat *)malloc(sizeof(quat) * N_CPTS * 3);
  for (int i = 0; i < N_CPTS; i++) {
    vec3 right = (vec3){0, 0, 0};
    if (i > 0) right = sph_tan(trace[i - 1], trace[i], 1);
    if (i < N_CPTS - 1)
      right = vec3_normalize(vec3_add(right, sph_tan(trace[i], trace[i + 1], 0)));
    waypts[i * 3] = rot_from_view(trace[i], right);
  }
  for (int i = 0; i < N_CPTS; i++) {
    quat q_in = (quat){0, 0, 0, 1};
    quat q_ou = (quat){0, 0, 0, 1};
    if (i > 0)
      q_in = quat_minorarc(waypts[(i - 1) * 3], waypts[i * 3]);
    if (i < N_CPTS - 1)
      q_ou = quat_minorarc(waypts[i * 3], waypts[(i + 1) * 3]);
    if (i == 0) q_in = q_ou;
    if (i == N_CPTS - 1) q_ou = q_in;
    quat log_q_in = quat_log(q_in);
    quat log_q_ou = quat_log(q_ou);
    quat q_offs = quat_exp((quat){
      (log_q_in.x + log_q_ou.x) / 2,
      (log_q_in.y + log_q_ou.y) / 2,
      (log_q_in.z + log_q_ou.z) / 2,
      (log_q_in.w + log_q_ou.w) / 2
    });
    q_offs = quat_pow(q_offs, 1/3.f);
    if (i < N_CPTS - 1)
      waypts[i * 3 + 1] = quat_mul(q_offs, waypts[i * 3]);
    if (i > 0)
      waypts[i * 3 - 1] = quat_mul(quat_inv(q_offs), waypts[i * 3]);
  }

  // Best positions
  ins = (fade_in_pt *)malloc(n_imgs * sizeof(fade_in_pt));
  for (int i = 0; i < n_imgs; i++) {
    const float step = 0.1;
    const int n_steps = (int)(N_CPTS / step);
    float best = 2, best_at;
    for (int j = 0; j < n_steps; j++) {
      float t = (float)N_CPTS * (j + (float)i / n_imgs) / n_steps;
      vec3 p = rot_by_quat((vec3){0, 0, -1}, trace_at(t));
      float d = vec3_distsq(p, imgpos[i]);
      if (best > d) { best = d; best_at = t; }
    }
    ins[i].id = i;
    ins[i].time = best_at;
  }
  qsort(ins, n_imgs, sizeof(fade_in_pt), cmp_fade_in_pt);
  for (int i = 0; i < n_imgs; i++)
    printf("%8.5f %3d (%.5f,%.5f,%.5f)\n", ins[i].time, ins[i].id,
      imgpos[ins[i].id].x, imgpos[ins[i].id].y, imgpos[ins[i].id].z);
}

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

static inline quat trace_at(float t)
{
  int ti = (int)floorf(t);
  t -= ti;
  if (ti < 0) {
    quat d = waypts[0];
    quat c = quat_mul(quat_minorarc(waypts[1], waypts[0]), d);
    quat b = quat_mul(quat_minorarc(waypts[2], waypts[1]), c);
    quat a = quat_mul(quat_minorarc(waypts[3], waypts[2]), b);
    return de_casteljau_cubic(a, b, c, d, t);
  } else if (ti < N_CPTS - 1) {
    return de_casteljau_cubic(
      waypts[ti * 3 + 0],
      waypts[ti * 3 + 1],
      waypts[ti * 3 + 2],
      waypts[ti * 3 + 3],
      t
    );
  } else {
    int n = (N_CPTS - 1) * 3;
    quat a = waypts[n];
    quat b = quat_mul(quat_minorarc(waypts[n - 1], waypts[n - 0]), a);
    quat c = quat_mul(quat_minorarc(waypts[n - 2], waypts[n - 1]), b);
    quat d = quat_mul(quat_minorarc(waypts[n - 3], waypts[n - 2]), c);
    return de_casteljau_cubic(a, b, c, d, t);
  }
}

static inline void draw_image(int i, float entertime, float exittime)
{
  int n_coeffs = (imgs[i].order + 1) * (imgs[i].order + 2);
  state_uniform2f(st, "projCen", imgs[i].c_ra, imgs[i].c_dec);
  state_uniform1i(st, "ord", imgs[i].order);
  state_uniform2fv(st, "coeff", n_coeffs / 2, imgs[i].coeff);
  texture_bind(imgs[i].tex, 0);
  state_uniform1f(st, "seed", i);
  state_uniform1f(st, "entertime", entertime);
  state_uniform1f(st, "exittime", exittime);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  state_draw(st);
}

#define INTERVAL 150

static int T = -INTERVAL;
static int ins_ptr = 0;

static inline void _update_collage()
{
  T++;
  float t = (float)T / INTERVAL;
  view_ori = trace_at(t);

  while (ins_ptr < n_imgs && t > ins[ins_ptr].time) {
    int id = ins[ins_ptr].id;
    printf("%d\n", id);
    imgs[id].tex = texture_loadfile(imgs[id].img_path);
    ins_ptr++;
  }
}
void update_collage()
{
  for (int i = 1; i > 0; i--) _update_collage();
}

void draw_collage()
{
  glEnable(GL_BLEND);
  canvas_screen();

  // Frozen texture
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  state_uniform1f(st_cubetoscr, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st_cubetoscr, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  state_uniform1f(st_cubetoscr, "baseOpacity", 1);
  texture_bind(can_frozen.tex, 0);
  state_draw(st_cubetoscr);

  // Transparent buffer
  if (0) {
    state_uniform1f(st_cubetoscr, "baseOpacity", (float)T / INTERVAL);
    texture_bind(can_transp.tex, 0);
    state_draw(st_cubetoscr);
  }

  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  state_uniform1i(st, "transp", 0);
  for (int i = 0; i < ins_ptr; i++) {
    draw_image(ins[i].id, (float)T / INTERVAL - ins[i].time, -1);
  }
  glDisable(GL_BLEND);
}

// Sequence determination by genetic algorithm

// Beginning of xoshiro256starstar.c

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

static inline float eval_chro(uint8_t *chro)
{
  vec3 p = (vec3){1, 0, 0};
  vec3 a = (vec3){0, 0, -1};
  for (size_t i = 0; i < N_CPTS; i++) {
    int codon = (chro[i / 4] >> ((i % 4) * 2)) & 3;
    a = rot(a, p, ROTA_TILT * (codon * (2.f/3) - 1));
    p = rot(p, a, ROTA_STEP);
    trace[i] = p;
  }

  float sum = 0;
  for (int i = 0; i < n_imgs; i++) {
    float best = 2;
    for (int j = 0; j < N_CPTS; j++) {
      float dsq = vec3_distsq(imgpos[i], trace[j]);
      if (best > dsq) best = dsq;
    }
    sum += best;
  }
  return sum;
}

static inline void crossover(uint8_t *restrict a, uint8_t *restrict b, uint8_t *restrict c)
{
  unsigned x = rand_next() % (CHRO_LEN - 1);
  memcpy(c, a, x / 8);
  memcpy(c + (x / 8 + 1), b + (x / 8 + 1), CHRO_SZ - (x / 8 + 1));
  uint8_t a_mask = (2 << (x % 8)) - 1;
  c[x / 8] = (a[x / 8] & a_mask) | (b[x / 8] & (0xff ^ a_mask));
}

static inline void mut(uint8_t *a)
{
  for (size_t x = 0; x < CHRO_SZ; x++)
    for (int i = 0; i < 8; i++)
      if (rand_next() % CHRO_LEN == 0) a[x] ^= (1 << i);
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

static const char *hexdig = "0123456789abcdef";
static inline uint8_t hexval(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

static inline void evo_(bool does_evo)
{
  seq = (int *)malloc(sizeof(int) * n_imgs);
  for (int i = 0; i < n_imgs; i++) seq[i] = i;

  const int N_ROUNDS = 1000;
  const int N_POP = 10000;
  const int N_REP = 2000;
  #define size (sizeof(float) + sizeof(uint8_t) * CHRO_SZ)
  #define start(_i) (_pop + (_i) * size)
  #define chro(_i) ((uint8_t *)(_pop + (_i) * size + sizeof(float)))
  #define val(_i) (*(float *)(_pop + (_i) * size))
  char *_poppool = (char *)malloc((N_POP + N_REP) * size * 2);
  char *_pop = _poppool, *_npop = _poppool + (N_POP + N_REP) * size;

  char *sort_scratch = (char *)malloc(16 * (N_POP + N_REP));
  int *pmx_scratch = (int *)malloc(sizeof(int) * n_imgs);

  FILE *f_in = fopen("evo.txt", "r");
  bool loaded = false;
  if (f_in) {
    for (int i = 0; i < 4; i++) {
      s[i] = 0;
      for (int b = 0; b < 16; b++)
        s[i] = (s[i] << 4) | hexval(fgetc(f_in));
    }
    fgetc(f_in);
    for (int i = 0; i < N_POP; i++) {
      for (int j = 0; j < CHRO_SZ; j++) {
        uint8_t hi = hexval(fgetc(f_in));
        uint8_t lo = hexval(fgetc(f_in));
        chro(i)[j] = (hi << 4) | lo;
      }
      fgetc(f_in);
    }
    if (feof(f_in) || ferror(f_in)) {
      puts("Invalid previous result, starting from scratch");
    } else {
      loaded = true;
    }
    fclose(f_in);
  }
  if (loaded) {
    puts("Loaded previous result");
  } else {
    s[0] = 0x8ff333dac9f8d20bULL;
    s[1] = 0x1b2f7f552f1bca4eULL;
    s[2] = 0xefbc429aee43484eULL;
    s[3] = 0x8d600265e6b6a70dULL;
    for (int i = 0; i < N_POP; i++) {
      uint64_t *p = (uint64_t *)chro(i);
      for (int j = 0; j < CHRO_SZ / 8; j++) p[j] = rand_next();
    }
  }

  for (int i = 0; i < N_POP; i++) {
    val(i) = eval_chro(chro(i));
  }
  clock_t lastclk = clock();
  if (does_evo) for (int it = 0; it < N_ROUNDS; it++) {
    // Offsprings
    for (int r = 0; r < N_REP; r++) {
      int i = rand_next() % N_POP;
      int j = rand_next() % (N_POP - 1);
      if (i == j) j = N_POP - 1;
      crossover(chro(i), chro(j), chro(N_POP + r));
      mut(chro(N_POP + r));
      val(N_POP + r) = eval_chro(chro(N_POP + r));
    }
    sort_genomes(_pop, N_POP + N_REP, size, _npop, N_POP, sort_scratch);
    char *_t = _pop; _pop = _npop; _npop = _t;
    if ((it + 1) * 100 / N_ROUNDS != it * 100 / N_ROUNDS) {
      clock_t now = clock();
      printf("== It. %d ==  (%.8lf)\n", it + 1, (double)(now - lastclk) / CLOCKS_PER_SEC);
      for (int i = 0; i < 5; i++) printf("%9.5f\n", val(i));
      printf("%9.5f\n", val(N_POP - 1));
      lastclk = now;
    }
    if ((it + 1) * 10 / N_ROUNDS != it * 10 / N_ROUNDS) {
      // Save to file
      FILE *f_out = fopen("evo.txt", "w");
      for (int i = 0; i < 4; i++) {
        for (int b = 0; b < 16; b++)
          fputc(hexdig[(s[i] >> ((15 - b) * 4)) & 0xf], f_out);
      }
      fputc('\n', f_out);
      for (int i = 0; i < N_POP; i++) {
        for (int j = 0; j < CHRO_SZ; j++) {
          fputc(hexdig[chro(i)[j] >> 4], f_out);
          fputc(hexdig[chro(i)[j] & 0xf], f_out);
        }
        fputc('\n', f_out);
      }
      fclose(f_out);
    }
  }

  eval_chro(chro(0));
  for (int i = 0; i < N_CPTS; i++)
    printf("%c(%.5f,%.5f,%.5f)", i == 0 ? '{' : ',',
      trace[i].x, trace[i].y, trace[i].z);
  putchar('}');
  putchar('\n');

  free(sort_scratch);

  #undef start
  #undef chro
  free(_poppool);
}

void evo()
{
  load_collage_files();
  evo_(true);
}

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

static int *seq;
static inline void find_seq();

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

  // Order
  find_seq();
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
  state_uniform2f(st, "viewCoord", view_ra, view_dec);
  static int T = 0, start = 0;
  if (++T == 8) { start = (start + 1) % n_imgs; T = 0; }
  for (int _i = 0; _i < 10; _i++) {
    int i = seq[(_i + start) % n_imgs];
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

float *dist_scores = NULL;
static inline float eval_seq(int *seq)
{
  float score = 0;
  // for (int i = 0; i < n_imgs; i++) score += (n_imgs - i) * seq[i];
  for (int i = 0; i < n_imgs - 1; i++)
    score += dist_scores[seq[i] * n_imgs + seq[i + 1]];
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
  int t = a[l]; a[l] = a[r]; a[r] = t;
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

  // Scores between pairs
  dist_scores = (float *)realloc(dist_scores, sizeof(float) * n_imgs * n_imgs);
  for (int i = 0; i < n_imgs - 1; i++) {
    double xi = cos(imgs[i].c_dec) * cos(imgs[i].c_ra);
    double yi = cos(imgs[i].c_dec) * sin(imgs[i].c_ra);
    double zi = sin(imgs[i].c_dec);
    for (int j = i + 1; j < n_imgs; j++) {
      double xj = cos(imgs[j].c_dec) * cos(imgs[j].c_ra);
      double yj = cos(imgs[j].c_dec) * sin(imgs[j].c_ra);
      double zj = sin(imgs[j].c_dec);
      double angle = acos(xi * xj + yi * yj + zi * zj);
      float score = -angle;
      dist_scores[i * n_imgs + j] =
      dist_scores[j * n_imgs + i] = score;
    }
  }

  const int N_ROUNDS = 1000;
  const int N_POP = 300;
  const int N_REP = 200;
  char *_pop = (char *)malloc((N_POP + N_REP) * (sizeof(float) + sizeof(int) * n_imgs));
  #define perm(_i) ((int *)(_pop + (_i) * (sizeof(float) + sizeof(int) * n_imgs) + sizeof(float)))
  #define val(_i) (*(float *)(_pop + (_i) * (sizeof(float) + sizeof(int) * n_imgs)))

  pmx_map = (int *)realloc(pmx_map, sizeof(int) * n_imgs);

  for (int i = 0; i < N_POP; i++) {
    int *p = perm(i);
    for (int j = 0; j < n_imgs; j++) {
      int k = rand_next() % (j + 1);
      p[j] = p[k];
      p[k] = j;
    }
    val(i) = eval_seq(p);
  }
  for (int it = 0; it < N_ROUNDS; it++) {
    for (int r = 0; r < N_REP; r++) {
      int i = rand_next() % N_POP;
      int j = rand_next() % (N_POP - 1);
      if (i == j) j = N_POP - 1;
      pmx(perm(i), perm(j), perm(N_POP + r));
      mut(perm(N_POP + r));
      val(N_POP + r) = eval_seq(perm(N_POP + r));
    /*
      for (int k = 0; k < n_imgs; k++) printf("%3d", perm(i)[k]); putchar('\n');
      for (int k = 0; k < n_imgs; k++) printf("%3d", perm(j)[k]); putchar('\n');
      for (int k = 0; k < n_imgs; k++) printf("%3d", perm(N_POP + r)[k]); putchar('\n');
      puts("===");
    */
    }
    qsort(_pop, N_POP + N_REP, (sizeof(float) + sizeof(int) * n_imgs), genome_cmp);
    for (int i = 0; i < 5; i++) {
      printf("%9.0f |", val(i));
      for (int k = 0; k < n_imgs; k++) printf("%3d", perm(i)[k]); putchar('\n');
    }
    puts("===");
  }

  memcpy(seq, perm(0), sizeof(int) * n_imgs);

  #undef perm
  #undef val
  free(_pop);
}

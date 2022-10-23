#include "main.h"

#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mcmf.h"

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

#define INTERVAL 150
#define LEAD 0.6
#define OUT_START_DELAY 3
#define TRANSP_IN_DUR 48
#define TRANSP_OUT_DUR 1440

static int T = -INTERVAL * 3;
static int ins_ptr = 0;

static int *out_start;
static int out_ptr = 0, del_ptr = 0;

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
      right = vec3_add(right, sph_tan(trace[i], trace[i + 1], 0));
    waypts[i * 3] = rot_from_view(trace[i], vec3_normalize(right));
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

  // Best positions by bipartite b-matching
  ins = (fade_in_pt *)malloc(n_imgs * sizeof(fade_in_pt));

  const int n_steps = 200;
  const int n_divs = 12;
  vec3 *pts = (vec3 *)malloc(sizeof(vec3) * n_steps * n_divs);
  for (int i = 0; i < n_steps * n_divs; i++)
    pts[i] = rot_by_quat((vec3){0, 0, -1},
      trace_at((i + 0.5f) * (N_CPTS - 1) / (n_steps * n_divs)));

  mcmf_init(n_imgs + n_steps + 2);
  int mcmf_src = n_imgs + n_steps;
  int mcmf_sink = n_imgs + n_steps + 1;
  char *best_div_in_step = (char *)malloc(sizeof(char) * n_imgs * n_steps);
  for (int i = 0; i < n_imgs; i++) {
    mcmf_link(mcmf_src, i, 1, 0);
    for (int j = 0; j < n_steps; j++) {
      float best = 2;
      char bestat;
      for (char k = 0; k < n_divs; k++) {
        float d = vec3_distsq(pts[j * n_divs + k], imgpos[i]);
        if (best > d) { best = d; bestat = k; }
      }
      best_div_in_step[i * n_steps + j] = bestat;
      mcmf_link(i, j + n_imgs, 1, best);
    }
  }
  for (int j = 0; j < n_steps; j++) {
    mcmf_link(j + n_imgs, mcmf_sink, 1, 0);
    mcmf_link(j + n_imgs, mcmf_sink, 2, 0.05f);
  }
  int flowamt = mcmf_solve(mcmf_src, mcmf_sink);
  assert(flowamt == n_imgs);

  for (int i = 0; i < n_imgs; i++) {
    for (int x = mcmf_e_start[i]; x != -1; x = mcmf_e[x].next) {
      if (mcmf_e[x ^ 1].cap > 0) {
        int j = mcmf_e[x].dest - n_imgs;
        ins[i].id = i;
        ins[i].time =
          (j * n_divs + best_div_in_step[i * n_steps + j] + 0.5f)
          * (N_CPTS - 1) / (n_steps * n_divs);
        break;
      }
    }
  }

  qsort(ins, n_imgs, sizeof(fade_in_pt), cmp_fade_in_pt);
  for (int it = 0; it < 1000; it++) {
    for (int i = 0; i < n_imgs; i++) {
      float d = (i == n_imgs - 1 ? N_CPTS - 1 : ins[i + 1].time) - ins[i].time;
      if (d < 0.9) ins[i].time += (d - 0.9) * 0.01;
      if (d > 1.5) ins[i].time += (d - 1.5) * 0.003;
    }
    for (int i = n_imgs - 1; i >= 0; i--) {
      float d = ins[i].time - (i == 0 ? 0 : ins[i - 1].time);
      if (d < 0.9) ins[i].time -= (d - 0.9) * 0.01;
      if (d > 1.5) ins[i].time -= (d - 1.5) * 0.003;
    }
  }
/*
  for (int i = 0; i < n_imgs; i++)
    printf("%8.5f %3d (%.5f,%.5f,%.5f)\n", ins[i].time, ins[i].id,
      imgpos[ins[i].id].x, imgpos[ins[i].id].y, imgpos[ins[i].id].z);
*/

  out_start = (int *)malloc(sizeof(int) * n_imgs);
  memset(out_start, -1, sizeof(int) * n_imgs);
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
  const float k = 1.25;
  if (t < 0) {
    t = -t;
    t = 2 * (sqrtf(k * (t + k)) - k);
    return quat_mul(quat_minorarc(trace_at(t), waypts[0]), waypts[0]);
  } else if (t <= N_CPTS - 1) {
    int ti = (int)floorf(t);
    if (ti >= N_CPTS - 1) ti = N_CPTS - 2;
    t -= ti;
    return de_casteljau_cubic(
      waypts[ti * 3 + 0],
      waypts[ti * 3 + 1],
      waypts[ti * 3 + 2],
      waypts[ti * 3 + 3],
      t
    );
  } else {
    t = t - (N_CPTS - 1);
    t = 2 * (sqrtf(k * (t + k)) - k);
    int n = (N_CPTS - 1) * 3;
    return quat_mul(quat_minorarc(trace_at((N_CPTS - 1) - t), waypts[n]), waypts[n]);
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

void update_collage()
{
  T++;
  float t = (float)T / INTERVAL;
  view_ori = trace_at(t);
  if (T >= INTERVAL * (N_CPTS + 10)) global_fade_out = true;

  while (ins_ptr < n_imgs && t > ins[ins_ptr].time - LEAD) {
    int id = ins[ins_ptr].id;
    imgs[id].tex = texture_loadfile(imgs[id].img_path);
    ins_ptr++;
    // printf("%d/%d\n", ins_ptr, (int)n_imgs);
  }

  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  if (out_start[out_ptr] >= 0) {
    if (T == out_start[out_ptr] + TRANSP_IN_DUR) {
      // Blit previous transparent layer to frozen
      canvas_bind(can_frozen);
      texture_bind(can_transp.tex, 0);
      state_draw(st_cubetocube);
      // Clear transparent layer
      canvas_clear(can_transp);
      out_ptr++;
    }
  }
  if (out_ptr < n_imgs && out_start[out_ptr] == -1 &&
      t > ins[out_ptr].time + OUT_START_DELAY) {
    int id = ins[out_ptr].id;
    // Draw to transparent layer
    canvas_bind(can_transp);
    state_uniform1i(st, "transp", 1);
    draw_image(ins[out_ptr].id, 999, -999);
    out_start[out_ptr] = T;
  }
  canvas_screen();

  while (del_ptr < out_ptr &&
      T >= out_start[del_ptr] + TRANSP_IN_DUR + TRANSP_OUT_DUR) {
    texture_del(imgs[ins[del_ptr].id].tex);
    imgs[ins[del_ptr].id].tex = 0;
    del_ptr++;
  }
}

void draw_collage()
{
  canvas_screen();

  // Frozen texture
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  state_uniform1f(st_cubetoscr, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st_cubetoscr, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  state_uniform1f(st_cubetoscr, "baseOpacity", 1);
  texture_bind(can_frozen.tex, 0);
  state_draw(st_cubetoscr);

  // Transparent buffer
  if (out_start[out_ptr] > 0) {
    float a = (float)(T - out_start[out_ptr]) / TRANSP_IN_DUR;
    state_uniform1f(st_cubetoscr, "baseOpacity", a < 1 ? sqrtf(a) : 1);
    texture_bind(can_transp.tex, 0);
    state_draw(st_cubetoscr);
  }

  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  state_uniform1i(st, "transp", 0);
  for (int i = del_ptr; i < ins_ptr; i++) {
    draw_image(ins[i].id, (float)T / INTERVAL - ins[i].time + LEAD,
      out_start[i] == -1 ? -999 : (float)(T - out_start[i] - TRANSP_IN_DUR) / 240);
  }
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
/*
  for (int i = 0; i < N_CPTS; i++)
    printf("%c(%.5f,%.5f,%.5f)", i == 0 ? '{' : ',',
      trace[i].x, trace[i].y, trace[i].z);
  putchar('}');
  putchar('\n');
*/

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

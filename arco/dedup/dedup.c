#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct record {
  const char *name;
  uint64_t hash[3];
} record;
record *records = NULL;
size_t n_records = 0, cap_records = 0;

static inline void dct(int n, float *x, int stride, bool inverse)
{
  float y[n];
  if (!inverse) for (int i = 0; i < n; i++) {
    y[i] = 0;
    for (int j = 0; j < n; j++)
      y[i] += x[j * stride] * cosf((float)M_PI / n * i * (j + 0.5f));
  } else for (int i = 0; i < n; i++) {
    y[i] = x[0] / 2;
    for (int j = 1; j < n; j++)
      y[i] += x[j * stride] * cosf((float)M_PI / n * j * (i + 0.5f));
    y[i] *= (2.0f / n);
  }
  for (int i = 0; i < n; i++) x[i * stride] = y[i];
}

void process(const char *path)
{
  printf("Processing %s", path);
  // Read image (converted to greyscale by stbi__compute_y)
  int w, h;
  unsigned char *pix = stbi_load(path, &w, &h, NULL, 1);
  if (pix == NULL) {
    printf(" -- Cannot open! Ignoring > <\n");
    return;
  }
  printf(" (%dx%d)\n", w, h);
  // Scale image
  unsigned char pix_s1[32][32];
  stbir_resize_uint8_srgb(
    pix, w, h, 0,
    &pix_s1[0][0], 32, 32, 0,
    1, STBIR_ALPHA_CHANNEL_NONE, 0);
  stbi_image_free(pix);

  // Perceptual hash
  // https://www.hackerfactor.com/blog/index.php?/archives/432-Looks-Like-It.html
  uint64_t phash = 0;
  float pix_s1f[32][32];
  for (int r = 0; r < 32; r++)
    for (int c = 0; c < 32; c++)
      pix_s1f[r][c] = pix_s1[r][c];
  // 2D DCT-II
  for (int i = 0; i < 32; i++) dct(32, &pix_s1f[i][0], 1, false);
  for (int i = 0; i < 32; i++) dct(32, &pix_s1f[0][i], 32, false);
  // Reduction
  float average = 0;
  for (int r = 0; r < 8; r++)
    for (int c = !r; c < 8; c++)
      average += pix_s1f[r][c];
  average /= 63;
  for (int r = 0; r < 8; r++)
    for (int c = !r; c < 8; c++)
      phash |= ((uint64_t)(pix_s1f[r][c] >= average) << (r * 8 + c));
  phash |= (pix_s1f[0][0] >= 127.5);

  // Difference hash
  unsigned char pix_s2[8][8];
  stbir_resize_uint8_srgb(
    pix, w, h, 0,
    &pix_s2[0][0], 8, 8, 0,
    1, STBIR_ALPHA_CHANNEL_NONE, 0);
  uint64_t dhash1 = 0, dhash2 = 0;
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++) {
      dhash1 |= ((uint64_t)
        (pix_s2[r][c] > pix_s2[r][(c + 1) % 8])) << (r * 8 + c);
      dhash2 |= ((uint64_t)
        (pix_s2[r][c] > pix_s2[(r + 1) % 8][c])) << (r * 8 + c);
    }

  // printf("%016llx %016llx %016llx\n", phash, dhash1, dhash2);
  // Append to records list
  if (n_records >= cap_records) {
    cap_records = (cap_records == 0 ? 32 : (cap_records * 2));
    records = (record *)realloc(records, sizeof(record) * cap_records);
  }
  records[n_records].name = strdup(path);
  records[n_records].hash[0] = phash;
  records[n_records].hash[1] = dhash1;
  records[n_records].hash[2] = dhash2;
  n_records++;
}

#define N_PROJS 300
#define DIST_LIMIT 30
#define RANGE (DIST_LIMIT / 2)
typedef struct proj {
  size_t record_id;
  float val;
} proj;
proj *projs[N_PROJS];
size_t *proj_recpos[N_PROJS];

int proj_cmp(const void *_a, const void *_b)
{
  proj *a = (proj *)_a;
  proj *b = (proj *)_b;
  return (a->val < b->val ? -1 : a->val > b->val ? 1 : 0);
}

size_t projs_binsearch(const proj *p, float val)
{
  ssize_t l = -1, r = n_records;
  while (l < r - 1) {
    size_t m = (l + r) / 2;
    if (p[m].val >= val) r = m; else l = m;
  }
  return r;
}

static inline float randuniform()
{
  return (float)rand() / RAND_MAX;
}
static inline float randnorm()
{
  float u = randuniform();
  float v = randuniform();
  return sqrtf(-2 * logf(u)) * cosf((float)M_PI * 2 * v);
}

void find_dup()
{
  srand(10);
  for (size_t proj_id = 0; proj_id < N_PROJS; proj_id++) {
    proj *p = (proj *)malloc(sizeof(proj) * n_records);
    size_t *ppos = (size_t *)malloc(sizeof(size_t) * n_records);
    projs[proj_id] = p;
    proj_recpos[proj_id] = ppos;
    // Projection
    float wght[192];
    for (int i = 0; i < 192; i++) wght[i] = randnorm();
    for (size_t i = 0; i < n_records; i++) {
      float val = 0;
      for (int d = 0; d < 192; d++) {
        int bit = (records[i].hash[d / 64] >> (d % 64)) & 1;
        val += (bit ? wght[d] : 0);
      }
      p[i].record_id = i;
      p[i].val = val;
    }
    qsort(p, n_records, sizeof(proj), proj_cmp);
    for (size_t i = 0; i < n_records; i++)
      ppos[p[i].record_id] = i;
  }
  // Find duplicates
  for (size_t i = 0; i < n_records; i++) {
    // Find a project with the minimum number of candidates
    size_t min_cand = n_records + 1;
    size_t min_proj_id, min_jl, min_jr;
    for (size_t proj_id = 0; proj_id < N_PROJS; proj_id++) {
      size_t j = proj_recpos[proj_id][i];
      float val = projs[proj_id][j].val;
      size_t jl = projs_binsearch(projs[proj_id], val - RANGE);
      size_t jr = projs_binsearch(projs[proj_id], val + RANGE);
      if (min_cand > jr - jl) {
        min_cand = jr - jl;
        min_proj_id = proj_id;
        min_jl = jl;
        min_jr = jr;
      }
    }
    for (size_t j = min_jl; j < min_jr; j++)
      if (j != proj_recpos[min_proj_id][i]) {
        size_t k = projs[min_proj_id][j].record_id;
        if (i > k) continue;
        int dist =
          __builtin_popcountll(records[i].hash[0] ^ records[k].hash[0]) +
          __builtin_popcountll(records[i].hash[1] ^ records[k].hash[1]) +
          __builtin_popcountll(records[i].hash[2] ^ records[k].hash[2]);
        if (dist <= DIST_LIMIT)
          printf("%2d -- %s %s\n", dist, records[i].name, records[k].name);
      }
  }
}

int main(int argc, char *argv[])
{
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      printf("(%d/%d) ", i, argc - 1);
      process(argv[i]);
    }
  } else {
    char path[1024];
    while (fgets(path, sizeof path, stdin) != NULL) {
      size_t len = strlen(path);
      while (len > 0 && isspace(path[len - 1])) len--;
      path[len] = '\0';
      process(path);
    }
  }

  find_dup();

  return 0;
}

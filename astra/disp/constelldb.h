#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define N_HIPPARCOS 120405
static struct hip_record {
  double ra, dec;
  vec3 pos3;
} hip[N_HIPPARCOS];

#define N_CONSTELL 88
#define N_LINES_TOTAL 676
static int _constell_lines[N_LINES_TOTAL * 2];
static struct constell {
  char short_name[3];
  int n_lines;
  int *pts;
} cons[N_CONSTELL];
static int n_constell;

static void load_constelldb(const char *hip_path, const char *lines_path)
{
  FILE *fp;

  // Load HIP catalogue
  fp = fopen(hip_path, "r");
  assert(fp != NULL);
  int id;
  double ra, dec;
  while (fscanf(fp, "%d%lf%lf", &id, &ra, &dec) == 3) {
    ra *= M_PI/180;
    dec *= M_PI/180;
    hip[id].ra = ra;
    hip[id].dec = dec;
    hip[id].pos3 = (vec3){
      cos(dec) * cos(ra),
      cos(dec) * sin(ra),
      sin(dec)
    };
  }
  fclose(fp);

  // Load constellation lines
  fp = fopen(lines_path, "r");
  assert(fp != NULL);

  int lines_ptr = 0;
  n_constell = 0;
  char short_name[8];
  int n_lines;
  while (fscanf(fp, "%s%d", short_name, &n_lines) == 2) {
    memcpy(cons[n_constell].short_name, short_name, 3);
    cons[n_constell].n_lines = n_lines;
    cons[n_constell].pts = &_constell_lines[lines_ptr];
    for (int i = 0; i < n_lines; i++)
      fscanf(fp, "%d%d",
        &cons[n_constell].pts[i * 2 + 0],
        &cons[n_constell].pts[i * 2 + 1]);
    lines_ptr += n_lines * 2;
    n_constell++;
  }
  assert(n_constell <= N_CONSTELL && lines_ptr <= N_LINES_TOTAL * 2);
  // printf("%d %d\n", n_constell, lines_ptr);

  fclose(fp);
}

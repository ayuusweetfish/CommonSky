#include "main.h"

#include <assert.h>
#include <stdio.h>

static draw_state st;

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

static inline vec3 sph_tan(vec3 a, vec3 b, float t)
{
  float o = acosf(vec3_dot(a, b));
  // Derivative of slerp
  vec3 d = vec3_add(
    vec3_mul(a, -o * cosf((1 - t) * o) / sinf(o)),
    vec3_mul(b, o * cosf(t * o) / sinf(o))
  );
  vec3 r = vec3_slerp(a, b, t);
  return vec3_normalize(vec3_cross(d, r));
}

static int draw_line(vec3 p0, vec3 p1, float *a)
{
  float o = acosf(vec3_dot(p0, p1));
  int N = (o < 0.1 ? 2 : o < 0.2 ? 3 : 4);
  int n = 0;
  for (int i = 0; i < N; i++) {
    vec3 s[6];
    vec3 q1 = vec3_slerp(p0, p1, (float)i / N);
    vec3 q2 = vec3_slerp(p0, p1, (float)(i + 1) / N);
    vec3 t1 = vec3_mul(sph_tan(p0, p1, (float)i / N), 1e-3);
    vec3 t2 = vec3_mul(sph_tan(p0, p1, (float)(i + 1) / N), 1e-3);
    s[0] = vec3_diff(q1, t1);
    s[1] = vec3_diff(q2, t2);
    s[2] = vec3_add(q1, t1);
    s[3] = vec3_add(q2, t2);
    s[4] = s[2];
    s[5] = s[1];
    for (int j = 0; j < 6; j++) {
      a[n++] = s[j].x;
      a[n++] = s[j].y;
      a[n++] = s[j].z;
    }
  }
  return n;
}

void setup_constell()
{
  st = state_new();
  state_shader_files(&st, "constellline.vert", "constellline.frag");

  FILE *fp;

  // Load HIP catalogue
  fp = fopen("constell/hip2_j2000.dat", "r");
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
  fp = fopen("constell/constellationship.fab", "r");
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

  // Upload lines to the buffer
  st.stride = 3;
  state_attr(st, 0, 0, 3);

  static float verts[32768];

  int verts_bufsz = 0;
  for (int i = 0; i < n_constell; i++) {
    for (int j = 0; j < cons[i].n_lines; j++)
      verts_bufsz += draw_line(
        hip[cons[i].pts[j * 2 + 0]].pos3,
        hip[cons[i].pts[j * 2 + 1]].pos3,
        verts + verts_bufsz);
  }
  // printf("%d\n", verts_bufsz);

  state_buffer(&st, verts_bufsz / 3, verts);
}

void draw_constell()
{
  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform2f(st, "viewCoord", view_ra, view_dec);
  glEnable(GL_CULL_FACE);
  state_draw(st);
  glDisable(GL_CULL_FACE);
}

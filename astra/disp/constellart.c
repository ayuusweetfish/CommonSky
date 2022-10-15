#include "main.h"

#include <assert.h>
#include <stdio.h>

static draw_state st;

#include "constelldb.h"

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

  // Load constellation database
  load_constelldb("constell/hip2_j2000.dat", "constell/constellationship.fab");

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
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  glEnable(GL_CULL_FACE);
  state_draw(st);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
}

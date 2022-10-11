#include "main.h"

static draw_state st;

void setup_constell()
{
  st = state_new();
  state_shader_files(&st, "constellline.vert", "constellline.frag");
}

static inline vec3 sph_tan(vec3 a, vec3 b, float t)
{
  float o = acosf(vec3_dot(a, b));
  // Derivative of slerp
  vec3 d = vec3_add(
    vec3_mul(a, -o * cosf((1 - t) * o) / sinf(o)),
    vec3_mul(b, o * cosf(t * o) / sinf(o))
  );
  vec3 r = vec3_slerp(a, b, t);
  return vec3_cross(d, r);
  //return vec3_normalize(vec3_diff(b, a));
}

void draw_constell()
{
  vec3 p[2] = {{0, 1, 0.7}, {1, 0, 0}};
  for (int i = 0; i < 2; i++) p[i] = vec3_normalize(p[i]);

  const int N = 30;
  st.stride = 2;
  state_attr(st, 0, 0, 2);
  float verts[N][6][2];
  for (int i = 0; i < N; i++) {
    vec2 s[6];
    vec3 q1 = vec3_slerp(p[0], p[1], (float)i / (N - 1));
    vec3 q2 = vec3_slerp(p[0], p[1], (float)(i + 1) / (N - 1));
    vec3 t1 = vec3_mul(sph_tan(p[0], p[1], (float)i / (N - 1)), 0.01);
    vec3 t2 = vec3_mul(sph_tan(p[0], p[1], (float)(i + 1) / (N - 1)), 0.01);
    s[0] = scr_pos(vec3_diff(q1, t1));
    s[1] = scr_pos(vec3_diff(q2, t2));
    s[2] = scr_pos(vec3_add(q1, t1));
    s[3] = scr_pos(vec3_add(q2, t2));
    s[4] = s[2];
    s[5] = s[1];
    for (int j = 0; j < 6; j++) {
      verts[i][j][0] = s[j].x;
      verts[i][j][1] = s[j].y;
    }
  }
  state_buffer(&st, N * 6, &verts[0][0][0]);

  state_draw(st);
}

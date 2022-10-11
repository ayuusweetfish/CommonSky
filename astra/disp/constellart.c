#include "main.h"

static draw_state st;

void setup_constell()
{
  st = state_new();
  state_shader_files(&st, "constellline.vert", "constellline.frag");
}

void draw_constell()
{
  vec3 p[2] = {{0, 1, 0.7}, {1, 0, 0}};
  for (int i = 0; i < 2; i++) p[i] = vec3_normalize(p[i]);

  const int N = 30;
  st.stride = 2;
  state_attr(st, 0, 0, 2);
  float verts[N][3][2];
  for (int i = 0; i < N; i++) {
    vec2 s[3];
    s[0] = scr_pos(vec3_slerp(p[0], p[1], (float)i / (N - 1)));
    s[1] = scr_pos(vec3_slerp(p[0], p[1], (float)(i + 1) / (N - 1)));
    s[2] = s[0]; s[2].y += 0.1;
    for (int j = 0; j < 3; j++) {
      verts[i][j][0] = s[j].x;
      verts[i][j][1] = s[j].y;
    }
  }
  state_buffer(&st, N * 3, &verts[0][0]);

  state_draw(st);
}

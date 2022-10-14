// gcc main.c constellart.c collage.c -Iglad glad.o -I../../arco/dedup stb.o -lglfw
#include "main.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int fb_w, fb_h;
double view_ra = 0, view_dec = 0;
quat view_ori = (quat){0, 0, 0, 1};

static GLFWwindow *window;

void fb_size_changed(GLFWwindow *window, int w, int h)
{
  fb_w = w;
  fb_h = h;
  glViewport(0, 0, w, h);
}

static draw_state st;
GLuint cubemap[6];

void setup()
{
  st = state_new();
  state_shader_files(&st, "1.vert", "1.frag");
  st.stride = 2;
  state_attr(st, 0, 0, 2);
  const float fullscreen_coords[12] = {
    -1.0, -1.0, -1.0,  1.0,  1.0,  1.0,
    -1.0, -1.0,  1.0, -1.0,  1.0,  1.0,
  };
  state_buffer(&st, 6, fullscreen_coords);

  for (int i = 0; i < 6; i++) {
    char s[64];
    sprintf(s, "cubemap/%d.png", i);
    cubemap[i] = texture_loadfile(s);
    sprintf(s, "cubemap[%d]", i);
    state_uniform1i(st, s, i);
  }

  setup_collage();
  setup_constell();
}

static bool cursor_capt = true;

static const float DEC_MAX = 88 * (M_PI/180);

void update()
{
  static double last_x = NAN, last_y;
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  double dx = (isnan(last_x) ? 0 : x - last_x);
  double dy = (isnan(last_x) ? 0 : y - last_y);
  last_x = x;
  last_y = y;
  if (!cursor_capt) dx = dy = 0;

  view_ra -= dx * 3.5e-3;
  view_ra = fmod(view_ra + M_PI*2, M_PI*2);
  view_dec -= dy * 3.5e-3;
  view_dec = (view_dec > DEC_MAX ? DEC_MAX :
    view_dec < -DEC_MAX ? -DEC_MAX : view_dec);
  // printf("%.4lf %.4lf\n", view_ra, view_dec);
  // Map from screen space to model space
  // (0, 0, -1) maps to p
  vec3 p = (vec3){
    cos(view_dec) * cos(view_ra),
    cos(view_dec) * sin(view_ra),
    sin(view_dec)
  };
  vec3 u = (vec3){0, 0, 1};
  // (0, 1, 0) maps to u
  u = vec3_normalize(vec3_diff(u, vec3_mul(p, vec3_dot(u, p))));
  // (1, 0, 0) maps to xto
  vec3 xto = vec3_cross(p, u);
  view_ori = rot_from_vecs(xto, u, vec3_mul(p, -1));
}

void draw()
{
  glClearColor(0.8, 0.8, 0.82, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_CULL_FACE);

  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  for (int i = 0; i < 6; i++) texture_bind(cubemap[i], i);
  state_draw(st);

  draw_collage();
  draw_constell();
}

int main(int argc, char *argv[])
{
  assert(glfwInit());

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

  window = glfwCreateWindow(800, 450, "Domus Astris", NULL, NULL);
  assert(window != NULL);

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  gladLoadGL();
  glGetError();   // Clear previous error

  glfwGetFramebufferSize(window, &fb_w, &fb_h);
  fb_size_changed(window, fb_w, fb_h);
  glfwSetFramebufferSizeCallback(window, fb_size_changed);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

  setup();
  assert_gl();

  bool cursor_k1 = false, cursor_k2 = false;

  double last_time = glfwGetTime();
  double cum_time = 0;

  while (1) {
    glfwSwapBuffers(window);
    glfwPollEvents();
    if (glfwWindowShouldClose(window)) break;

    bool k1 = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    if (!cursor_k1 && k1) cursor_capt = !cursor_capt;
    cursor_k1 = k1;
    bool k2 = (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    if (cursor_capt && !cursor_k2 && k2) cursor_capt = false;
    bool b1 = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    if (!cursor_capt && b1) cursor_capt = true;
    bool b2 = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    if (cursor_capt && b2) cursor_capt = false;
    glfwSetInputMode(window, GLFW_CURSOR,
      cursor_capt ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    double T = glfwGetTime();
    cum_time += (T - last_time);
    last_time = T;
    while (cum_time >= 0) {
      cum_time -= 1.0 / 240;
      update();
    }

    draw();
    assert_gl();
  }

  return 0;
}

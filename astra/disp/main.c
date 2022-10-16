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
static GLuint cubemap;

static draw_state st_overlay;
static canvas can_overlay;

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

  cubemap = texture_loadfile("cubemap.png");
  state_uniform1i(st, "cubemap", 0);

  // Overlay
  st_overlay = state_new();
  state_shader_files(&st_overlay, "2.vert", "2.frag");
  st_overlay.stride = 2;
  state_attr(st_overlay, 0, 0, 2);
  state_buffer(&st_overlay, 6, fullscreen_coords);
  can_overlay = canvas_new(3200, 2400);
  canvas_bind(can_overlay);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Premultiplied alpha
  state_draw(st_overlay);
  glDisable(GL_BLEND);
  canvas_screen();

  setup_collage();
  setup_constell();
}

static bool cursor_capt = false;

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

  update_collage();

  if (cursor_capt) {
    // Map from screen space to model space
    vec3 p = (vec3){
      cos(view_dec) * cos(view_ra),
      cos(view_dec) * sin(view_ra),
      sin(view_dec)
    };
    vec3 u = (vec3){0, 0, 1};
    u = vec3_normalize(vec3_diff(u, vec3_mul(p, vec3_dot(u, p))));
    view_ori = rot_from_view(p, vec3_cross(p, u));
  }
}

void draw()
{
  glClearColor(0.8, 0.8, 0.82, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_CULL_FACE);

  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  texture_bind(cubemap, 0);
  state_draw(st);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  texture_bind(can_overlay.tex, 0);
  // state_draw(st);
  glDisable(GL_BLEND);

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

  int w;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &w);
  printf("Maximum texture dimension is %d\n", w);
  assert(w >= 6144);

  glfwGetFramebufferSize(window, &fb_w, &fb_h);
  fb_size_changed(window, fb_w, fb_h);
  glfwSetFramebufferSizeCallback(window, fb_size_changed);
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

    bool k1 = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
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

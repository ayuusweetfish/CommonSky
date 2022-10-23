// gcc main.c constellart.c collage.c -Iglad glad.o -I../../arco/dedup stb.o -lglfw
#include "main.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define W 960
#define H 600

int fb_w, fb_h;
double view_ra = 0, view_dec = 0;
quat view_ori = (quat){0, 0, 0, 1};

bool global_fade_out = false;

static GLFWwindow *window;

void fb_size_changed(GLFWwindow *window, int w, int h)
{
  fb_w = w;
  fb_h = h;
  glViewport(0, 0, w, h);
}

static draw_state st;
static GLuint cubemap;

static draw_state st_cover;

void setup()
{
  const float fullscreen_coords[12] = {
    -1.0, -1.0, -1.0,  1.0,  1.0,  1.0,
    -1.0, -1.0,  1.0, -1.0,  1.0,  1.0,
  };

  st = state_new();
  state_shader_files(&st, "plain.vert", "cubemap.frag");
  st.stride = 2;
  state_attr(st, 0, 0, 2);
  state_buffer(&st, 6, fullscreen_coords);

  cubemap = texture_loadfile("cubemap.png");
  state_uniform1i(st, "cubemap", 0);

  st_cover = state_new();
  state_shader_files(&st_cover, "plain.vert", "cover.frag");
  st_cover.stride = 2;
  state_attr(st_cover, 0, 0, 2);
  state_buffer(&st_cover, 6, fullscreen_coords);

  setup_collage();
  setup_constell();
}

static bool cursor_capt = false;

static const float DEC_MAX = 88 * (M_PI/180);

static int T = 0;
static int since_global_fade_out = -1;

void update()
{
  T++;
  if (global_fade_out) since_global_fade_out++;

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
  glClearColor(0.09, 0.06, 0.12, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);

/*
  state_uniform1f(st, "aspectRatio", (float)fb_w / fb_h);
  state_uniform4f(st, "viewOri", view_ori.x, view_ori.y, view_ori.z, view_ori.w);
  state_uniform1f(st, "baseOpacity", 1);
  texture_bind(cubemap, 0);
  state_draw(st);
*/

  draw_collage();
  draw_constell();

  float alpha = 0;
  if (T < 240) {
    float x = (float)T / 240;
    alpha = (cosf(x * M_PI) + 1) / 2;
  } else if (since_global_fade_out >= 0) {
    float x = (float)(since_global_fade_out + 0.5f) / 240;
    if (x > 1) x = 1;
    alpha = (1 - cosf(x * M_PI)) / 2;
  }
  if (alpha > 0) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    state_uniform1f(st_cover, "alpha", alpha);
    state_draw(st_cover);
  }
}

int main(int argc, char *argv[])
{
  if (argc >= 2 && argv[1][0] == 'e') {
    evo();
    exit(0);
  }
  const char *record_dir = (argc >= 3 && argv[1][0] == 'r' ? argv[2] : NULL);

  assert(glfwInit());

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

  window = glfwCreateWindow(W, H, "Domus Astris", NULL, NULL);
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
  if (record_dir == NULL)
    glfwSetFramebufferSizeCallback(window, fb_size_changed);
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

  setup();
  assert_gl();

  bool cursor_k1 = false, cursor_k2 = false;

  double last_time = glfwGetTime();
  double cum_time = 0;

  char *record_filename = NULL;
  char *record_numptr = NULL;
  void *screen_pixels = NULL;
  if (record_dir != NULL) {
    size_t len = strlen(record_dir);
    record_filename = (char *)malloc(len + 20);
    strcpy(record_filename, record_dir);
    strcpy(record_filename + len, "/astra-??????.png");
    record_numptr = record_filename + len + 12;
    mkdir(record_dir, 0755);
    screen_pixels = malloc(W * H * 3);
  }

  while (1) {
    glfwSwapBuffers(window);
    glfwPollEvents();
    if (glfwWindowShouldClose(window)) break;

    if (record_dir == NULL) {
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
      float step = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ?
        1.0f / 2400 : 1.0f / 240;
      while (cum_time >= 0) {
        cum_time -= step;
        update();
      }
    } else {
      for (int i = 0; i < 8; i++) update();
    }

    draw();
    assert_gl();

    if (record_dir != NULL) {
      for (int i = 0, num = T / 8; i < 6; i++) {
        record_numptr[-i] = '0' + num % 10;
        num /= 10;
      }
      puts(record_filename);
      glFlush();
      glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, screen_pixels);
      stbi_write_png(record_filename, W, H, 3, screen_pixels, 0);
    }

    if (since_global_fade_out >= 360) break;
  }

  return 0;
}

#include "glad.h" // --profile="compatibility" --api="gl=3.2" --generator="c" --spec="gl" --local-files --extensions=""
#include "GLFW/glfw3.h"

#include "glwrap.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

GLFWwindow *window;

void fb_size_changed(GLFWwindow *window, int w, int h)
{
  glViewport(0, 0, w, h);
}

draw_state st;
GLuint cubemap[6];

static inline char *read_all(const char *path)
{
  FILE *f = fopen(path, "r");
  fseek(f, -1, SEEK_END);
  size_t len = (size_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc(len + 1);
  fread(buf, len, 1, f);
  fclose(f);
  buf[len] = '\0';
  return buf;
}

void setup()
{
  st = state_new();
  char *vs = read_all("1.vert");
  char *fs = read_all("1.frag");
  state_shader(&st, vs, fs);
  free(vs);
  free(fs);
  st.stride = 2;
  state_attr(st, 0, 0, 2);
  const float fullscreen_coords[12] = {
    -1.0, -1.0, -1.0,  1.0,  1.0,  1.0,
    -1.0, -1.0,  1.0, -1.0,  1.0,  1.0,
  };
  state_buffer(&st, 6, fullscreen_coords);

  for (int i = 0; i < 6; i++) {
    cubemap[i] = texture_new();
    char s[64];
    sprintf(s, "cubemap/%d.png", i);
    texture_loadfile(cubemap[i], s);
    sprintf(s, "cubemap[%d]", i);
    state_uniform1i(st, s, i);
  }
}

double view_ra = 0, view_dec = 0;

void update()
{
  static double last_x = NAN, last_y;
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  double dx = (isnan(last_x) ? 0 : x - last_x);
  double dy = (isnan(last_x) ? 0 : y - last_y);
  last_x = x;
  last_y = y;

  view_ra += dx * 0.2;
  view_ra = fmod(view_ra + 360, 360);
  view_dec -= dy * 0.2;
  view_dec = (view_dec > 88 ? 88 : view_dec < -88 ? -88 : view_dec);
  // printf("%.4lf %.4lf\n", view_ra, view_dec);
}

void draw()
{
  glClearColor(0.8, 0.8, 0.82, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  float ra = view_ra * (M_PI/180);
  float dec = view_dec * (M_PI/180);
  state_uniform2f(st, "viewCoord", ra, dec);
  for (int i = 0; i < 6; i++) texture_bind(cubemap[i], i);
  state_draw(st);
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

  glfwSetFramebufferSizeCallback(window, fb_size_changed);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

  setup();
  assert_gl();

  double last_time = glfwGetTime();
  double cum_time = 0;

  while (1) {
    glfwSwapBuffers(window);
    glfwPollEvents();
    if (glfwWindowShouldClose(window)) break;

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

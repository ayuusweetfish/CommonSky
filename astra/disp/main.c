#include "glad.h" // --profile="compatibility" --api="gl=3.2" --generator="c" --spec="gl" --local-files --extensions=""
#include "GLFW/glfw3.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define assert_gl() assert_gl_(__LINE__)
static inline void assert_gl_(int line)
{
  GLenum e = glGetError();
  if (e != 0) {
    printf("line %d: glGetError() = %d\n", line, (int)e);
    assert(false);
  }
}

GLFWwindow *window;

void fb_size_changed(GLFWwindow *window, int w, int h)
{
  glViewport(0, 0, w, h);
}

void setup()
{
}

void update()
{
  static double last_x = NAN, last_y;
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  float dx = (isnan(last_x) ? 0 : x - last_x);
  float dy = (isnan(last_x) ? 0 : y - last_y);
  last_x = x;
  last_y = y;
}

void draw()
{
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

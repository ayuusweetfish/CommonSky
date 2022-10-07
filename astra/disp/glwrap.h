#ifndef GLWRAP_H
#define GLWRAP_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define assert_gl() assert_gl_(__LINE__)
static inline void assert_gl_(int line)
{
  GLenum e = glGetError();
  if (e != 0) {
    printf("line %d: glGetError() = %d\n", line, (int)e);
    assert(false);
  }
}

// Shader program

static inline GLuint compile_shader(GLenum type, const char *src) {
  GLuint id = glCreateShader(type);
  GLint len = strlen(src);
  glShaderSource(id, 1, &src, &len);
  glCompileShader(id);
  char log[1024];
  glGetShaderInfoLog(id, sizeof(log), NULL, log);
  if (log[0] != '\0') {
    printf("shader compilation message:\n%s===\n", log);
  }
  return id;
}

static inline GLuint shader_program(const char *vert, const char *frag) {
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vert);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag);
  GLuint id = glCreateProgram();
  glAttachShader(id, vs);
  glAttachShader(id, fs);
  glLinkProgram(id);
  glDeleteShader(vs);
  glDeleteShader(fs);
  return id;
}

static inline GLuint uniform_loc(GLuint id, const char *name) {
  GLint location = glGetUniformLocation(id, name);
  if (location == -1) {
    printf("uniform %s not found\n", name);
    assert(location != -1);
  }
  return location;
}

// Draw state

typedef struct draw_state {
  GLuint vao, vbo;
  GLuint prog;
  int stride, elems;
} draw_state;

draw_state state_new() {
  draw_state s;
  glGenVertexArrays(1, &s.vao);
  glGenBuffers(1, &s.vbo);
  s.prog = -1;
  s.stride = s.elems = 0;
  return s;
}

void state_bind(const draw_state s) {
  glBindVertexArray(s.vao);
  glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
}

void state_shader(draw_state *s,
  const char *vs, const char *fs
) {
  // TODO: Resource management
  s->prog = shader_program(vs, fs);
}

void state_uniform2f(const draw_state s,
  const char *name, float v0, float v1
) {
  glUseProgram(s.prog);
  glUniform2f(uniform_loc(s.prog, name), v0, v1);
}

void state_uniform3f(const draw_state s,
  const char *name, float v0, float v1, float v2
) {
  glUseProgram(s.prog);
  glUniform3f(uniform_loc(s.prog, name), v0, v1, v2);
}

void state_attr(const draw_state s,
  int id, int offset, int size
) {
  state_bind(s);
  glEnableVertexAttribArray(id);
  glVertexAttribPointer(
    id, size, GL_FLOAT, GL_FALSE,
    s.stride * sizeof(float), (const void *)(offset * sizeof(float))
  );
}

void state_buffer(draw_state *s,
  int n, const void *buf
) {
  state_bind(*s);
  s->elems = n;
  glBufferData(
    GL_ARRAY_BUFFER,
    n * s->stride * sizeof(float),
    buf,
    GL_STREAM_DRAW
  );
}

void state_draw(const draw_state s) {
  state_bind(s);
  glUseProgram(s.prog);
  glDrawArrays(GL_TRIANGLES, 0, s.elems);
}

#endif

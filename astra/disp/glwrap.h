#ifndef GLWRAP_H
#define GLWRAP_H

#include "stb_image.h"

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

extern int fb_w, fb_h;

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

// Texture

static inline void texture_bind(GLuint id, int unit) {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id);
}

static inline GLuint texture_new() {
  GLuint id;
  glGenTextures(1, &id);
  texture_bind(id, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
  return id;
}

static inline void texture_update(GLuint id, int w, int h, void *pix) {
  texture_bind(id, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
}

static inline GLuint texture_loadfile(const char *path) {
  int w, h;
  unsigned char *pix = stbi_load(path, &w, &h, NULL, 4);
  GLuint id = texture_new();
  texture_bind(id, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  stbi_image_free(pix);
  return id;
}

// Render target

typedef struct canvas {
  GLuint fb, tex;
  int w, h;
} canvas;

static inline canvas canvas_new(int w, int h) {
  GLuint fb, tex;

  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
  GLenum db = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &db);

  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  return (canvas){fb, tex, w, h};
}

static inline void canvas_bind(const canvas c) {
  glBindFramebuffer(GL_FRAMEBUFFER, c.fb);
  glViewport(0, 0, c.w, c.h);
}

static inline void canvas_screen() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fb_w, fb_h);
}

// Draw state

typedef struct draw_state {
  GLuint vao, vbo;
  GLuint prog;
  int stride, elems;
} draw_state;

static inline draw_state state_new() {
  draw_state s;
  glGenVertexArrays(1, &s.vao);
  glGenBuffers(1, &s.vbo);
  s.prog = -1;
  s.stride = s.elems = 0;
  return s;
}

static inline void state_bind(const draw_state s) {
  glBindVertexArray(s.vao);
  glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
}

static inline void state_shader(draw_state *s,
  const char *vs, const char *fs
) {
  // TODO: Resource management
  s->prog = shader_program(vs, fs);
}

static inline void state_uniform1f(const draw_state s,
  const char *name, float v0
) {
  glUseProgram(s.prog);
  glUniform1f(uniform_loc(s.prog, name), v0);
}

static inline void state_uniform2fv(const draw_state s,
  const char *name, int n, float *v
) {
  glUseProgram(s.prog);
  glUniform2fv(uniform_loc(s.prog, name), n, v);
}

static inline void state_uniform2f(const draw_state s,
  const char *name, float v0, float v1
) {
  glUseProgram(s.prog);
  glUniform2f(uniform_loc(s.prog, name), v0, v1);
}

static inline void state_uniform3f(const draw_state s,
  const char *name, float v0, float v1, float v2
) {
  glUseProgram(s.prog);
  glUniform3f(uniform_loc(s.prog, name), v0, v1, v2);
}

static inline void state_uniform4f(const draw_state s,
  const char *name, float v0, float v1, float v2, float v3
) {
  glUseProgram(s.prog);
  glUniform4f(uniform_loc(s.prog, name), v0, v1, v2, v3);
}

static inline void state_uniform1i(const draw_state s,
  const char *name, int v0
) {
  glUseProgram(s.prog);
  glUniform1i(uniform_loc(s.prog, name), v0);
}

static inline void state_uniform4x4f(const draw_state s,
  const char *name, const float *v
) {
  glUseProgram(s.prog);
  glUniformMatrix4fv(uniform_loc(s.prog, name), 1, GL_FALSE, v);
}

static inline void state_attr(const draw_state s,
  int id, int offset, int size
) {
  state_bind(s);
  glEnableVertexAttribArray(id);
  glVertexAttribPointer(
    id, size, GL_FLOAT, GL_FALSE,
    s.stride * sizeof(float), (const void *)(offset * sizeof(float))
  );
}

static inline void state_buffer(draw_state *s,
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

static inline void state_draw(const draw_state s) {
  state_bind(s);
  glUseProgram(s.prog);
  glDrawArrays(GL_TRIANGLES, 0, s.elems);
}

#endif

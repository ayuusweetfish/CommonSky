#pragma once

#include "glad.h" // --profile="compatibility" --api="gl=3.2" --generator="c" --spec="gl" --local-files --extensions=""
#include "GLFW/glfw3.h"

#include "glwrap.h"

#include <math.h>

extern int fb_w, fb_h;
extern double view_ra, view_dec;

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

static inline void state_shader_files( 
  draw_state *st, const char *vspath, const char *fspath)
{
  char *vs = read_all(vspath);
  char *fs = read_all(fspath);
  state_shader(st, vs, fs);
  free(vs);
  free(fs);
}

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } quat;
static inline float vec3_dot(vec3 a, vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline vec3 vec3_lerp(vec3 a, vec3 b, float t) {
  return (vec3){
    a.x + (b.x - a.x) * t,
    a.y + (b.y - a.y) * t,
    a.z + (b.z - a.z) * t
  };
}
static inline vec3 vec3_slerp(vec3 a, vec3 b, float t) {
  float o = acosf(vec3_dot(a, b));
  float w1 = sinf((1 - t) * o) / sinf(o);
  float w2 = sinf(t * o) / sinf(o);
  return (vec3){
    a.x * w1 + b.x * w2,
    a.y * w1 + b.y * w2,
    a.z * w1 + b.z * w2
  };
}
static inline vec3 vec3_normalize(vec3 a) {
  float norm = sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
  return (vec3){a.x / norm, a.y / norm, a.z / norm};
}
static inline quat quat_mul(quat p, quat q) {
  return (quat){
    p.w * q.x + q.w * p.x + (p.y * q.z - q.y * p.z),
    p.w * q.y + q.w * p.y - (p.x * q.z - q.x * p.z),
    p.w * q.z + q.w * p.z + (p.x * q.y - q.x * p.y),
    p.w * q.w - (p.x * q.x + p.y * q.y + p.z * q.z)
  };
}
static inline vec3 rot(vec3 v, vec3 axis, float angle) {
  quat q = (quat){
    axis.x * sinf(angle / 2),
    axis.y * sinf(angle / 2),
    axis.z * sinf(angle / 2),
    cosf(angle / 2)
  };
  quat r = quat_mul(quat_mul(q, (quat){v.x, v.y, v.z, 0}),
    (quat){q.x, q.y, q.z, -q.w});
  return (vec3){r.x, r.y, r.z};
}

static inline vec2 scr_pos(vec3 p) {
  p = rot(p, (vec3){0, 0, 1}, -(view_ra - M_PI / 2));
  p = rot(p, (vec3){1, 0, 0}, -(view_dec + M_PI / 2));
  float px = p.x / (1 - p.z);
  float py = p.y / (1 - p.z);
  const float projCircleR = 3;  // TODO: Sync
  float aspectRatio = (float)fb_w / fb_h;
  px *= projCircleR;
  py *= (projCircleR * aspectRatio);
  return (vec2){px, py};
}

// constellart.c

void setup_constell();
void draw_constell();

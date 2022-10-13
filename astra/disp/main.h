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
static inline vec3 vec3_add(vec3 a, vec3 b) {
  return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}
static inline vec3 vec3_diff(vec3 a, vec3 b) {
  return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
static inline vec3 vec3_mul(vec3 a, float k) {
  return (vec3){a.x * k, a.y * k, a.z * k};
}
static inline float vec3_dot(vec3 a, vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline float vec3_norm(vec3 a) { return vec3_dot(a, a); }
static inline float vec3_dist(vec3 a, vec3 b) {
  return vec3_norm(vec3_diff(a, b));
}
static inline vec3 vec3_cross(vec3 a, vec3 b) {
  return (vec3){
    +(a.y * b.z - b.y * a.z),
    -(a.x * b.z - b.x * a.z),
    +(a.x * b.y - b.x * a.y),
  };
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
  return vec3_add(
    vec3_mul(a, sinf((1 - t) * o) / sinf(o)),
    vec3_mul(b, sinf(t * o) / sinf(o))
  );
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

// collage.c

void setup_collage();
void draw_collage();

// constellart.c

void setup_constell();
void draw_constell();

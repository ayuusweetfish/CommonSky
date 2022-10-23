#pragma once

#include "glad.h" // --profile="compatibility" --api="gl=3.2" --generator="c" --spec="gl" --local-files --extensions=""
#include "GLFW/glfw3.h"

#include "glwrap.h"

#include "stb_image_write.h"

#include <math.h>

extern int fb_w, fb_h;
extern double view_ra, view_dec;

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } quat;
extern quat view_ori;

extern bool global_fade_out;

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
static inline float vec3_norm(vec3 a) { return sqrtf(vec3_dot(a, a)); }
static inline float vec3_dist(vec3 a, vec3 b) {
  return vec3_norm(vec3_diff(a, b));
}
static inline float vec3_distsq(vec3 a, vec3 b) {
  return vec3_dot(vec3_diff(a, b), vec3_diff(a, b));
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
static inline quat quat_inv(quat q) {
  return (quat){-q.x, -q.y, -q.z, q.w};
}
static inline quat quat_scale(quat q, float k) {
  return (quat){k*q.x, k*q.y, k*q.z, k*q.w};
}
static inline float quat_norm(quat q) {
  return sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
}
static inline quat quat_normalize(quat q) {
  return quat_scale(q, 1.f / quat_norm(q));
}
static inline quat quat_add(quat p, quat q) {
  return (quat){p.x+q.x, p.y+q.y, p.z+q.z, p.w+q.w};
}
static inline quat quat_mul(quat p, quat q) {
  return (quat){
    p.w * q.x + q.w * p.x + (p.y * q.z - q.y * p.z),
    p.w * q.y + q.w * p.y - (p.x * q.z - q.x * p.z),
    p.w * q.z + q.w * p.z + (p.x * q.y - q.x * p.y),
    p.w * q.w - (p.x * q.x + p.y * q.y + p.z * q.z)
  };
}
static inline vec3 rot_by_quat(vec3 v, quat q) {
  quat r = quat_mul(quat_mul(q, (quat){v.x, v.y, v.z, 0}), quat_inv(q));
  return (vec3){r.x, r.y, r.z};
}
static inline vec3 rot(vec3 v, vec3 axis, float angle) {
  quat q = (quat){
    axis.x * sinf(angle / 2),
    axis.y * sinf(angle / 2),
    axis.z * sinf(angle / 2),
    cosf(angle / 2)
  };
  return rot_by_quat(v, q);
}
static inline quat rot_from_vecs(vec3 xto, vec3 yto, vec3 zto) {
  float M[3][3] = {
    {xto.x, yto.x, zto.x},
    {xto.y, yto.y, zto.y},
    {xto.z, yto.z, zto.z},
  };
  // Largest diagonal entry
  float maxd = -2;
  int maxdi;
  for (int i = 0; i < 3; i++)
    if (maxd < M[i][i]) { maxd = M[i][i]; maxdi = i; }
  float r = sqrtf(1 - M[0][0] - M[1][1] - M[2][2] + maxd * 2);
  float w, x, y, z;
  if (maxdi == 0) {
    w = (M[2][1] - M[1][2]) / (r * 2);
    x = r / 2;
    y = (M[0][1] + M[1][0]) / (r * 2);
    z = (M[0][2] + M[2][0]) / (r * 2);
  } else if (maxdi == 1) {
    w = (M[0][2] - M[2][0]) / (r * 2);
    x = (M[1][0] + M[0][1]) / (r * 2);
    y = r / 2;
    z = (M[1][2] + M[2][1]) / (r * 2);
  } else {
    w = (M[1][0] - M[0][1]) / (r * 2);
    x = (M[2][0] + M[0][2]) / (r * 2);
    y = (M[2][1] + M[1][2]) / (r * 2);
    z = r / 2;
  }
  return (quat){x, y, z, w};
}
static inline quat rot_from_view(vec3 lookat, vec3 right) {
  return rot_from_vecs(
    right,
    vec3_cross(right, lookat),
    vec3_mul(lookat, -1)
  );
}
static inline quat quat_exp(quat q) {
  float e = expf(q.w);
  vec3 v = (vec3){q.x, q.y, q.z};
  float vnorm = vec3_norm(v);
  float w = e * cosf(vnorm);
  v = vec3_mul(v, e * (fabsf(vnorm) < 1e-4 ? 1 : sinf(vnorm) / vnorm));
  return (quat){v.x, v.y, v.z, w};
}
static inline quat quat_log(quat q) {
  // Caveat: does not handle pure quaternions
  float qnorm = quat_norm(q);
  vec3 v = (vec3){q.x, q.y, q.z};
  float vnorm = vec3_norm(v);
  float w = logf(qnorm);
  v = vec3_mul(v, acosf(q.w / qnorm) / vnorm);
  return (quat){v.x, v.y, v.z, w};
}
static inline quat quat_pow(quat q, float t) {
  quat l = quat_log(q);
  return quat_exp(quat_scale(l, t));
}
static inline quat quat_minorarc(quat from, quat to) {
  quat q = quat_mul(to, quat_inv(from));
  return q.w < 0 ? quat_scale(q, -1) : q;
}
static inline quat quat_slerp(quat a, quat b, float t) {
  return quat_mul(quat_pow(quat_minorarc(a, b), t), a);
}

static inline vec3 sph_tan(vec3 a, vec3 b, float t)
{
  float o = acosf(vec3_dot(a, b));
  // Derivative of slerp
  vec3 d = vec3_add(
    vec3_mul(a, -o * cosf((1 - t) * o) / sinf(o)),
    vec3_mul(b, o * cosf(t * o) / sinf(o))
  );
  return d;
}

// collage.c

void setup_collage();
void update_collage();
void draw_collage();

void evo();

// constellart.c

void setup_constell();
void draw_constell();

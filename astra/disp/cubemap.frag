#version 330 core
in vec2 fragPos;
out vec4 fragColor;

uniform sampler2D cubemap;
uniform float baseOpacity;

uniform float aspectRatio;
uniform vec4 viewOri;
uniform float projCircleR;

const float pi = acos(-1);

vec4 quat_mul(vec4 p, vec4 q) {
  return vec4(
    p.w * q.xyz + q.w * p.xyz + cross(p.xyz, q.xyz),
    p.w * q.w - dot(p.xyz, q.xyz)
  );
}
vec3 quat_rot(vec3 v, vec4 q) {
  return quat_mul(quat_mul(q, vec4(v, 0)), vec4(-q.xyz, q.w)).xyz;
}
vec3 rot(vec3 v, vec3 axis, float angle) {
  vec4 q = vec4(axis * sin(angle / 2), cos(angle / 2));
  return quat_rot(v, q);
}
vec3 look_at(vec2 fragPos) {
  vec2 p = fragPos / (projCircleR * vec2(1, aspectRatio));
  vec3 s = normalize(vec3(p * 2, -1 + dot(p, p)));
  return quat_rot(s, viewOri);
}

// t in [-1, +1] * [-1, +1]
// Returned value in [0, 1] * [0, 1]
vec2 EAC_facemap(vec2 t) {
  t = vec2(atan(t.x), atan(t.y)) * (4 / pi);
  return (t + 1) / 2;
}
vec2 EAC_texcoord(vec3 p) {
  float absx = abs(p.x);
  float absy = abs(p.y);
  float absz = abs(p.z);
  float absmax = max(max(absx, absy), absz);
  vec2 t;
  if (absmax == absx) {
    if (p.x > 0) t = vec2(1, 0) + EAC_facemap(vec2(-p.y, -p.z) / absmax);
    else         t = vec2(1, 1) + EAC_facemap(vec2(+p.z, +p.y) / absmax);
  } else if (absmax == absy) {
    if (p.y > 0) t = vec2(0, 0) + EAC_facemap(vec2(+p.x, -p.z) / absmax);
    else         t = vec2(2, 0) + EAC_facemap(vec2(-p.x, -p.z) / absmax);
  } else {
    if (p.z > 0) t = vec2(2, 1) + EAC_facemap(vec2(+p.x, +p.y) / absmax);
    else         t = vec2(0, 1) + EAC_facemap(vec2(-p.x, +p.y) / absmax);
  }
  t /= vec2(3, 2);
  return t;
}

void main() {
  vec3 lookAt = look_at(fragPos);
  vec2 texcoord = EAC_texcoord(lookAt);
  fragColor = texture(cubemap, texcoord) * baseOpacity;
}

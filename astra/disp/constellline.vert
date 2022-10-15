#version 330 core
layout (location = 0) in vec3 vertPos;

uniform float aspectRatio;
uniform vec4 viewOri;

const float pi = acos(-1);
const float projCircleR = 3;

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
vec3 scr_pos(vec3 s) {
  s = quat_rot(s, vec4(-viewOri.xyz, viewOri.w));
  vec2 p = s.xy / (1 - s.z);
  p *= (projCircleR * vec2(1, aspectRatio));
  // s.z = -1 at the centre, s.z = +1 at the back
  // A value of (s.z + 1) outside of [-1, 1] will be clipped
  return vec3(p, s.z + 1);
}

void main() {
  gl_Position = vec4(scr_pos(vertPos), 1);
}

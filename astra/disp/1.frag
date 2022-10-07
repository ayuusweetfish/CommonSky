#version 330 core
in vec2 fragPos;
out vec4 fragColor;

uniform vec2 viewCoord;
// uniform vec3 viewCen;
uniform vec3 camRight;

const float pi = acos(-1);
const vec3 camUp = vec3(0, 0, 1);
const float projCircleR = 3;

vec4 quat_mul(vec4 p, vec4 q) {
  return vec4(
    p.w * q.xyz + q.w * p.xyz + cross(p.xyz, q.xyz),
    p.w * q.w - dot(p.xyz, q.xyz)
  );
}

vec4 quat_rot(vec3 v, vec4 q) {
  return quat_mul(quat_mul(q, vec4(v, 0)), vec4(q.xyz, -q.w));
}

vec3 rot(vec3 v, vec3 axis, float angle) {
  vec4 q = vec4(axis * sin(angle / 2), cos(angle / 2));
  return quat_rot(v, q).xyz;
}

void main() {
  vec2 p = fragPos / projCircleR; // TODO: Aspect ratio
  vec3 lookAt = normalize(vec3(p * 2, -1 + dot(p, p)));
  lookAt = rot(lookAt, vec3(1, 0, 0), viewCoord.y + pi / 2);
  lookAt = rot(lookAt, vec3(0, 0, 1), -viewCoord.x);
  fragColor = vec4((lookAt + 1) / 2, 1);
}

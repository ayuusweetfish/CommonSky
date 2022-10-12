#version 330 core
in vec2 fragPos;
out vec4 fragColor;

uniform float aspectRatio;
uniform vec2 viewCoord;

uniform sampler2D cubemap[6];

const float pi = acos(-1);
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
vec3 look_at(vec2 fragPos) {
  vec2 p = fragPos / (projCircleR * vec2(1, aspectRatio));
  vec3 s = normalize(vec3(p * 2, -1 + dot(p, p)));
  s = rot(s, vec3(1, 0, 0), viewCoord.y + pi / 2);
  s = rot(s, vec3(0, 0, 1), viewCoord.x - pi / 2);
  return s;
}

void main() {
  vec3 lookAt = look_at(fragPos);
  float absx = abs(lookAt.x);
  float absy = abs(lookAt.y);
  float absz = abs(lookAt.z);
  float absmax = max(max(absx, absy), absz);
  int texidx;
  vec2 texcoord;
  if (absmax == absx) {
    texcoord.x = (lookAt.x > 0 ? -lookAt.z : lookAt.z);
    texcoord.y = lookAt.y;
    texidx = (lookAt.x > 0 ? 0 : 1);
  } else if (absmax == absy) {
    texcoord.x = lookAt.x;
    texcoord.y = (lookAt.y > 0 ? -lookAt.z : lookAt.z);
    texidx = (lookAt.y > 0 ? 2 : 3);
  } else {
    texcoord.x = (lookAt.z > 0 ? lookAt.x : -lookAt.x);
    texcoord.y = lookAt.y;
    texidx = (lookAt.z > 0 ? 4 : 5);
  }
  texcoord = (texcoord * vec2(1, -1) / absmax + 1) / 2;

  fragColor = texture(cubemap[texidx], texcoord);
}

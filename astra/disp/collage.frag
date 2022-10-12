#version 330 core
in vec2 fragPos;
out vec4 fragColor;

uniform sampler2D image;
uniform vec2 projCen;
#define MAX_ORD 6
uniform int ord;
uniform vec2 coeff[(MAX_ORD + 1) * (MAX_ORD + 2) / 2];

uniform float aspectRatio;
uniform vec2 viewCoord;

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
  vec3 s = look_at(fragPos);

  // Map to texture space
  // Stereographic map
  vec3 t = s;
  t = rot(t, vec3(0, 0, 1), -projCen.x * (pi/180));
  t = rot(t, vec3(0, -1, 0), (-90 - projCen.y) * (pi/180));
  vec2 u = t.xy / (1 - t.z);
  // Apply polynomial transform
  vec2 pows[MAX_ORD + 1];
  pows[0] = vec2(1);
  for (int i = 1; i <= ord; i++) pows[i] = pows[i - 1] * u;
  vec2 texCoord = vec2(0);
  for (int i = 0; i <= ord; i++)
    for (int j = 0; j <= ord - i; j++) {
      int id = (i+j) * (i+j+1) / 2 + i;
      texCoord += pows[i].x * pows[j].y * coeff[id];
    }

  // Sample texture
  if (texCoord.x < 0 || texCoord.x > 1 ||
      texCoord.y < 0 || texCoord.y > 1) {
    //fragColor = vec4(s, 0.5);
    fragColor = vec4(0);
  } else {
    fragColor = texture(image, texCoord);
    //fragColor = vec4(texCoord.xy, 0, 1);
  }
  //fragColor = vec4((u.xy + 1) / 2, 0, 0.95);
}

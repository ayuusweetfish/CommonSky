#version 330 core
#define double float
#define dvec2 vec2
in vec2 fragPos;
out vec4 fragColor;

uniform sampler2D image;
uniform vec2 projCen;
uniform int ord;
uniform vec2 coeff[28]; // (6+1) * (7+1) / 2

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
vec3 look_at(vec2 fragPos) {
  vec2 p = fragPos / (projCircleR * vec2(1, aspectRatio));
  vec3 s = normalize(vec3(p * 2, -1 + dot(p, p)));
  return quat_rot(s, viewOri);
}

void main() {
  vec3 s = look_at(fragPos);

  // Map to texture space
  // Stereographic map
  vec3 t = s;
  t = rot(t, vec3(0, 0, 1), -projCen.x);
  t = rot(t, vec3(0, -1, 0), (-pi/2 - projCen.y));
  vec2 u = t.xy / (1 - t.z);
  // Apply polynomial transform
  double x2 = u.x * u.x;
  double y2 = u.y * u.y;
  dvec2 texCoord = dvec2(0);
  texCoord += coeff[0];
  texCoord += coeff[1] * u.y;
  texCoord += coeff[2] * u.x;
  texCoord += coeff[3] * y2;
  texCoord += coeff[4] * u.x * u.y;
  texCoord += coeff[5] * x2;
  if (texCoord.x < 0 || texCoord.x > 1 || texCoord.y < 0 || texCoord.y > 1) discard;
  if (ord >= 3) {
    double x3 = x2 * u.x;
    double y3 = y2 * u.y;
    texCoord += coeff[6] * y3;
    texCoord += coeff[7] * y2 * u.x;
    texCoord += coeff[8] * u.y * x2;
    texCoord += coeff[9] * x3;
    if (texCoord.x < 0 || texCoord.x > 1 || texCoord.y < 0 || texCoord.y > 1) discard;
    if (ord >= 4) {
      double x4 = x3 * u.x;
      double y4 = y3 * u.y;
      texCoord += coeff[10] * y4;
      texCoord += coeff[11] * y3 * u.x;
      texCoord += coeff[12] * y2 * x2;
      texCoord += coeff[13] * u.y * x3;
      texCoord += coeff[14] * x4;
      if (texCoord.x < 0 || texCoord.x > 1 || texCoord.y < 0 || texCoord.y > 1) discard;
      if (ord >= 5) {
        double x5 = x4 * u.x;
        double y5 = y4 * u.y;
        texCoord += coeff[15] * y5;
        texCoord += coeff[16] * y4 * u.x;
        texCoord += coeff[17] * y3 * x2;
        texCoord += coeff[18] * y2 * x3;
        texCoord += coeff[19] * u.y * x4;
        texCoord += coeff[20] * x5;
        if (texCoord.x < 0 || texCoord.x > 1 || texCoord.y < 0 || texCoord.y > 1) discard;
        if (ord >= 6) {
          double x6 = x5 * u.x;
          double y6 = y5 * u.y;
          texCoord += coeff[21] * y6;
          texCoord += coeff[22] * y5 * u.x;
          texCoord += coeff[23] * y4 * x2;
          texCoord += coeff[24] * y3 * x3;
          texCoord += coeff[25] * y2 * x4;
          texCoord += coeff[26] * u.y * x5;
          texCoord += coeff[27] * x6;
          if (texCoord.x < 0 || texCoord.x > 1 || texCoord.y < 0 || texCoord.y > 1) discard;
        }
      }
    }
  }

  // Sample texture
  fragColor = texture(image, vec2(texCoord));
}

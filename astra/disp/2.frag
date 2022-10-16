#version 330 core
in vec2 fragTexCoord;
out vec4 fragColor;

const float pi = acos(-1);

vec3 EAC_modelcoord(vec2 t) {
  t *= vec2(3, 2);
  int xid = int(floor(t.x));
  int yid = int(floor(t.y));
  t -= vec2(xid, yid);
  t = (t * 2 - 1) * (pi / 4);
  t = vec2(tan(t.x), tan(t.y));
  vec3 p;
  if (xid == 0 && yid == 0) p = vec3(+t.x, +1, -t.y);
  if (xid == 1 && yid == 0) p = vec3(+1, -t.x, -t.y);
  if (xid == 2 && yid == 0) p = vec3(-t.x, -1, -t.y);
  if (xid == 0 && yid == 1) p = vec3(-t.x, +t.y, +1);
  if (xid == 1 && yid == 1) p = vec3(-1, +t.y, +t.x);
  if (xid == 2 && yid == 1) p = vec3(+t.x, +t.y, -1);
  return normalize(p);
}

void main() {
  vec3 lookAt = EAC_modelcoord(fragTexCoord);
  fragColor = vec4(0);
  if (lookAt.x >=  0.6 && lookAt.x <=  0.65) fragColor = vec4(1);
  if (lookAt.x <= -0.6 && lookAt.x >= -0.65) fragColor = vec4(0.6);
  if (abs(lookAt.z) <= 0.025) fragColor = vec4(1, 0.9, 0.3, 1);
}

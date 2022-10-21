#version 330 core
#define double float
#define dvec2 vec2
in vec2 fragPos;
out vec4 fragColor;

uniform float seed;
uniform float entertime, exittime;
uniform bool transp;

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

// https://thebookofshaders.com/11/

// Some useful functions
vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }
vec4 permute(vec4 x) { return mod289(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
float snoise(vec3 v)
{
  const vec2 C = vec2(1.0/6.0, 1.0/3.0);
  const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

  // First corner
  vec3 i  = floor(v + dot(v, C.yyy));
  vec3 x0 =   v - i + dot(i, C.xxx);

  // Other corners
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min(g.xyz, l.zxy);
  vec3 i2 = max(g.xyz, l.zxy);

  //   x0 = x0 - 0.0 + 0.0 * C.xxx;
  //   x1 = x0 - i1  + 1.0 * C.xxx;
  //   x2 = x0 - i2  + 2.0 * C.xxx;
  //   x3 = x0 - 1.0 + 3.0 * C.xxx;
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
  vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y

  // Permutations
  i = mod289(i);
  vec4 p = permute(permute(permute(
     i.z + vec4(0.0, i1.z, i2.z, 1.0))
   + i.y + vec4(0.0, i1.y, i2.y, 1.0))
   + i.x + vec4(0.0, i1.x, i2.x, 1.0));

  // Gradients: 7x7 points over a square, mapped onto an octahedron.
  // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
  float n_ = 0.142857142857; // 1.0/7.0
  vec3  ns = n_ * D.wyz - D.xzx;

  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  // mod(p,7*7)

  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_);    // mod(j,N)

  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);

  vec4 b0 = vec4(x.xy, y.xy);
  vec4 b1 = vec4(x.zw, y.zw);

  //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
  //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));

  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww;

  vec3 p0 = vec3(a0.xy,h.x);
  vec3 p1 = vec3(a0.zw,h.y);
  vec3 p2 = vec3(a1.xy,h.z);
  vec3 p3 = vec3(a1.zw,h.w);

  //Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;

  // Mix final noise value
  vec4 m = max(0.5 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m * m;
  return 105.0 * dot(m*m, vec4(dot(p0,x0),dot(p1,x1),
                        dot(p2,x2), dot(p3,x3)));
}

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
  if (xid == 0 && yid == 1) p = vec3(-t.x, +t.y, -1);
  if (xid == 1 && yid == 1) p = vec3(-1, +t.y, +t.x);
  if (xid == 2 && yid == 1) p = vec3(+t.x, +t.y, +1);
  return normalize(p);
}

vec2 maxedgeupdate(vec2 maxedge, vec2 texCoord) {
  return max(max(maxedge, -texCoord), texCoord - 1);
}
void main() {
  vec3 s;
  if (!transp) s = look_at(fragPos);
  else s = EAC_modelcoord((fragPos + 1) / 2);

  vec2 maxedge = vec2(0, 0);

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
  maxedge = maxedgeupdate(maxedge, texCoord);
  if (ord >= 3) {
    double x3 = x2 * u.x;
    double y3 = y2 * u.y;
    texCoord += coeff[6] * y3;
    texCoord += coeff[7] * y2 * u.x;
    texCoord += coeff[8] * u.y * x2;
    texCoord += coeff[9] * x3;
    maxedge = maxedgeupdate(maxedge, texCoord);
    if (ord >= 4) {
      double x4 = x3 * u.x;
      double y4 = y3 * u.y;
      texCoord += coeff[10] * y4;
      texCoord += coeff[11] * y3 * u.x;
      texCoord += coeff[12] * y2 * x2;
      texCoord += coeff[13] * u.y * x3;
      texCoord += coeff[14] * x4;
      maxedge = maxedgeupdate(maxedge, texCoord);
      if (ord >= 5) {
        double x5 = x4 * u.x;
        double y5 = y4 * u.y;
        texCoord += coeff[15] * y5;
        texCoord += coeff[16] * y4 * u.x;
        texCoord += coeff[17] * y3 * x2;
        texCoord += coeff[18] * y2 * x3;
        texCoord += coeff[19] * u.y * x4;
        texCoord += coeff[20] * x5;
        maxedge = maxedgeupdate(maxedge, texCoord);
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
          maxedge = maxedgeupdate(maxedge, texCoord);
        }
      }
    }
  }
  if (texCoord.x < 0 || texCoord.x > 1 || texCoord.y < 0 || texCoord.y > 1) discard;
  float maxedgetol = 0.02;
  if (maxedge.x >= maxedgetol || maxedge.y >= maxedgetol) discard;

  // Erosion rate
  float headroom1 = 1;
  float noiserate1 = 1;
  float headroom2 = 1.2;
  float noiserate2 = 1;

  // (1) Fade-in
  float eps = 30;
  float epsfactor = 0.95 + snoise(((t + seed) - 1016) * 10) * 0.05;
  float dxdx = dFdx(texCoord.x);
  float dxdy = dFdy(texCoord.x);
  float dydx = dFdx(texCoord.y);
  float dydy = dFdy(texCoord.y);
  float epsx = eps * sqrt(dxdx * dxdx + dxdy * dxdy) * epsfactor;
  float epsy = eps * sqrt(dydx * dydx + dydy * dydy) * epsfactor;
  float edgefactor = 1;
  if (texCoord.x < epsx) edgefactor *= texCoord.x / epsx;
  if (texCoord.x > 1 - epsx) edgefactor *= (1 - texCoord.x) / epsx;
  if (texCoord.y < epsy) edgefactor *= texCoord.y / epsy;
  if (texCoord.y > 1 - epsy) edgefactor *= (1 - texCoord.y) / epsy;
  edgefactor *= (1 - maxedge.x / (maxedgetol * epsfactor));
  edgefactor *= (1 - maxedge.y / (maxedgetol * epsfactor));
  noiserate1 = edgefactor;

  float d_texcen = sqrt(
    (1 - 2 * abs(texCoord.x - 0.5)) *
    (1 - 2 * abs(texCoord.y - 0.5))
  );
  // Clamp at 1 to avoid numerical error cumulation
  float d_viewcen = acos(min(1, dot(s, vec3(
    cos(projCen.y) * cos(projCen.x),
    cos(projCen.y) * sin(projCen.x),
    sin(projCen.y)))));

  float enterrate = sqrt(
    clamp(d_texcen - 1 + entertime, 0, 1) *
    clamp((entertime * 0.4 - d_viewcen) * 4, 0, 1)
  );
  float noisevalenter = (snoise((t - seed - 1021) * 12) + 1) / 2;
  enterrate = clamp((enterrate * 6 - noisevalenter) / 5, 0, 1);
  noiserate1 *= enterrate;

  // (2) Fade-out
  if (!transp && exittime > 0) {
    float x = min(exittime / 6, 1);
    // Scaled Beta function
    float k = 4;
    float x1 = (x * k + 1) / (k + 1);
    float scale = pow(k, k) / pow(k + 1, k + 1);
    float f = x1 * pow(1 - x1, k) / scale;
    noiserate2 *= f;
  }

  float noiseval1 = (snoise((t + seed) * 80) + 1) / 2;
  float noiseval2 = (snoise(((t + seed) + 2022) * 15) + 1) / 2;

  // Sample texture
  fragColor = texture(image, vec2(texCoord));
  fragColor.a *= clamp((noiserate1 * (1 + headroom1) - noiseval1) / headroom1, 0, 1);
  fragColor.a *= clamp((noiserate2 * (1 + headroom2) - noiseval2) / headroom2, 0, 1);
  if (transp) {
    fragColor.a *= 0.75;
    fragColor.rgb *= fragColor.a;
  }
}

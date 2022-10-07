#pragma language glsl3
uniform vec3 c;

vec4 effect(vec4 color, Image tex, vec2 texture_coords, vec2 screen_coords) {
  // Dither by 8x8 Bayer matrix
  vec3 c1 = c * 255.999;
  int xi = int(screen_coords.x) % 8;
  int yi = int(screen_coords.y) % 8;
  int orderi =
    ((((((yi >> 2) ^ (xi >> 2)) & 1) << 1) | ((yi >> 2) & 1)) << 0) |
    ((((((yi >> 1) ^ (xi >> 1)) & 1) << 1) | ((yi >> 1) & 1)) << 2) |
    ((((((yi >> 0) ^ (xi >> 0)) & 1) << 1) | ((yi >> 0) & 1)) << 4);
  float order = float(orderi) / 64;

  int ri = int(c1.r);
  int gi = int(c1.g);
  int bi = int(c1.b);
  if (c1.r - ri >= order) ri += 1;
  if (c1.g - gi >= order) gi += 1;
  if (c1.b - bi >= order) bi += 1;

  return vec4(float(ri) / 255, float(gi) / 255, float(bi) / 255, 1);
}

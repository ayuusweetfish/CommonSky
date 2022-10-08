#pragma language glsl3

uniform float offy;
uniform vec3 base, highlight, water;
uniform float highlightAmt;
uniform float sunsetAmt;

vec4 effect(vec4 color, Image tex, vec2 texture_coords, vec2 screen_coords) {
  vec2 p = (screen_coords - vec2(0, offy)) / love_ScreenSize.xy;
  vec3 c = base;

  float cenx2 = (p.x - 0.5) * (p.x - 0.5);

  if (p.y >= 0.72) {
    float t1 = min(1, (p.y - 0.72) / 0.45);
    c += (water - base) * t1 * t1;
    float y2 = 0.78 + 0.02 * sunsetAmt
      + 0.04 * exp(-cenx2 * 16) * (1 + sunsetAmt * 0.6);
    if (p.y <= y2) {
      float t2 = (p.y - 0.72) / (y2 - 0.72);
      c += (highlight - c) * (1 - t2) * (1 - t2) * highlightAmt;
    }
  } else {
    float d1 = 0.1 +
      0.1 * exp(-cenx2 * 16) +
      0.2 * exp(-cenx2 * 5) * sunsetAmt;
    float t1 = 1 - (0.72 - p.y) / d1;
    if (t1 > 0) {
      c += (highlight - base) * highlightAmt * t1 * t1;
    }
  }

  // Dither by 32x32 Bayer matrix
  int xi = int(screen_coords.x) % 32;
  int yi = int(screen_coords.y) % 32;
  int orderi =
    (((xi ^ yi) & (1 << 4)) >> 3) | ((yi & (1 << 4)) >> 4) |
    (((xi ^ yi) & (1 << 3)) >> 0) | ((yi & (1 << 3)) >> 1) |
    (((xi ^ yi) & (1 << 2)) << 3) | ((yi & (1 << 2)) << 2) |
    (((xi ^ yi) & (1 << 1)) << 6) | ((yi & (1 << 1)) << 5) |
    (((xi ^ yi) & (1 << 0)) << 9) | ((yi & (1 << 0)) << 8);
  float order = float(orderi) / 1024;

  return vec4(c + (order - 0.5) / 255, 1);
}

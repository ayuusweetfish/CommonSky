#pragma language glsl3
uniform vec3 c;

vec4 effect(vec4 color, Image tex, vec2 texture_coords, vec2 screen_coords) {
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

  return vec4(c + vec3((order - 0.5) / 255), 1);
}

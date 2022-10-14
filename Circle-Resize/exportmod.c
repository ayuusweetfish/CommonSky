// gcc -fPIC -shared exportmod.c -o libexportmod.so
// gcc -fPIC -dynamiclib exportmod.c -o libexportmod.dylib
// x86_64-w64-mingw32-gcc -fPIC -shared -static -static-libgcc exportmod.c -o exportmod.dll
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TARGET_W 1920
#define TARGET_H 1080
#define TARGET_R 216

#define roundf(_x) ((int)floor((_x) + 0.5))
static inline int clamp(int x, int a, int b)
{
  return x < a ? a : x > b ? b : x;
}

void export_image(
  const char *in_path, const char *out_path,
  double cx, double cy, double cr)
{
  int w, h;
  unsigned char *pix = stbi_load(in_path, &w, &h, NULL, 4);
  double scale = TARGET_R / cr;
  double tl_im_x = (cx - TARGET_W / 2 / scale);
  double tl_im_y = (cy - TARGET_H / 2 / scale);
  double br_im_x = (cx + TARGET_W / 2 / scale);
  double br_im_y = (cy + TARGET_H / 2 / scale);

  int clip_x1 = clamp(round(tl_im_x), 0, w - 1);
  int clip_x2 = clamp(round(br_im_x), clip_x1 + 1, w);
  int clip_y1 = clamp(round(tl_im_y), 0, h - 1);
  int clip_y2 = clamp(round(br_im_y), clip_y1 + 1, h);
  int scaled_offx = clamp(round((clip_x1 - tl_im_x) * scale), 0, TARGET_W - 1);
  int scaled_offy = clamp(round((clip_y1 - tl_im_y) * scale), 0, TARGET_H - 1);
  int scaled_w = clamp(round((clip_x2 - clip_x1) * scale), 1, TARGET_W - scaled_offx);
  int scaled_h = clamp(round((clip_y2 - clip_y1) * scale), 1, TARGET_H - scaled_offy);

  unsigned char *canvas =
    (unsigned char *)malloc(TARGET_W * TARGET_H * 4);
  memset(canvas, 0, TARGET_W * TARGET_H * 4);
  stbir_resize_uint8_srgb(
    pix + (clip_y1 * w + clip_x1) * 4,
    clip_x2 - clip_x1,
    clip_y2 - clip_y1,
    w * 4,
    canvas + (scaled_offy * TARGET_W + scaled_offx) * 4,
    scaled_w, scaled_h,
    TARGET_W * 4,
    4, 3, 0);

  stbi_write_png(out_path, TARGET_W, TARGET_H, 4, canvas, TARGET_W * 4);

  free(canvas);
}

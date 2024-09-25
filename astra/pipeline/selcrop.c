// gcc -o selcrop -O2 selcrop.c -I ../../aux/raylib-4.2.0/include ../../aux/raylib-4.2.0/lib/libraylib.a -framework OpenGL -framework Cocoa -framework IOKit
#include "raylib.h"

#include <math.h> // ceilf
#include <stdbool.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("Usage: %s <input> <output>\n", argv[0]);
    return 0;
  }

  SetTraceLogLevel(LOG_WARNING);

  Image img = LoadImage(argv[1]);
  int iw = img.width;
  int ih = img.height;
  if (iw == 0) {
    printf("Cannot open image\n");
    return 1;
  }

  float scx = (float)800 / iw;
  float scy = (float)600 / ih;
  float sc = (scx < scy ? scx : scy);
  int scrw = (int)ceilf(iw * sc + 50);
  int scrh = (int)ceilf(ih * sc + 50);
  float offx = (scrw - iw * sc) / 2;
  float offy = (scrh - ih * sc) / 2;

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(scrw, scrh, NULL);
  SetTargetFPS(60);

  Texture2D tex = LoadTextureFromImage(img);

  Vector2 p1, p2;
  bool has_rect = false;

  float x1, y1, x2, y2;

  while (!WindowShouldClose()) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      has_rect = true;
      p1 = GetMousePosition();
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      p2 = GetMousePosition();
    }
    if (has_rect) {
      x1 = p1.x; y1 = p1.y;
      x2 = p2.x; y2 = p2.y;
      if (x1 > x2) { float t = x1; x1 = x2; x2 = t; }
      if (y1 > y2) { float t = y1; y1 = y2; y2 = t; }
      x1 = floorf((x1 - offx) / sc); x2 = ceilf((x2 - offx) / sc);
      y1 = floorf((y1 - offy) / sc); y2 = ceilf((y2 - offy) / sc);
      x1 = (x1 < 0 ? 0 : x1 >= iw ? iw - 1 : x1);
      x2 = (x2 <= x1 ? x1 + 1 : x2 > iw ? iw : x2);
      y1 = (y1 < 0 ? 0 : y1 >= ih ? ih - 1 : y1);
      y2 = (y2 <= y1 ? y1 + 1 : y2 > ih ? ih : y2);
      if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) break;
    }

    BeginDrawing();
    DrawTextureEx(tex, (Vector2){offx, offy}, 0, sc, WHITE);
    if (has_rect) {
      DrawRectangleLinesEx((Rectangle){
        x1 * sc + offx, y1 * sc + offx,
        (x2 - x1) * sc, (y2 - y1) * sc,
      }, 2, GREEN);
    }
    EndDrawing();
  }

  if (!has_rect) {
    printf("No rectangle selected!\n");
    return 1;
  }

  CloseWindow();

  ImageCrop(&img, (Rectangle){x1, y1, x2 - x1, y2 - y1});
  if (!ExportImage(img, argv[2])) {
    printf("Cannot write to output!\n");
    return 1;
  }

  return 0;
}

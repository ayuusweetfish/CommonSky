#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

double *read_fits_table(const char *path, const char **colnames, long *count);

// Image and scaling

Image img;
int iw, ih;
Texture2D itex;

int scrw, scrh;
float sc;
#define scale(_x, _y) (Vector2){(_x) * sc, (_y) * sc}

// FITS tables

const char *col_names_axy[] = {"X", "Y", NULL};
long nr_axy;
double *data_axy;
#define axy_x(_i) data_axy[(_i) * 2 + 0]
#define axy_y(_i) data_axy[(_i) * 2 + 1]

const char *col_names_corr[] = {"index_x", "index_y", "field_id", "index_ra", "index_dec", NULL};
long nr_corr;
double *data_corr;
#define corr_x(_i)      data_corr[(_i) * 5 + 0]
#define corr_y(_i)      data_corr[(_i) * 5 + 1]
#define corr_axyid(_i)  (long)data_corr[(_i) * 5 + 2]
#define corr_ra(_i)     data_corr[(_i) * 5 + 3]
#define corr_dec(_i)    data_corr[(_i) * 5 + 4]

// Auxiliary data
const int AXY_LIMIT = 200;
bool axy_matched[AXY_LIMIT] = { false };
bool refined_matched[AXY_LIMIT] = { false };

enum dispmode_e {
  DISP_CALCULATED = 0,
  DISP_REFINED,
  DISP_NUM,
} dispmode;

void update_and_draw()
{
  // Update
  if (IsKeyPressed(KEY_SPACE)) dispmode = (dispmode + 1) % DISP_NUM;

  // Draw
  BeginDrawing();

  DrawTextureEx(itex, (Vector2){0, 0}, 0, sc, WHITE);

  // solve-field.c: plot_index_overlay
  if (dispmode == DISP_CALCULATED) {
    for (long i = 0; i < nr_corr; i++) {
      if (corr_axyid(i) >= AXY_LIMIT)
        DrawRing(
          scale(axy_x(corr_axyid(i)), axy_y(corr_axyid(i))),
          4, 6, 0, 360, 12, PURPLE);
    }
    for (long i = 0; i < nr_axy && i < AXY_LIMIT; i++) {
      DrawRing(scale(axy_x(i), axy_y(i)),
        4, 6, 0, 360, 12, axy_matched[i] ? ORANGE : RED);
    }
    for (long i = 0; i < nr_corr; i++) {
      DrawRing(scale(corr_x(i), corr_y(i)),
        2, 4, 0, 360, 12, GREEN);
      DrawLineEx(
        scale(corr_x(i), corr_y(i)),
        scale(axy_x(corr_axyid(i)), axy_y(corr_axyid(i))),
        2, GREEN);
    }
  } else if (dispmode == DISP_REFINED) {
    for (long i = 0; i < nr_axy && i < AXY_LIMIT; i++) {
      DrawRing(scale(axy_x(i), axy_y(i)),
        4, 6, 0, 360, 12, refined_matched[i] ? ORANGE : RED);
    }
    for (long i = 0; i < nr_corr; i++) {
      DrawRing(scale(corr_x(i), corr_y(i)),
        2, 4, 0, 360, 12, GREEN);
    }
  }

  EndDrawing();
}

int main(int argc, char *argv[])
{
  if (argc < 4) {
    printf("Usage: %s <image> <objs FITS (axy)> <link FITS (corr)>\n", argv[0]);
    return 0;
  }

  // Load image
  Image img = LoadImage(argv[1]);
  int iw = img.width;
  int ih = img.height;
  if (iw == 0) {
    printf("Cannot open image\n");
    return 1;
  }

  float scx = (float)1080 / iw;
  float scy = (float)960 / ih;
  sc = (scx < scy ? scx : scy);
  scrw = iw * sc; // Round down to avoid black borders
  scrh = ih * sc;

  // Read FITS tables
  if ((data_axy = read_fits_table(argv[2], col_names_axy, &nr_axy)) == NULL) {
    printf("Error loading the objects table\n");
    return 1;
  }
  if ((data_corr = read_fits_table(argv[3], col_names_corr, &nr_corr)) == NULL) {
    printf("Error loading the correlation table\n");
    return 1;
  }

  // Graphics setup
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(scrw, scrh, NULL);
  SetTargetFPS(60);

  itex = LoadTextureFromImage(img);

  // Auxiliary data initialization
  for (long i = 0; i < nr_corr; i++) {
    if (corr_axyid(i) < AXY_LIMIT) axy_matched[corr_axyid(i)] = true;
  }

  while (!WindowShouldClose()) {
    update_and_draw();
  }

  CloseWindow();

  return 0;
}


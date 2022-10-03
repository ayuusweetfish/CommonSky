#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

double *read_fits_table(const char *path, const char **colnames, long *count);

// Image and scaling

Image img;
int iw, ih;
Texture2D itex;

int scrw, scrh;
float sc, sc_base;
float offx, offy;
// Image space -> screen
#define scale(_x, _y) (Vector2){((_x) - offx) * sc, ((_y) - offy) * sc}
// Screen space -> image
#define iscale(_x, _y) (Vector2){(_x) / sc + offx, (_y) / sc + offy}

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

// Refinement
int refi_axy_match[AXY_LIMIT];
int *refi_cat_match;

static inline void refi_clear_axy(int a)
{
  if (refi_axy_match[a] != -1) {
    refi_cat_match[refi_axy_match[a]] = -1;
    refi_axy_match[a] = -1;
  }
}
static inline void refi_clear_cat(int c)
{
  if (refi_cat_match[c] != -1) {
    refi_axy_match[refi_cat_match[c]] = -1;
    refi_cat_match[c] = -1;
  }
}
static inline void refi_match(int c, int a)
{
  refi_clear_cat(c);
  refi_clear_axy(a);
  refi_cat_match[c] = a;
  refi_axy_match[a] = c;
}

// Display and interactions
enum dispmode_e {
  DISP_CALCULATED = 0,
  DISP_REFINED,
  DISP_NUM,
} dispmode;
int sel_cat = -1;
int hover_cat = -1, hover_axy = -1;

void update_and_draw()
{
  // Update

  // Movement
  #define clamp(_x, _a, _b) \
    ((_x) < (_a) ? (_a) : (_x) > (_b) ? (_b) : (_x))
  if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_TWO)) sc += 0.02 * sc_base;
  if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_ONE)) sc -= 0.02 * sc_base;
  sc = clamp(sc, sc_base * 1, sc_base * 5);
  if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) offy -= 10;
  if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) offy += 10;
  offy = clamp(offy, 0, ih - scrh / sc);
  if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) offx -= 10;
  if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) offx += 10;
  offx = clamp(offx, 0, iw - scrw / sc);
  #undef clamp

  // Display mode switch
  if (IsKeyPressed(KEY_SPACE)) {
    dispmode = (dispmode + 1) % DISP_NUM;
    if (dispmode == DISP_CALCULATED) {
      sel_cat = hover_cat = hover_axy = -1;
    }
  }

  // Selection
  if (dispmode == DISP_REFINED) {
    Vector2 p = GetMousePosition();
    p = iscale(p.x, p.y);
    #define dist_sq(_x, _y) \
      ((p.x - (_x)) * (p.x - (_x)) + (p.y - (_y)) * (p.y - (_y)))
    double dsq_best = 100;
    if (sel_cat == -1) {
      hover_cat = -1;
      for (long i = 0; i < nr_corr; i++) {
        double dsq = dist_sq(corr_x(i), corr_y(i));
        if (dsq < dsq_best) { dsq_best = dsq; hover_cat = i; }
      }
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        sel_cat = hover_cat;
      }
      if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        refi_clear_cat(hover_cat);
      }
    } else {
      hover_axy = -1;
      for (long i = 0; i < nr_axy && i < AXY_LIMIT; i++) {
        double dsq = dist_sq(axy_x(i), axy_y(i));
        if (dsq < dsq_best) { dsq_best = dsq; hover_axy = i; }
      }
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hover_axy != -1) refi_match(sel_cat, hover_axy);
        sel_cat = hover_cat = hover_axy = -1;
      }
    }
    #undef dist
  }

  // Draw

  BeginDrawing();

  DrawTextureEx(itex, (Vector2){-offx * sc, -offy * sc}, 0, sc, WHITE);

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
        4, 6, 0, 360, 12,
        i == hover_axy ?
          WHITE : (refi_axy_match[i] != -1 ? ORANGE : RED));
    }
    for (long i = 0; i < nr_corr; i++) {
      DrawRing(scale(corr_x(i), corr_y(i)),
        2, 4, 0, 360, 12,
        i == hover_cat || i == sel_cat ?
          WHITE : (refi_cat_match[i] != -1 ? GREEN : LIME));
      if (refi_cat_match[i] != -1)
        DrawLineEx(
          scale(corr_x(i), corr_y(i)),
          scale(axy_x(refi_cat_match[i]), axy_y(refi_cat_match[i])),
          2, GREEN);
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
  iw = img.width;
  ih = img.height;
  if (iw == 0) {
    printf("Cannot open image\n");
    return 1;
  }

  float scx = (float)1080 / iw;
  float scy = (float)960 / ih;
  sc = sc_base = (scx < scy ? scx : scy);
  scrw = iw * sc; // Round down to avoid black borders
  scrh = ih * sc;
  offx = offy = 0;

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

  memset(refi_axy_match, -1, sizeof refi_axy_match);
  refi_cat_match = (int *)malloc(sizeof(int) * nr_corr);
  memset(refi_cat_match, -1, sizeof(int) * nr_corr);

  while (!WindowShouldClose()) {
    update_and_draw();
  }

  CloseWindow();

  return 0;
}


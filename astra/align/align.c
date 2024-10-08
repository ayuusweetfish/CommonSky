// gcc -o align -O2 align.c readfits.c polyfit.c constell.c -I ../../aux -I ../../aux/raylib-4.2.0/include ../../aux/raylib-4.2.0/lib/libraylib.a -I ../../aux/cfitsio-4.5.0 ../../aux/cfitsio-4.5.0/*.o -framework OpenGL -framework Cocoa -framework IOKit -lm -lcurl -lz
// ./align ../img-processed/32186236600_7605b3bdec_b{.png,.axy,.rdls,-indx.xyls,.corr,.wcs,.refi,.coeff}
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

double *read_fits_table(const char *path, const char **colnames, long *count);
int read_fits_headers(const char *path, const char **keys, double *values);
void polyfit(int n, double *u, double *v, double view_ra, double view_dec, int ord, double *o_coeff);
void polyapply(int n, double *u, double view_ra, double view_dec, int ord, double *coeff);
void constell_load();
void constell_prepare(
  double ra_min, double ra_max,
  double dec_min, double dec_max,
  double view_ra, double view_dec, int ord, double *coeff);
void constell_draw(int iw, int ih, int scrw, int scrh, float sc, float offx, float offy);

// Image and scaling

Image img;
int iw, ih;
Texture2D itex;

int scrw, scrh;
float sc, sc_base;
float offx, offy;
// Image space -> screen
#define scale(_x, _y) (Vector2){((_x) - offx) * sc + scrw/2, ((_y) - offy) * sc + scrh/2}
// Screen space -> image
#define iscale(_x, _y) (Vector2){((_x) - scrw/2) / sc + offx, ((_y) - scrh/2) / sc + offy}

// Utility functions
static inline bool between(float x, float a, float b)
{
  return x == a || x == b || ((x < a) ^ (x < b));
}

// FITS tables

const char *col_names_axy[] = {"X", "Y", NULL};
long nr_axy;
double *data_axy;
#define axy_x(_i) data_axy[(_i) * 2 + 0]
#define axy_y(_i) data_axy[(_i) * 2 + 1]

const char *col_names_rdls[] = {"RA", "DEC", NULL};
long nr_rdls;
double *data_rdls;
const char *col_names_xyls[] = {"X", "Y", NULL};
long nr_xyls;
double *data_xyls;
#define cat_ra(_i)  data_rdls[(_i) * 2 + 0]
#define cat_dec(_i) data_rdls[(_i) * 2 + 1]
#define cat_x(_i)   data_xyls[(_i) * 2 + 0]
#define cat_y(_i)   data_xyls[(_i) * 2 + 1]
#define nr_cat nr_rdls

const char *col_names_corr[] = {"field_id", "index_id", NULL};
long nr_corr;
double *data_corr;
#define corr_axyid(_i)  (long)data_corr[(_i) * 2 + 0]
#define corr_catid(_i)  (long)data_corr[(_i) * 2 + 1]

const char *header_names_wcs[] = {"CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2", NULL};
double view_ra, view_dec;

// Auxiliary data

int axy_limit;
bool *axy_matched;

// Reverse lookup of catalogue (rdls) records in correltaion table
// -1 if not present
int *corr_id;

// Refinement
int *refi_axy_match;
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
  DISP_REFINED = 0,
  DISP_APPLIED,
  DISP_NUM,
} dispmode;
int sel_cat = -1;
int hover_cat = -1, hover_axy = -1;

bool rectsel = false;
float rectx, recty;
bool rectremove;

// 2 - In the initial state, displaying calculated matches
// 1 - Tab pressed
// 0 - Out of the initial state
char initial_calculated = 2;

const char *save_load_path = NULL;
void save()
{
  FILE *fp = fopen(save_load_path, "w");
  if (fp == NULL) {
    printf("Cannot save to %s\n", save_load_path);
    exit(1);
  }
  for (long i = 0; i < nr_axy && i < axy_limit; i++)
    fprintf(fp, "%d ", refi_axy_match[i]);
  fclose(fp);
}
void load()
{
  FILE *fp = fopen(save_load_path, "r");
  if (fp == NULL) return;
  for (long i = 0; i < nr_axy && i < axy_limit; i++)
    fscanf(fp, "%d", &refi_axy_match[i]);
  if (ferror(fp)) {
    printf("Cannot load from %s\n", save_load_path);
    exit(1);
  }
  fclose(fp);
  for (long i = 0; i < nr_axy && i < axy_limit; i++)
    if (refi_axy_match[i] != -1)
      refi_cat_match[refi_axy_match[i]] = i;
}
int probe_axy_limit()
{
  FILE *fp = fopen(save_load_path, "r");
  if (fp == NULL) return 500;
  int count = 0, x;
  while (fscanf(fp, "%d", &x) == 1) count++;
  return count;
}

// For fitting
#define MAX_ORD 10
double poly_coeff[(MAX_ORD + 1) * (MAX_ORD + 2)];
int ord = 4;
double *applied = NULL;

const char *coeff_path = NULL;
void save_coeff()
{
  FILE *fp = fopen(coeff_path, "w");
  if (fp == NULL) {
    printf("Cannot save to %s\n", coeff_path);
    exit(1);
  }
  fprintf(fp, "%d\n%.16lf %.16lf\n", ord, view_ra, view_dec);
  for (int i = 0; i < (ord + 1) * (ord + 2); i++)
    fprintf(fp, "%.16lf\n", poly_coeff[i]);
  fclose(fp);
}

int grid_ra_min, grid_ra_max;
int grid_dec_min, grid_dec_max;
const int GRID_SUBDIV = 50;
double grid_ra_ngroups, grid_dec_ngroups;
double *grid_ra_applied = NULL;
double *grid_dec_applied = NULL;

void fit()
{
  double *u = (double *)malloc(sizeof(double) * axy_limit * 2);
  double *v = (double *)malloc(sizeof(double) * axy_limit * 2);
  int n = 0;
  for (long i = 0; i < nr_axy && i < axy_limit; i++) {
    if (refi_axy_match[i] != -1) {
      u[n * 2 + 0] = cat_ra(refi_axy_match[i]);
      u[n * 2 + 1] = cat_dec(refi_axy_match[i]);
      v[n * 2 + 0] = axy_x(i) / iw;
      v[n * 2 + 1] = axy_y(i) / ih;
      n++;
    }
  }
  polyfit(n, u, v, view_ra, view_dec, ord, poly_coeff);
  free(u);
  free(v);
  save_coeff();

  if (applied == NULL) {
    applied = (double *)malloc(sizeof(double) * nr_cat * 2);
    float ra_min, ra_max;
    float dec_min, dec_max;
    ra_min = dec_min = 9999;
    ra_max = dec_max = -9999;
    for (long i = 0; i < nr_cat; i++) {
      if (cat_ra(i) < ra_min) ra_min = cat_ra(i);
      if (cat_ra(i) > ra_max) ra_max = cat_ra(i);
      if (cat_dec(i) < dec_min) dec_min = cat_dec(i);
      if (cat_dec(i) > dec_max) dec_max = cat_dec(i);
    }
    // Value range guarantees precise arithmetic
    grid_ra_min = floor(ra_min / 10) * 10;
    grid_ra_max = ceil(ra_max / 10) * 10;
    grid_dec_min = floor(dec_min / 10) * 10;
    grid_dec_max = ceil(dec_max / 10) * 10;
    // printf("%4d %4d\n", grid_ra_min, grid_ra_max);
    // printf("%4d %4d\n", grid_dec_min, grid_dec_max);
    grid_ra_ngroups = (grid_ra_max - grid_ra_min) / 10 + 1;
    grid_dec_ngroups = (grid_dec_max - grid_dec_min) / 10 + 1;
    grid_ra_applied = (double *)malloc(
      sizeof(double) * grid_ra_ngroups * GRID_SUBDIV * 2);
    grid_dec_applied = (double *)malloc(
      sizeof(double) * grid_dec_ngroups * GRID_SUBDIV * 2);
  }

  for (long i = 0; i < nr_cat; i++) {
    applied[i * 2 + 0] = cat_ra(i);
    applied[i * 2 + 1] = cat_dec(i);
  }
  polyapply(nr_cat, applied, view_ra, view_dec, ord, poly_coeff);

  for (int i = 0; i < grid_ra_ngroups; i++) {
    for (int j = 0; j < GRID_SUBDIV; j++) {
      grid_ra_applied[(i * GRID_SUBDIV + j) * 2 + 0] = grid_ra_min + 10 * i;
      grid_ra_applied[(i * GRID_SUBDIV + j) * 2 + 1] = grid_dec_min +
        (grid_dec_max - grid_dec_min) * ((double)j / (GRID_SUBDIV - 1));
    }
  }
  polyapply(grid_ra_ngroups * GRID_SUBDIV, grid_ra_applied, view_ra, view_dec, ord, poly_coeff);
  for (int i = 0; i < grid_dec_ngroups; i++) {
    for (int j = 0; j < GRID_SUBDIV; j++) {
      grid_dec_applied[(i * GRID_SUBDIV + j) * 2 + 0] = grid_ra_min +
        (grid_ra_max - grid_ra_min) * ((double)j / (GRID_SUBDIV - 1));
      grid_dec_applied[(i * GRID_SUBDIV + j) * 2 + 1] = grid_dec_min + 10 *i;
    }
  }
  polyapply(grid_dec_ngroups * GRID_SUBDIV, grid_dec_applied, view_ra, view_dec, ord, poly_coeff);

  constell_prepare(
    grid_ra_min, grid_ra_max, grid_dec_min, grid_dec_max,
    view_ra, view_dec, ord, poly_coeff);
}

void update_and_draw()
{
  // Update

  // Movement
  #define clamp(_x, _a, _b) \
    ((_x) < (_a) ? (_a) : (_x) > (_b) ? (_b) : (_x))
  if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_TWO)) sc += 0.02 * sc_base;
  if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_ONE)) sc -= 0.02 * sc_base;
  sc += 0.02 * GetMouseWheelMove();
  sc = clamp(sc, sc_base * 1, sc_base * 5);
  if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) offy -= 10;
  if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) offy += 10;
  offy = clamp(offy, scrh / sc / 2, ih - scrh / sc / 2);
  if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) offx -= 10;
  if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) offx += 10;
  offx = clamp(offx, scrw / sc / 2, iw - scrw / sc / 2);
  #undef clamp

  // Display mode switch
  if (IsKeyPressed(KEY_SPACE)) {
    dispmode = (dispmode + 1) % DISP_NUM;
    if (dispmode != DISP_REFINED) {
      sel_cat = hover_cat = hover_axy = -1;
      rectsel = false;
    }
    if (dispmode == DISP_APPLIED) {
      fit();
    }
    initial_calculated = 0;
  }
  bool show_calculated = (dispmode == DISP_REFINED && IsKeyDown(KEY_TAB));
  if (initial_calculated == 2 && show_calculated) initial_calculated = 1;
  else if (initial_calculated == 1 && !show_calculated) initial_calculated = 0;
  show_calculated |= initial_calculated;

  // Pointer position
  Vector2 p = GetMousePosition();
  p = iscale(p.x, p.y);

  // Selection
  if (dispmode == DISP_REFINED) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
        IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
      initial_calculated = 0;
    if (!rectsel) {
      #define dist_sq(_x, _y) \
        ((p.x - (_x)) * (p.x - (_x)) + (p.y - (_y)) * (p.y - (_y)))
      double dsq_best = 100 / (sc * sc);
      if (sel_cat == -1) {
        hover_cat = -1;
        for (long i = 0; i < nr_cat; i++) {
          double dsq = dist_sq(cat_x(i), cat_y(i));
          if (dsq < dsq_best) { dsq_best = dsq; hover_cat = i; }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          sel_cat = hover_cat;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
          refi_clear_cat(hover_cat);
          save();
        }
      } else {
        hover_axy = -1;
        for (long i = 0; i < nr_axy && i < axy_limit; i++) {
          double dsq = dist_sq(axy_x(i), axy_y(i));
          if (dsq < dsq_best) { dsq_best = dsq; hover_axy = i; }
        }
        if (IsMouseButtonUp(MOUSE_BUTTON_LEFT)) {
          if (hover_axy != -1) {
            refi_match(sel_cat, hover_axy);
            save();
          }
          sel_cat = hover_cat = hover_axy = -1;
        }
      }
      #undef dist_sq
    }
    if (!rectsel &&
        (sel_cat == -1 || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_LEFT_CONTROL)) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      sel_cat = hover_cat = -1;
      rectsel = true;
      rectx = p.x;
      recty = p.y;
      rectremove = IsKeyDown(KEY_LEFT_CONTROL);
    } else if (rectsel) {
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        rectsel = false;
        for (long i = 0; i < nr_cat; i++) {
          if (between(cat_x(i), rectx, p.x) &&
              between(cat_y(i), recty, p.y)) {
            if (rectremove) {
              refi_clear_cat(i);
            } else if (corr_id[i] != -1 &&
                corr_axyid(corr_id[i]) < axy_limit &&
                between(axy_x(corr_axyid(corr_id[i])), rectx, p.x) &&
                between(axy_y(corr_axyid(corr_id[i])), recty, p.y)) {
              refi_match(i, corr_axyid(corr_id[i]));
            }
          }
        }
        save();
      }
    }
  }

  // Draw

  BeginDrawing();

  DrawTextureEx(itex, (Vector2){-offx * sc + scrw/2, -offy * sc + scrh/2}, 0, sc, WHITE);

  // solve-field.c: plot_index_overlay
  if (show_calculated) {
    for (long i = 0; i < nr_corr; i++) {
      if (corr_axyid(i) >= axy_limit)
        DrawRing(
          scale(axy_x(corr_axyid(i)), axy_y(corr_axyid(i))),
          4, 6, 0, 360, 12, PURPLE);
    }
    for (long i = 0; i < nr_axy && i < axy_limit; i++) {
      DrawRing(scale(axy_x(i), axy_y(i)),
        4, 6, 0, 360, 12, axy_matched[i] ? ORANGE : RED);
    }
    for (long i = 0; i < nr_cat; i++) {
      DrawRing(scale(cat_x(i), cat_y(i)),
        2, 4, 0, 360, 12, corr_id[i] != -1 ? GREEN : LIME);
      if (corr_id[i] != -1)
        DrawLineEx(
          scale(cat_x(i), cat_y(i)),
          scale(axy_x(corr_axyid(corr_id[i])), axy_y(corr_axyid(corr_id[i]))),
          2, GREEN);
    }
  } else if (dispmode == DISP_REFINED) {
    for (long i = 0; i < nr_axy && i < axy_limit; i++) {
      DrawRing(scale(axy_x(i), axy_y(i)),
        4, 6, 0, 360, 12,
        i == hover_axy ?
          WHITE : (refi_axy_match[i] != -1 ? ORANGE : RED));
    }
    for (long i = 0; i < nr_cat; i++) {
      DrawRing(scale(cat_x(i), cat_y(i)),
        2, 4, 0, 360, 12,
        i == hover_cat || i == sel_cat ?
          WHITE : (refi_cat_match[i] != -1 ? GREEN : LIME));
      if (refi_cat_match[i] != -1)
        DrawLineEx(
          scale(cat_x(i), cat_y(i)),
          scale(axy_x(refi_cat_match[i]), axy_y(refi_cat_match[i])),
          2, GREEN);
      if (i == sel_cat)
        DrawLineEx(
          scale(cat_x(i), cat_y(i)),
          scale(p.x, p.y),
          2, GREEN);
    }
    if (rectsel) {
      float rw = fabsf(rectx - p.x);
      float rh = fabsf(recty - p.y);
      Vector2 rpos = scale((rectx + p.x - rw) / 2, (recty + p.y - rh) / 2);
      DrawRectangleLinesEx((Rectangle){
        rpos.x, rpos.y,
        rw * sc, rh * sc
      }, 2, rectremove ? BEIGE : WHITE);
    }
  } else if (dispmode == DISP_APPLIED) {
    // Grid
    for (int i = 0; i < grid_ra_ngroups; i++) {
      for (int j = 1; j < GRID_SUBDIV; j++)
        DrawLineEx(scale(
          grid_ra_applied[(i * GRID_SUBDIV + (j - 0)) * 2 + 0] * iw,
          grid_ra_applied[(i * GRID_SUBDIV + (j - 0)) * 2 + 1] * ih
        ), scale(
          grid_ra_applied[(i * GRID_SUBDIV + (j - 1)) * 2 + 0] * iw,
          grid_ra_applied[(i * GRID_SUBDIV + (j - 1)) * 2 + 1] * ih
        ), 2, Fade(GRAY, 0.5));
    }
    for (int i = 0; i < grid_dec_ngroups; i++) {
      for (int j = 1; j < GRID_SUBDIV; j++)
        DrawLineEx(scale(
          grid_dec_applied[(i * GRID_SUBDIV + (j - 0)) * 2 + 0] * iw,
          grid_dec_applied[(i * GRID_SUBDIV + (j - 0)) * 2 + 1] * ih
        ), scale(
          grid_dec_applied[(i * GRID_SUBDIV + (j - 1)) * 2 + 0] * iw,
          grid_dec_applied[(i * GRID_SUBDIV + (j - 1)) * 2 + 1] * ih
        ), 2, Fade(GRAY, 0.5));
    }
    // Constellations
    constell_draw(iw, ih, scrw, scrh, sc, offx, offy);
    // Image objects
    for (long i = 0; i < nr_axy && i < axy_limit; i++) {
      DrawRing(scale(axy_x(i), axy_y(i)),
        4, 6, 0, 360, 12, refi_axy_match[i] != -1 ? ORANGE : RED);
      if (refi_axy_match[i] != -1)
        DrawLineEx(
          scale(axy_x(i), axy_y(i)),
          scale(applied[refi_axy_match[i] * 2 + 0] * iw,
                applied[refi_axy_match[i] * 2 + 1] * ih),
          2, GREEN);
    }
    // Catalogue objects
    for (long i = 0; i < nr_cat; i++) {
      DrawRing(scale(applied[i * 2 + 0] * iw, applied[i * 2 + 1] * ih),
        2, 4, 0, 360, 12,
        refi_cat_match[i] != -1 ? GREEN : LIME);
    }
  }

  EndDrawing();
}

int main(int argc, char *argv[])
{
  if (argc < 9) {
    printf("Usage: %s <image> <objs FITS (axy)> "
      "<catalogue FITS (rdls)> <catalogue FITS (xyls)> "
      "<link FITS (corr)> "
      "<geometry FITS (wcs)> "
      "<save/load path> "
      "<coefficients save path>\n", argv[0]);
    return 0;
  }

  SetTraceLogLevel(LOG_WARNING);

  // Load image
  Image img = LoadImage(argv[1]);
  iw = img.width;
  ih = img.height;
  if (iw == 0) {
    printf("Cannot open image\n");
    return 1;
  }

  float scx = (float)1080 / iw;
  float scy = (float)720 / ih;
  sc = sc_base = (scx < scy ? scx : scy);
  scrw = iw * sc; // Round down to avoid black borders
  scrh = ih * sc;
  offx = offy = 0;

  // Read FITS tables
  if ((data_axy = read_fits_table(argv[2], col_names_axy, &nr_axy)) == NULL) {
    printf("Error loading the objects table\n");
    return 1;
  }
  if ((data_rdls = read_fits_table(argv[3], col_names_rdls, &nr_rdls)) == NULL) {
    printf("Error loading the catalogue table (RA, Dec)\n");
    return 1;
  }
  if ((data_xyls = read_fits_table(argv[4], col_names_xyls, &nr_xyls)) == NULL) {
    printf("Error loading the catalogue table (X, Y)\n");
    return 1;
  }
  if ((data_corr = read_fits_table(argv[5], col_names_corr, &nr_corr)) == NULL) {
    printf("Error loading the correlation table\n");
    return 1;
  }

  if (nr_rdls != nr_xyls) {
    printf("Different number of rows in RA-Dec and X-Y catalogue tables (%ld and %ld)\n",
      nr_rdls, nr_xyls);
    return 1;
  }
  double wcs_header_values[4];
  if (read_fits_headers(argv[6], header_names_wcs, wcs_header_values) == 0) {
    printf("Error loading the geometry (WCS) metadata\n");
    return 1;
  }
  // XXX: CRPIX1 and CRPIX2 values are unused. Possible?
  view_ra = wcs_header_values[0];
  view_dec = wcs_header_values[1];

  // Graphics setup
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(scrw, scrh, NULL);
  SetTargetFPS(60);

  itex = LoadTextureFromImage(img);

  // Auxiliary data initialization
  save_load_path = argv[7];
  axy_limit = probe_axy_limit();
  refi_axy_match = (int *)malloc(sizeof(int) * axy_limit);
  memset(refi_axy_match, -1, sizeof(int) * axy_limit);
  refi_cat_match = (int *)malloc(sizeof(int) * nr_cat);
  memset(refi_cat_match, -1, sizeof(int) * nr_cat);
  load();

  axy_matched = (bool *)malloc(sizeof(bool) * axy_limit);
  memset(axy_matched, 0, sizeof(bool) * axy_limit);
  for (long i = 0; i < nr_corr; i++) {
    if (corr_axyid(i) < axy_limit) axy_matched[corr_axyid(i)] = true;
  }
  corr_id = (int *)malloc(sizeof(int) * nr_cat);
  memset(corr_id, -1, sizeof(int) * nr_cat);
  for (long i = 0; i < nr_corr; i++) corr_id[corr_catid(i)] = i;

  coeff_path = argv[8];
  FILE *fp_coeff = fopen(coeff_path, "r");
  if (fp_coeff) {
    int saved_ord;
    fscanf(fp_coeff, "%d", &saved_ord);
    if (saved_ord != ord && saved_ord > 0 && saved_ord <= MAX_ORD) {
      ord = saved_ord;
      printf("Order set to %d\n", ord);
    }
    fclose(fp_coeff);
  }

  constell_load();

  while (!WindowShouldClose()) {
    update_and_draw();
  }

  CloseWindow();

  return 0;
}


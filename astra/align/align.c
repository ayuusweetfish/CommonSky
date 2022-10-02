#include "raylib.h"
#include "fitsio.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

double *read_fits_table(const char *path, const char **colnames, long *count);

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
  float sc = (scx < scy ? scx : scy);
  int scrw = iw * sc; // Round down to avoid black borders
  int scrh = ih * sc;
  #define scale(_x, _y) (Vector2){(_x) * sc, (_y) * sc}

  // Read FITS tables

  const char *col_names_axy[] = {"X", "Y", NULL};
  long nr_axy;
  double *data_axy = read_fits_table(argv[2], col_names_axy, &nr_axy);
  if (data_axy == NULL) {
    printf("Error loading the objects table\n");
    return 1;
  }
  #define axy_x(_i) data_axy[(_i) * 2 + 0]
  #define axy_y(_i) data_axy[(_i) * 2 + 1]

  const char *col_names_corr[] = {"index_x", "index_y", "field_id", "index_ra", "index_dec", NULL};
  long nr_corr;
  double *data_corr = read_fits_table(argv[3], col_names_corr, &nr_corr);
  if (data_corr == NULL) {
    printf("Error loading the correlation table\n");
    return 1;
  }
  #define corr_x(_i)      data_corr[(_i) * 5 + 0]
  #define corr_y(_i)      data_corr[(_i) * 5 + 1]
  // #define corr_catid(_i)  (long)data_corr[(_i) * 4 + 3]
  #define corr_axyid(_i)  (long)data_corr[(_i) * 5 + 2]
  #define corr_ra(_i)     data_corr[(_i) * 5 + 3]
  #define corr_dec(_i)      data_corr[(_i) * 5 + 4]

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(scrw, scrh, NULL);
  SetTargetFPS(60);

  Texture2D itex = LoadTextureFromImage(img);

  const int AXY_LIMIT = 200;
  bool axy_mapped[AXY_LIMIT] = { false };
  for (long i = 0; i < nr_corr; i++) {
    if (corr_axyid(i) < AXY_LIMIT) axy_mapped[corr_axyid(i)] = true;
  }

  while (!WindowShouldClose()) {
    BeginDrawing();

    DrawTextureEx(itex, (Vector2){0, 0}, 0, sc, WHITE);

    // solve-field.c: plot_index_overlay
    for (long i = 0; i < nr_corr; i++) {
      if (corr_axyid(i) >= AXY_LIMIT)
        DrawRing(
          scale(axy_x(corr_axyid(i)), axy_y(corr_axyid(i))),
          4, 6, 0, 360, 12, PURPLE);
    }
    for (long i = 0; i < nr_axy && i < AXY_LIMIT; i++) {
      DrawRing(scale(axy_x(i), axy_y(i)),
        4, 6, 0, 360, 12, axy_mapped[i] ? ORANGE : RED);
    }
    for (long i = 0; i < nr_corr; i++) {
      DrawRing(scale(corr_x(i), corr_y(i)),
        2, 4, 0, 360, 12, GREEN);
      DrawLineEx(
        scale(corr_x(i), corr_y(i)),
        scale(axy_x(corr_axyid(i)), axy_y(corr_axyid(i))),
        2, GREEN);
    }

    EndDrawing();
  }

  CloseWindow();

  return 0;
}

double *read_fits_table(const char *path, const char **colnames, long *count)
{
  fitsfile *ffp;
  int fstatus = 0;
  if (fits_open_file(&ffp, path, READONLY, &fstatus) != 0) {
    printf("Cannot open FITS file -- ");
    fits_report_error(stdout, fstatus);
    return NULL;
  }

  int hdunum, hdutype;
  if (fits_get_hdu_num(ffp, &hdunum) == 1)
    fits_movabs_hdu(ffp, 2, &hdutype, &fstatus);
  else 
    fits_get_hdu_type(ffp, &hdutype, &fstatus);

  if (hdutype == IMAGE_HDU) {
    printf("Expected tables, but FITS file contains images\n");
    return NULL;
  }

  int nc_req = 0;
  while (colnames[nc_req] != NULL) nc_req++;

  long nr;
  int nc;
  fits_get_num_rows(ffp, &nr, &fstatus);
  fits_get_num_cols(ffp, &nc, &fstatus);

  int *colnums = (int *)malloc(nc_req * sizeof(int));
  memset(colnums, -1, nc_req * sizeof(int));

  char keyword[FLEN_KEYWORD];
  char colname[FLEN_VALUE];
  for (int i = 1; i <= nc; i++) {
    fits_make_keyn("TTYPE", i, keyword, &fstatus);
    fits_read_key(ffp, TSTRING, keyword, colname, NULL, &fstatus);
    for (int j = 0; j < nc_req; j++)
      if (strcmp(colname, colnames[j]) == 0) {
        colnums[j] = i;
        break;
      }
  }
  for (int i = 0; i < nc_req; i++)
    if (colnums[i] == -1) {
      printf("Column \"%s\" not found\n", colnames[i]);
      return NULL;
    }

  double *rec = (double *)malloc(nr * nc_req * sizeof(double));

  for (long i = 1; i <= nr; i++) {
    double val;
    int anynul; // Unused
    for (int j = 0; j < nc_req; j++) {
      if (fits_read_col_dbl(ffp, colnums[j], i,
          1, 1, 0, &val, &anynul, &fstatus) != 0)
        break;
      rec[(i - 1) * nc_req + j] = val;
    }
    if (fstatus != 0) {
      printf("Error during table read -- ");
      fits_report_error(stdout, fstatus);
      return NULL;
    }
  }

  free(colnums);
  *count = nr;
  return rec;
}

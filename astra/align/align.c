#include "raylib.h"
#include "fitsio.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("Usage: %s <image> <corr FITS>\n", argv[0]);
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

  float scx = (float)800 / iw;
  float scy = (float)640 / ih;
  float sc = (scx < scy ? scx : scy);
  int scrw = iw * sc; // Round down to avoid black borders
  int scrh = ih * sc;

  // Read FITS table

  const char *col_names[] = {
    "field_x", "field_y", "field_ra", "field_dec",
    "index_x", "index_y", "index_ra", "index_dec",
    "index_id", "field_id", "match_weight",
    "FLUX", "BACKGROUND"
  };
  typedef struct record {
    float field_x, field_y;
    float index_x, index_y;
    float index_ra, index_dec;
  } record;

  fitsfile *ffp;
  int fstatus = 0;
  if (fits_open_file(&ffp, argv[2], READONLY, &fstatus) != 0) {
    printf("Cannot open FITS file -- ");
    fits_report_error(stdout, fstatus);
    return 1;
  }

  int hdunum, hdutype;
  if (fits_get_hdu_num(ffp, &hdunum) == 1)
    fits_movabs_hdu(ffp, 2, &hdutype, &fstatus);
  else 
    fits_get_hdu_type(ffp, &hdutype, &fstatus);

  if (hdutype == IMAGE_HDU) {
    printf("Expected tables, but FITS file contains images\n");
    return 1;
  }

  long nr;
  int nc;
  const int NC = sizeof col_names / sizeof col_names[0];
  fits_get_num_rows(ffp, &nr, &fstatus);
  fits_get_num_cols(ffp, &nc, &fstatus);
  if (nc != NC) {
    printf("Expected %d columns, but contains %d\n", NC, nc);
    return 1;
  }
  char keyword[FLEN_KEYWORD];
  char colname[FLEN_VALUE];
  for (int i = 1; i <= nc; i++) {
    fits_make_keyn("TTYPE", i, keyword, &fstatus);
    fits_read_key(ffp, TSTRING, keyword, colname, NULL, &fstatus);
    if (strcmp(colname, col_names[i - 1]) != 0) {
      printf("Column %d should be named \"%s\", but the file contains \"%s\"",
        i, col_names[i - 1], colname);
      return 1;
    }
  }

  record *rec = (record *)malloc(nr * sizeof(record));

  for (int i = 1; i <= nr; i++) {
    double val;
    int anynul; // Unused
    for (int j = 1; j <= nc; j++) {
      if (fits_read_col_dbl(ffp, j, i, 1, 1, 0, &val, &anynul, &fstatus) != 0)
        break;
      switch (j) {
        case 1: rec[i - 1].field_x = val; break;
        case 2: rec[i - 1].field_y = val; break;
        case 5: rec[i - 1].index_x = val; break;
        case 6: rec[i - 1].index_y = val; break;
        case 7: rec[i - 1].index_ra = val; break;
        case 8: rec[i - 1].index_dec = val; break;
        default: break;
      }
    }
    if (fstatus != 0) {
      printf("Error during table read -- ");
      fits_report_error(stdout, fstatus);
      return 1;
    }
  }

  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(scrw, scrh, NULL);
  SetTargetFPS(60);

  Texture2D itex = LoadTextureFromImage(img);

  while (!WindowShouldClose()) {
    BeginDrawing();

    DrawTextureEx(itex, (Vector2){0, 0}, 0, sc, WHITE);

    for (int i = 0; i < nr; i++) {
      DrawRing((Vector2){rec[i].field_x * sc, rec[i].field_y * sc},
        5, 7, 0, 360, 24, RED);
    }
    for (int i = 0; i < nr; i++) {
      DrawRing((Vector2){rec[i].index_x * sc, rec[i].index_y * sc},
        3, 5, 0, 360, 24, GREEN);
      DrawLineEx(
        (Vector2){rec[i].index_x * sc, rec[i].index_y * sc},
        (Vector2){rec[i].field_x * sc, rec[i].field_y * sc},
        2, GREEN);
    }

    EndDrawing();
  }

  CloseWindow();

  return 0;
}

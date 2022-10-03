#include "fitsio.h"
#include <stdio.h>
#include <string.h>

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

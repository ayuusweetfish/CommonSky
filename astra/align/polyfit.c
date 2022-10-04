#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C (n*m) = A^-1 (n*n) B (n*m)
static void invert_mul(int n, int m, double *a, double *b, double *c);

static inline void stereo_proj(
  double view_ra, double view_dec,
  double ra, double dec,
  double *o_x, double *o_y)
{
  ra *= (M_PI / 180);
  dec *= (M_PI / 180);
  double x = cos(dec) * cos(ra);
  double y = cos(dec) * sin(ra);
  double z = sin(dec);
  // Move target to (0, 0, -1)
  // by rotating around Z axis first (to RA=0), and then X axis (to Dec=-90deg)
  double rot_ra = -view_ra * (M_PI / 180);
  double rot_dec = (-90 - view_dec) * (M_PI / 180);
  double x0, y0, z0;
  // Around Z
  x0 = x; y0 = y;
  x = x0 * cos(rot_ra) - y0 * sin(rot_ra);
  y = x0 * sin(rot_ra) + y0 * cos(rot_ra);
  // Around X
  y0 = y; z0 = z;
  y = y0 * cos(rot_dec) - z0 * sin(rot_dec);
  z = y0 * sin(rot_dec) + z0 * cos(rot_dec);
  // Stereographic projection
  *o_x = x / (1 - z);
  *o_y = y / (1 - z);
}

// For 0<=k<n, 0<=c<=1:
// v[2k+c] =
//  let ux = u[2k+0], uy = u[2k+1]
//  sum_{i,j>=0, i+j<=ord} C[c,i,j] ux^i uy^j
// C[c,i,j] = o_coeff[c * (ord+1)*(ord+2)/2 + (i*(ord+1) + j - i*(i-1)/2)]

#define id(_i, _j) ((_i)*(ord+1) + (_j) - (_i)*((_i)-1)/2)
#define C(_c, _i, _j) \
  COEFF[(_c) * (ord+1)*(ord+2)/2 + id((_i), (_j))]

#define COEFF o_coeff
void polyfit(int n, double *u, double *v, double view_ra, double view_dec, int ord, double *o_coeff)
{
  int n_coeffs = (ord + 1) * (ord + 2) / 2;
  // Initial guess: LLS
  // XB = Y
  // X: n * n_coeffs (u)
  // Y: n * 2 (v)
  // B: n_coeffs * 2 
  // B = inv(X^T X) * X^T * Y
  double *XTX = (double *)malloc(sizeof(double) * n_coeffs * n_coeffs);
  double *uxpow = (double *)malloc(sizeof(double) * (ord + 1));
  double *uypow = (double *)malloc(sizeof(double) * (ord + 1));
  memset(XTX, 0, sizeof(double) * n_coeffs * n_coeffs);
  uxpow[0] = uypow[0] = 1;
  for (int i = 0; i < n; i++) {
    double x, y;
    stereo_proj(view_ra, view_dec, u[i * 2 + 0], u[i * 2 + 1], &x, &y);
    for (int j = 1; j <= ord; j++) {
      uxpow[j] = uxpow[j - 1] * x;
      uypow[j] = uypow[j - 1] * y;
    }
    for (int xp1 = 0; xp1 <= ord; xp1++)
    for (int yp1 = 0; yp1 <= ord - xp1; yp1++) {
      double val1 = uxpow[xp1] * uypow[yp1];
      for (int xp2 = 0; xp2 <= ord; xp2++)
      for (int yp2 = 0; yp2 <= ord - xp2; yp2++) {
        double val2 = uxpow[xp2] * uypow[yp2];
        XTX[id(xp1, yp1) * n_coeffs + id(xp2, yp2)]
          += val1 * val2;
      }
    }
  }
  // X^T * Y
  double *XTY = (double *)malloc(sizeof(double) * n_coeffs * 2);
  memset(XTY, 0, sizeof(double) * n_coeffs * 2);
  for (int i = 0; i < n; i++) {
    double x, y;
    stereo_proj(view_ra, view_dec, u[i * 2 + 0], u[i * 2 + 1], &x, &y);
    for (int j = 1; j <= ord; j++) {
      uxpow[j] = uxpow[j - 1] * x;
      uypow[j] = uypow[j - 1] * y;
    }
    for (int xp1 = 0; xp1 <= ord; xp1++)
    for (int yp1 = 0; yp1 <= ord - xp1; yp1++) {
      int j = id(xp1, yp1);
      for (int k = 0; k < 2; k++)
        XTY[j * 2 + k] += uxpow[xp1] * uypow[yp1] * v[i * 2 + k];
    }
  }
  // Invert and multiply
  double LLS_sol[n_coeffs][2];
  invert_mul(n_coeffs, 2, &XTX[0], &XTY[0], &LLS_sol[0][0]);
  // Copy to coefficients as a starting point
  for (int c = 0; c < 2; c++) {
    for (int xp1 = 0; xp1 <= ord; xp1++)
    for (int yp1 = 0; yp1 <= ord - xp1; yp1++) {
      C(c, xp1, yp1) = LLS_sol[id(xp1, yp1)][c];
    }
  }

  free(XTX);
  free(uxpow);
  free(uypow);
  free(XTY);
}

#undef COEFF
#define COEFF coeff
void polyapply(int n, double *u, double view_ra, double view_dec, int ord, double *coeff)
{
  double uxpow[ord + 1], uypow[ord + 1];
  uxpow[0] = uypow[0] = 1;
  for (int k = 0; k < n; k++) {
    double x, y;
    stereo_proj(view_ra, view_dec, u[k * 2 + 0], u[k * 2 + 1], &x, &y);
    for (int i = 1; i <= ord; i++) {
      uxpow[i] = uxpow[i - 1] * x;
      uypow[i] = uypow[i - 1] * y;
    }
    for (int c = 0; c <= 1; c++) {
      double sum = 0;
      for (int i = 0; i <= ord; i++)
        for (int j = 0; j <= ord - i; j++) {
          sum += uxpow[i] * uypow[j] * C(c, i, j);
        }
      u[k * 2 + c] = sum;
    }
  }
}

static void invert_mul(int n, int m, double *a, double *b, double *c)
{
  double *aux = (double *)malloc(sizeof(double) * n * (n + m));
  #define A(_i, _j) aux[(_i) * (n + m) + (_j)]

  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++)
      A(i, j) = a[i * n + j];
    for (int j = 0; j < m; j++)
      A(i, j + n) = b[i * m + j];
  }

  for (int i = 0; i < n; i++) {
    int pivot = i;
    double maxabs = fabs(A(i, i));
    for (int j = i + 1; j < n; j++)
      if (maxabs < fabs(A(j, i))) {
        maxabs = fabs(A(j, i));
        pivot = j;
      }
    if (pivot != i)
      for (int j = i; j < n + m; j++) {
        double t = A(i, j);
        A(i, j) = A(pivot, j);
        A(pivot, j) = t;
      }
    for (int j = i + 1; j < n; j++) {
      double factor = A(j, i) / A(i, i);
      for (int k = i; k < n + m; k++)
        A(j, k) -= factor * A(i, k);
    }
  }
  for (int i = n - 1; i >= 0; i--) {
    for (int j = i + 1; j < n + m; j++)
      A(i, j) /= A(i, i);
    A(i, i) = 1;
    for (int j = 0; j < i; j++)
      for (int k = n + m - 1; k >= i; k--)
        A(j, k) -= A(j, i) * A(i, k);
  }

  for (int i = 0; i < n; i++)
    for (int j = 0; j < m; j++)
      c[i * m + j] = A(i, j + n);

  #undef A
  free(aux);
}

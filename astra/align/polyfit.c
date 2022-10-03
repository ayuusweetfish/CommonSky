#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// C (n*m) = A^-1 (n*n) B (n*m)
static void invert_mul(int n, int m, double *a, double *b, double *c);

// For 0<=k<n, 0<=c<=1:
// v[2k+c] =
//  let ux = u[2k+0], uy = u[2k+1]
//  sum_{i,j>=0, i+j<=ord} C[c,i,j] ux^i uy^j
// C[c,i,j] = o_coeff[c * (ord+1)*(ord+2)/2 + (i*(ord+1) + j - i*(i-1)/2)]

#define C(_c, _i, _j) \
  COEFF[(_c) * (ord+1)*(ord+2)/2 + ((_i)*(ord+1) + (_j) - (_i)*((_i)-1)/2)]

#define COEFF o_coeff
void polyfit(int n, double *u, double *v, int ord, double *o_coeff)
{
  // Initial guess: LLS
  // XB = Y
  // X: n * 3 (u)
  // Y: n * 2 (v)
  // B: 3 * 2 
  // B = inv(X^T X) * X^T * Y
  double XTX[3][3] = {{ 0 }};
  for (int i = 0; i < n; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 3; k++)
        XTX[j][k] +=
          (j == 2 ? 1 : u[i * 2 + j]) *
          (k == 2 ? 1 : u[i * 2 + k]);
  // X^T * Y
  double XTY[3][2] = {{ 0 }};
  for (int i = 0; i < n; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        XTY[j][k] += (j == 2 ? 1 : u[i * 2 + j]) * v[i * 2 + k];
  // Invert and multiply
  double LLS_sol[3][2];
  invert_mul(3, 2, &XTX[0][0], &XTY[0][0], &LLS_sol[0][0]);
  // Copy to coefficients as a starting point
  for (int c = 0; c < 2; c++) {
    C(c, 0, 0) = LLS_sol[2][c];
    C(c, 1, 0) = LLS_sol[0][c];
    C(c, 0, 1) = LLS_sol[1][c];
  }
}

#undef COEFF
#define COEFF coeff
void polyapply(int n, double *u, double *o_v, int ord, double *coeff)
{
  double uxpow[ord + 1], uypow[ord + 1];
  uxpow[0] = uypow[0] = 1;
  for (int k = 0; k < n; k++) {
    double ux = u[k * 2 + 0], uy = u[k * 2 + 1];
    for (int i = 1; i <= ord; i++) {
      uxpow[i] = uxpow[i - 1] * ux;
      uypow[i] = uypow[i - 1] * uy;
    }
    for (int c = 0; c <= 1; c++) {
      double sum = 0;
      for (int i = 0; i <= ord; i++)
        for (int j = 0; j <= ord - i; j++) {
          sum += uxpow[i] * uypow[j] * C(c, i, j);
        }
      o_v[k * 2 + c] = sum;
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

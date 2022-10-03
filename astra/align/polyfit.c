#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
  // Invert
  #define a11 XTX[0][0]
  #define a12 XTX[0][1]
  #define a13 XTX[0][2]
  #define a21 XTX[1][0]
  #define a22 XTX[1][1]
  #define a23 XTX[1][2]
  #define a31 XTX[2][0]
  #define a32 XTX[2][1]
  #define a33 XTX[2][2]
  double d =
      a11 * (a22 * a33 - a32 * a23)
    - a21 * (a12 * a33 - a13 * a32)
    + a31 * (a12 * a23 - a13 * a22);
  if (fabs(d) <= 1e-12) {
    printf("Non-invertible matrix (X^T X)!");
    exit(1);
  }
  double invXTX[3][3] = {
    { (a22*a33 - a32*a23)/d, -(a12*a33 - a13*a32)/d,  (a12*a23 - a13*a22)/d},
    {-(a21*a33 - a23*a31)/d,  (a11*a33 - a13*a31)/d, -(a11*a23 - a13*a21)/d},
    { (a21*a32 - a22*a31)/d, -(a11*a32 - a12*a31)/d,  (a11*a22 - a12*a21)/d},
  };
  // X^T * Y
  double XTY[3][2] = {{ 0 }};
  for (int i = 0; i < n; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        XTY[j][k] += (j == 2 ? 1 : u[i * 2 + j]) * v[i * 2 + k];
  // Multiply
  double LLS_sol[3][2] = {{ 0 }};
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 2; j++)
      for (int k = 0; k < 3; k++)
        LLS_sol[i][j] += invXTX[i][k] * XTY[k][j];
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

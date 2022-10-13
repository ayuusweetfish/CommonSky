#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"

typedef struct { float x, y, z; } vec3;

#include "../disp/constelldb.h"

void polyapply(int n, double *u, double view_ra, double view_dec, int ord, double *coeff);

void constell_load()
{
  load_constelldb("../disp/constell/hip2_j2000.dat",
    "../disp/constell/constellationship.fab");
}

static Vector2 *scrlines = NULL;
static size_t n_scrlines, cap_scrlines = 0;

static const int SUBDIV = 4;

void constell_prepare(
  double ra_min, double ra_max,
  double dec_min, double dec_max,
  double view_ra, double view_dec, int ord, double *coeff)
{
  n_scrlines = 0;
  for (int i = 0; i < n_constell; i++) {
    for (int j = 0; j < cons[i].n_lines; j++) {
      float raA = hip[cons[i].pts[j * 2 + 0]].ra;
      float decA = hip[cons[i].pts[j * 2 + 0]].dec;
      float raB = hip[cons[i].pts[j * 2 + 1]].ra;
      float decB = hip[cons[i].pts[j * 2 + 1]].dec;
      if (raA * (180/M_PI) >= ra_min && raA * (180/M_PI) <= ra_max &&
          decA * (180/M_PI) >= dec_min && decA * (180/M_PI) <= dec_max &&
          raB * (180/M_PI) >= ra_min && raB * (180/M_PI) <= ra_max &&
          decB * (180/M_PI) >= dec_min && decB * (180/M_PI) <= dec_max) {
        double xA = cos(raA) * cos(decA), yA = sin(raA) * cos(decA), zA = sin(decA);
        double xB = cos(raB) * cos(decB), yB = sin(raB) * cos(decB), zB = sin(decB);
        double O = acos(xA * xB + yA * yB + zA * zB);
        double u[(SUBDIV + 1) * 2];
        for (int k = 0; k <= SUBDIV; k++) {
          double t = (double)k / SUBDIV;
          // Slerp(A, B, t)
          double kA = sin((1 - t) * O) / sin(O);
          double kB = sin(t * O) / sin(O);
          double xC = xA * kA + xB * kB;
          double yC = yA * kA + yB * kB;
          double zC = zA * kA + zB * kB;
          u[k * 2 + 0] = atan2(yC, xC) * (180/M_PI);
          u[k * 2 + 1] = asin(zC) * (180/M_PI);
        }
        polyapply(SUBDIV + 1, u, view_ra, view_dec, ord, coeff);
        if (n_scrlines + (SUBDIV + 1) > cap_scrlines) {
          cap_scrlines = (cap_scrlines == 0 ? 32 : cap_scrlines * 2);
          scrlines = (Vector2 *)realloc(scrlines, sizeof(Vector2) * cap_scrlines);
        }
        for (int k = 0; k <= SUBDIV; k++)
          scrlines[n_scrlines++] = (Vector2){u[k * 2 + 0], u[k * 2 + 1]};
      }
    }
  }
  //printf("%zu %.4lf %.4lf\n", n_scrlines, scrlines[0].x, scrlines[0].y);
}

void constell_draw(int iw, int ih, int scrw, int scrh, float sc, float offx, float offy)
{
  // rlSetLineWidth(2);
  for (int i = 0; i < n_scrlines; i += (SUBDIV + 1)) {
    // DrawLineStrip(&scrlines[i], SUBDIV + 1, Fade(WHITE, 0.2));
    for (int j = 0; j < SUBDIV; j++) {
      Vector2 pA = (Vector2){
        (scrlines[i + j + 0].x * iw - offx) * sc + scrw / 2,
        (scrlines[i + j + 0].y * ih - offy) * sc + scrh / 2,
      };
      Vector2 pB = (Vector2){
        (scrlines[i + j + 1].x * iw - offx) * sc + scrw / 2,
        (scrlines[i + j + 1].y * ih - offy) * sc + scrh / 2,
      };
      DrawLineEx(pA, pB, 2, Fade(YELLOW, 0.2));
    }
  }
}

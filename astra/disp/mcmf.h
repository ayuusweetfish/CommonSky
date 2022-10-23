#ifndef MCMF_H
#define MCMF_H

// Minimum-cost maximum-flow

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int mcmf_n;

typedef struct mcmf_edge {
  int dest, next;
  int cap;
  float cost;
} mcmf_edge;

static mcmf_edge *mcmf_e = NULL;
static int mcmf_e_num, mcmf_e_cap;
static int *mcmf_e_start = NULL;

static float *mcmf_d = NULL;
static int *mcmf_pred = NULL;
static int *mcmf_q = NULL;
static bool *mcmf_in_q = NULL;

static inline void mcmf_init(int n)
{
  mcmf_n = n;
  mcmf_e_num = 0;
  mcmf_e_cap = 32;
  mcmf_e = (mcmf_edge *)realloc(mcmf_e, sizeof(mcmf_edge) * mcmf_e_cap);
  mcmf_e_start = (int *)realloc(mcmf_e_start, sizeof(int) * n);
  memset(mcmf_e_start, -1, sizeof(int) * n);

  mcmf_d = (float *)realloc(mcmf_d, sizeof(float) * n);
  mcmf_pred = (int *)realloc(mcmf_pred, sizeof(int) * n);
  mcmf_q = (int *)realloc(mcmf_q, sizeof(int) * n);
  mcmf_in_q = (bool *)realloc(mcmf_in_q, sizeof(bool) * n);
}

#define n mcmf_n
#define e mcmf_e
#define e_start mcmf_e_start

static inline int mcmf_link(int u, int v, int c, float w)
{
  if (mcmf_e_num + 2 > mcmf_e_cap) {
    mcmf_e_cap <<= 1;
    e = (mcmf_edge *)realloc(e, sizeof(mcmf_edge) * mcmf_e_cap);
  }
  int fwd = mcmf_e_num + 0;
  int bwd = mcmf_e_num + 1;
  e[fwd] = (mcmf_edge){v, e_start[u], c, +w};
  e[bwd] = (mcmf_edge){u, e_start[v], 0, -w};
  e_start[u] = fwd;
  e_start[v] = bwd;
  mcmf_e_num += 2;
  return fwd;
}

static inline void mcmf_sssp(int src)
{
  for (int i = 0; i < n; i++) mcmf_d[i] = INFINITY;
  // Bellman-Ford with queue optimization (SPFA)
  memset(mcmf_in_q, 0, sizeof(bool) * n);
#define q mcmf_q
  int qhead = 0, qtail = 0;
  mcmf_d[src] = 0;
  q[qtail] = src; mcmf_in_q[src] = true; qtail = (qtail + 1) % n;
  while (qhead != qtail || mcmf_in_q[0]) {
    int u = q[qhead]; qhead = (qhead + 1) % n;
    mcmf_in_q[u] = false;
    for (int x = e_start[u]; x != -1; x = e[x].next) if (e[x].cap > 0) {
      int v = e[x].dest;
      if (mcmf_d[v] > mcmf_d[u] + e[x].cost + 1e-7) {
        mcmf_d[v] = mcmf_d[u] + e[x].cost;
        mcmf_pred[v] = x;
        if (!mcmf_in_q[v]) {
          mcmf_in_q[v] = true;
          // SLF optimization
          if (qhead != qtail && mcmf_d[v] < mcmf_d[q[qhead]]) {
            qhead = (qhead - 1 + n) % n; q[qhead] = v;
          } else {
            q[qtail] = v; qtail = (qtail + 1) % n;
          }
        }
      }
    }
  }
#undef q
}

static inline int mcmf_solve(int src, int sink)
{
  int flow = 0;
  float cost = 0;
  while (1) {
    mcmf_sssp(src);
    if (isinf(mcmf_d[sink])) break;
    int curflow = INT_MAX;
    for (int u = sink; u != src; u = e[mcmf_pred[u] ^ 1].dest) {
      if (curflow > e[mcmf_pred[u]].cap)
        curflow = e[mcmf_pred[u]].cap;
    }
    for (int u = sink; u != src; u = e[mcmf_pred[u] ^ 1].dest) {
      e[mcmf_pred[u]].cap -= curflow;
      e[mcmf_pred[u] ^ 1].cap += curflow;
      cost += curflow * e[mcmf_pred[u]].cost;
    }
    flow += curflow;
  }
  return flow;
}

#undef n
#undef e
#undef e_start

#endif

// Cuckoo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2014 John Tromp

#include "cuckoo.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <set>

// ok for size up to 2^32
#define MAXPATHLEN 8192

class cuckoo_ctx {
public:
  siphash_ctx sip_ctx;
  nonce_t easiness;
  node_t *cuckoo;

  cuckoo_ctx(const char* header, nonce_t easy_ness) {
    setheader(&sip_ctx, header);
    easiness = easy_ness;
    assert(cuckoo = (node_t *)calloc(1+SIZE, sizeof(node_t)));
  }
  ~cuckoo_ctx() {
    free(cuckoo);
  }
};

int path(node_t *cuckoo, node_t u, node_t *us) {
  int nu;
  for (nu = 0; u; u = cuckoo[u]) {
    if (++nu >= MAXPATHLEN) {
      while (nu-- && us[nu] != u) ;
      if (nu < 0)
        printf("maximum path length exceeded\n");
      else printf("illegal % 4d-cycle\n", MAXPATHLEN-nu);
      exit(0);
    }
    us[nu] = u;
  }
  return nu;
}

typedef std::pair<node_t,node_t> edge;

void solution(cuckoo_ctx *ctx, node_t *us, int nu, node_t *vs, int nv) {
  std::set<edge> cycle;
  unsigned n;
  cycle.insert(edge(*us, *vs));
  while (nu--)
    cycle.insert(edge(us[(nu+1)&~1], us[nu|1])); // u's in even position; v's in odd
  while (nv--)
    cycle.insert(edge(vs[nv|1], vs[(nv+1)&~1])); // u's in odd position; v's in even
  printf("Solution");
  for (nonce_t nonce = n = 0; nonce < ctx->easiness; nonce++) {
    edge e(1+sipnode(&ctx->sip_ctx, nonce, 0), 1+HALFSIZE+sipnode(&ctx->sip_ctx, nonce, 1));
    if (cycle.find(e) != cycle.end()) {
      printf(" %lx", nonce);
      cycle.erase(e);
    }
  }
  printf("\n");
}

void worker(cuckoo_ctx *ctx) {
  node_t *cuckoo = ctx->cuckoo;
  node_t us[MAXPATHLEN], vs[MAXPATHLEN];
  for (node_t nonce = 0; nonce < ctx->easiness; nonce++) {
    node_t u0, v0;
    sipedge(&ctx->sip_ctx, nonce, &u0, &v0);
    u0 += 1        ;  // make non-zero
    v0 += 1 + HALFSIZE;  // make v's different from u's
    node_t u = cuckoo[u0], v = cuckoo[v0];
    if (u == v0 || v == u0)
      continue; // ignore duplicate edges
    us[0] = u0;
    vs[0] = v0;
#ifdef SHOW
    for (unsigned j=0; j<SIZE; j++)
      if (!cuckoo[1+j]) printf("%2d:   ",j);
      else            printf("%2d:%02ld ",j,cuckoo[1+j]-1);
    printf(" %lx (%ld,%ld)\n", nonce,*us-1,*vs-1);
#endif
    int nu = path(cuckoo, u, us), nv = path(cuckoo, v, vs);
    if (us[nu] == vs[nv]) {
      int min = nu < nv ? nu : nv;
      for (nu -= min, nv -= min; us[nu] != vs[nv]; nu++, nv++) ;
      int len = nu + nv + 1;
      printf("% 4d-cycle found at %d%%\n", len, (int)(nonce*100L/ctx->easiness));
      if (len == PROOFSIZE)
        solution(ctx, us, nu, vs, nv);
      continue;
    }
    if (nu < nv) {
      while (nu--)
        cuckoo[us[nu+1]] = us[nu];
      cuckoo[u0] = v0;
    } else {
      while (nv--)
        cuckoo[vs[nv+1]] = vs[nv];
      cuckoo[v0] = u0;
    }
  }
}

int main(int argc, char **argv) {
  const char *header = "";
  int c, easipct = 50;
  while ((c = getopt (argc, argv, "e:h:")) != -1) {
    switch (c) {
      case 'e':
        easipct = atoi(optarg);
        break;
      case 'h':
        header = optarg;
        break;
    }
  }
  assert(easipct >= 0 && easipct <= 100);
  printf("Looking for %d-cycle on cuckoo%d(\"%s\") with %d%% edges\n",
               PROOFSIZE, SIZESHIFT, header, easipct);
  u64 easiness = easipct * SIZE / 100;
  cuckoo_ctx ctx(header, easiness);
  worker(&ctx);
}

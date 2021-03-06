// C implementation of Heap Sort
#include <stdio.h>
#include <stdlib.h>

#include "patternManager.h"

Pattern* patternCache[0xFFFF];

#define CACHE_LIFETIME 10;

void deleteCachePattern(Pattern* pat) {
	if (pat == NULL) return;
	if (pat->inUse != 0) return;
	pat->managed = 0;
	glDeleteTextures(1, &pat->tex);
	free(pat);
}

static u16 getHash(int param0, int param1) {
	u8 c[8];
	int i;
	u16 hash = 0xAAAA;
	c[0] = (param0 >> 0) & 0xFF;
	c[1] = (param0 >> 8) & 0xFF;
	c[2] = (param0 >> 16) & 0xFF;
	c[3] = (param0 >> 24) & 0xFF;
	c[4] = (param1 >> 0) & 0xFF;
	c[5] = (param1 >> 8) & 0xFF;
	c[6] = (param1 >> 16) & 0xFF;
	c[7] = (param1 >> 24) & 0xFF;

	for (i = 0; i<7; i++) {
		hash = hash ^ c[i];
	}
	for (i = 1; i<8; i++) {
		hash = hash ^ (c[i] << 8);
	}
	return hash;
}

Pattern* popCachePattern(int param0, int param1, int param2, int w, int h) {
  Pattern *pat = patternCache[getHash(param0, param1)];
  if ((pat!= NULL) && (pat->param[0]==param0) && (pat->param[1]==param1) && (pat->param[2]==param2) && (pat->width == w) && (pat->height == h)) {
	pat->inUse++;
  	return pat;
  } else {
	return NULL;
  }
}

void pushCachePattern(Pattern *pat) {
	if (pat == NULL) return;
	pat->inUse = pat->inUse-1;
	if (pat->managed == 0) deleteCachePattern(pat);
}

void addCachePattern(Pattern* pat) {
	Pattern *collider = patternCache[getHash(pat->param[0], pat->param[1])];
	if ((collider != NULL) && (collider->inUse > 0)) return;
	if (collider != NULL) {
		deleteCachePattern(collider);
	}
	pat->managed = 1;
	patternCache[getHash(pat->param[0], pat->param[1])] = pat;
}

Pattern* createCachePattern(int param0, int param1, int param2, int w, int h, float tw, float th, int mesh) {
	Pattern* new = malloc(sizeof(Pattern));
	new->param[0] = param0;
	new->param[1] = param1;
	new->param[2] = param2;
        new->width = w;
	new->height = h;
	new->managed = 0;
	new->inUse = 1;
        new->tw = tw;
        new->th = th;
	new->mesh = mesh;
	return new;
}


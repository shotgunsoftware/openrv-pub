/* simple.h: definitions of some simple, common constants and macros */

#ifndef SIMPLE_HDR
#define SIMPLE_HDR

/* $Header: /home/src/cvsroot/src/pub/arg/simple.h,v 1.1 2007/10/15 22:32:38 src Exp $ */

#include <stdio.h>
#include <math.h>

/* better than standard assert.h: doesn't gag on 'if (p) assert(q); else r;' */
#define assert(p) if (!(p)) \
    { \
        fprintf(stderr, "Assertion failed: %s line %d\n", __FILE__, __LINE__); \
        exit(1); \
    } \
    else

#define str_eq(a, b)    (strcmp(a, b) == 0)
#define MIN(a, b)       ((a)<(b) ? (a) : (b))
#define MAX(a, b)       ((a)>(b) ? (a) : (b))
#define ABS(a)          ((a)>=0 ? (a) : -(a))
#define SWAP(a, b, t)   {t = a; a = b; b = t;}
#define LERP(t, a, b)   ((a)+(t)*((b)-(a)))
#define ALLOC(ptr, type, n)  assert(ptr = (type *)malloc((n)*sizeof(type)))
#define ALLOC_ZERO(ptr, type, n)  assert(ptr = (type *)calloc(n, sizeof(type)))

#define RAD_TO_DEG(x) ((x)*(180./M_PI))
#define DEG_TO_RAD(x) ((x)*(M_PI/180.))

/* note: the following are machine dependent! (ifdef them if possible) */
#define MINSHORT -32768
#define MINLONG -2147483648
#define MININT MINLONG
#ifndef MAXINT  /* sgi has these in values.h */
#   define MAXSHORT 32767
#   define MAXLONG 2147483647
#   define MAXINT MAXLONG
#endif


#ifdef hpux     /* hp's unix doesn't have bzero */
#   define bzero(a, n) memset(a, 0, n)
#endif

#endif

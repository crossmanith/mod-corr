/*----------------------------------------------------------------------
  File    : pcc.h
  Contents: compute pairwise Pearson correlation coefficients
  Author  : Kristian Loewe, Christian Borgelt
----------------------------------------------------------------------*/
#ifndef __PEARSONCC__

#ifdef _MSC_VER
#define uint32_t   unsigned __int32
#define uint64_t   unsigned __int64
#define inline     __inline
#else                           /* MSC still does not support C99 */
#include <stdint.h>
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif
#ifdef __AVX__
#include <immintrin.h>
#endif

/*----------------------------------------------------------------------
  Data Type Definition / Recursion Handling
----------------------------------------------------------------------*/
#ifdef REAL                     /* if REAL is defined */
#  undef  _PCC_PASS             /* ensure _PCC_PASS is undefined */
#  define _PCC_PASS 0           /* define macro for single pass */
#  ifndef SUFFIX                /* function names get no suffix */
#  define SUFFIX                /* (only single set of functions) */
#  endif
#elif !defined _PCC_PASS        /* if in first pass of two */
#  undef  _PCC_PASS             /* ensure _PCC_PASS is undefined */
#  define _PCC_PASS 1           /* define macro for first pass */
#  define REAL      float       /* first pass: single precision */
#  define SUFFIX    Flt         /* function name suffix is 'Flt' */
#else                           /* if in second pass of two */
#  undef  _PCC_PASS             /* ensure _PCC_PASS is undefined */
#  define _PCC_PASS 2           /* define macro for second pass */
#  define REAL      double      /* second pass: double precision */
#  define SUFFIX    Dbl         /* function name suffix is 'Dbl' */
#endif

#define float  1                /* to check the definition of REAL */
#define double 2
#if REAL==float                 /* if single precision data */
#undef  REAL_IS_DOUBLE
#define REAL_IS_DOUBLE  0       /* clear indicator for double */
#else                           /* if double precision data */
#undef  REAL_IS_DOUBLE
#define REAL_IS_DOUBLE  1       /* set   indicator for double */
#endif
#undef float                    /* delete definitions */
#undef double                   /* used for type checking */

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#ifndef PCC_AUTO
#define PCC_AUTO        0       /* use automatic choice */
#define PCC_NAIVE       1       /* use naive implementation */
#define PCC_SSE2        2       /* use SSE2 (streaming SIMD exts.) */
#define PCC_AVX         3       /* use AVX  (advanced vector exts.) */

#define PCC_VARIANT  0x0f       /* mask for variant */
#define PCC_TILED    0x10       /* flag for tiled version */
#define PCC_COBL     0x20       /* flag for cache-oblivious version */
#define PCC_THREAD   0x40       /* flag for multi-thread version */
#endif

#ifndef SFXNAME                 /* macros to generate function names */
#define SFXNAME(n)      SFXNAME_1(n,SUFFIX)
#define SFXNAME_1(n,s)  SFXNAME_2(n,s)
#define SFXNAME_2(n,s)  n##s    /* the two step recursion is needed */
#endif                          /* to ensure proper expansion */

/*----------------------------------------------------------------------
  Functions Prototypes
----------------------------------------------------------------------*/
extern int  SFXNAME(pcc)  (REAL *data, REAL *res, int N, int T);
extern int  SFXNAME(pccx) (REAL *data, REAL *res, int N, int T,
                           int var, ...);

extern void SFXNAME(init_naive) (REAL *data, int N, int T,
                                 REAL *diff, int X, REAL *rssd);
extern void SFXNAME(init_sse2)  (REAL *data, int N, int T,
                                 REAL *diff, int X, REAL *rssd);
extern void SFXNAME(init_avx)   (REAL *data, int N, int T,
                                 REAL *diff, int X, REAL *rssd);


/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#if   _PCC_PASS <= 0
#define pcc(d,r,N,T)     pccx(d,r,N,T,0)
#elif _PCC_PASS <= 1
#define pccFlt(d,r,N,T)  pccxFlt(d,r,N,T,0)
#elif _PCC_PASS <= 2
#define pccDbl(d,r,N,T)  pccxDbl(d,r,N,T,0)
#endif

/*----------------------------------------------------------------------
  Inline Functions
----------------------------------------------------------------------*/

inline REAL SFXNAME(pair_naive) (REAL *a, REAL *b, int n)
{                               /* --- process a pair of series */
  int  k;                       /* loop variable */
  REAL sum;                     /* sum of products */

  for (sum = 0, k = 0; k < n; k++)
    sum += a[k] *b[k];          /* sum pairwise products */
  return sum;                   /* return the computed sum */
}  /* pair_naive() */

/*--------------------------------------------------------------------*/
#ifdef __SSE2__

inline REAL SFXNAME(pair_sse2) (REAL *a, REAL *b, int n)
{                               /* --- process a pair of series */
  int     k;                    /* loop variable */
  #if REAL_IS_DOUBLE            /* data is double precision */
  __m128d s;                    /* register for SSE2 computations */
  s = _mm_setzero_pd();         /* initialize the sum (2 values) */
  for (k = 0; k < n; k += 2)    /* sum two products in parallel */
    s = _mm_add_pd(s, _mm_mul_pd(_mm_load_pd(a+k), _mm_load_pd(b+k)));
  s = _mm_add_pd(s, _mm_shuffle_pd(s, s, 1));
  return _mm_cvtsd_f64(s);      /* sum two sums horizontally */
  #else                         /* data is single precision */
  __m128  s;                    /* register for SSE2 computations */
  s = _mm_setzero_ps();         /* initialize the sum (4 values) */
  for (k = 0; k < n; k += 4)    /* sum four products in parallel */
    s = _mm_add_ps(s, _mm_mul_ps(_mm_load_ps(a+k), _mm_load_ps(b+k)));
  s = _mm_add_ps(s, _mm_movehl_ps(s, s));
  s = _mm_add_ss(s, _mm_shuffle_ps(s, s, 1));
  return _mm_cvtss_f32(s);      /* sum four sums horizontally */
  #endif
}  /* pair_sse2() */

#endif
/*--------------------------------------------------------------------*/
#ifdef __AVX__

inline REAL SFXNAME(pair_avx) (REAL *a, REAL *b, int n)
{                               /* --- process a pair of series */
  int     k;                    /* loop variable */
  #if REAL_IS_DOUBLE            /* data is double precision */
  __m256d s;                    /* register for AVX  computations */
  __m128d x;                    /* register for SSE2 computations */
  s = _mm256_setzero_pd();      /* initialize the sum (4 values) */
  for (k = 0; k < n; k += 4)    /* sum two products in parallel */
    s = _mm256_add_pd(s, _mm256_mul_pd(_mm256_load_pd(a+k),
                                       _mm256_load_pd(b+k)));
  x = _mm_add_pd(_mm256_extractf128_pd(s, 0),
                 _mm256_extractf128_pd(s, 1));
  x = _mm_add_pd(x, _mm_shuffle_pd(x, x, 1));
  return _mm_cvtsd_f64(x);      /* sum four sums horizontally */
  #else                         /* data is single precision */
  __m256  s;                    /* register for AVX  computations */
  __m128  x;                    /* register for SSE2 computations */
  s = _mm256_setzero_ps();      /* initialize the sum (8 values) */
  for (k = 0; k < n; k += 8)    /* sum four products in parallel */
    s = _mm256_add_ps(s, _mm256_mul_ps(_mm256_load_ps(a+k),
                                       _mm256_load_ps(b+k)));
  #if 0
  x = _mm_add_ps(_mm256_castps256_ps128(s),
                 _mm256_extractf128_ps(s, 1));
  x = _mm_add_ps(x, _mm_movehl_ps(x, x));
  x = _mm_add_ss(x, _mm_shuffle_ps(x, x, 1));
  #else
  s = _mm256_hadd_ps(s, s);     /* do horizontal sums in upper */
  s = _mm256_hadd_ps(s, s);     /* and lower half of the register */
  x = _mm_add_ss(_mm256_castps256_ps128(s),
                 _mm256_extractf128_ps(s, 1));
  #endif
  return _mm_cvtss_f32(x);      /* sum eight sums horizontally */
  #endif
}  /* pair_avx() */

#endif
/*----------------------------------------------------------------------
  Recursion Handling
----------------------------------------------------------------------*/
#if _PCC_PASS == 1              /* if in first of two passes */
#undef REAL
#undef SUFFIX
#include "pcc.h"                /* process header recursively */
#elif _PCC_PASS == 2
#undef REAL
#endif

#undef SUFFIX
#undef SFXNAME
#undef SFXNAME_1
#undef SFXNAME_2
#undef REAL_IS_DOUBLE

#undef  _PCC_PASS
#define __PEARSONCC__
#endif
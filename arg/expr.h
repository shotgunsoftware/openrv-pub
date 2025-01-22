/* expr.h: definitions for expression evaluator */

#ifndef EXPR_HDR
#define EXPR_HDR

/* $Header: /home/src/cvsroot/src/pub/arg/expr.h,v 1.1 2007/10/15 22:32:38 src
 * Exp $ */

/* error codes */
#define EXPR_GOOD 0  /* expression totally good */
#define EXPR_SOSO -1 /* expression partially good */
#define EXPR_BAD -2  /* expression totally bad */

extern int expr_error; /* holds error code after expr_eval */

#ifdef __cplusplus
extern "C"
{
    int expr_eval_int(char* str);
    long expr_eval_long(char* str);
    double expr_eval(char* str);
}
#else
int expr_eval_int();
long expr_eval_long();
double expr_eval();
#endif

#endif

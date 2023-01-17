/*
 * expr_eval: expression evaluator - converts ascii string to floating point
 * Works by top-down predictive parsing.
 * Most of the routines gobble characters and advance global string pointer s.
 * Sets global expr_err if an error occurs.
 *
 * supports: parentheses, % for mod, ^ for pow, elementary functions,
 * constants pi and e, variable base constants
 *
 * Paul Heckbert    18 April 1988
 */

static char rcsid[] = "$Header: /home/src/cvsroot/src/pub/arg/expr.c,v 1.1 2007/10/15 22:32:38 src Exp $";

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "simple.h"
#include "expr.h"
#define space() for (; isspace(*s); s++)

/* AJG - no M_PI in win32 math.h */
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

void paren2(double *x, double *y);
int expr_eval_int( char *str );
long expr_eval_long(char *str);
double expr_eval(char *str);
double expr();
double term();
double factor();
double signednumber();
double number();
double paren();
void paren2(double *x, double *y);
double posconst();
int digit(int c);
double expt(int a, int n);
int eq(int n, char *a, char *b);
void error(char *s, int len, char *err);
void prints(int n, char *s);

/* ***************************************************************************** */

static char *s0, *s;
int expr_error;

int expr_eval_int( char *str )
{
    double x;

    x = expr_eval(str);
    /* do unsigned double to signed int conversion: */
    return x>INT_MAX ? x+2.*INT_MIN : x;
}

long expr_eval_long(char *str)
{
    double x;

    x = expr_eval(str);
    /* do unsigned double to signed long conversion: */
    return x>LONG_MAX ? x+2.*LONG_MIN : x;
}

double expr_eval(char *str)
{
    double x;

    s0 = s = str;
    expr_error = EXPR_GOOD;
    x = expr();
    if (*s) {
    error(s, 1, "garbage in expression");
    expr_error = s==s0 ? EXPR_BAD : EXPR_SOSO;
    }
    return x;
}

double expr()
{
    double x;

    for (x=term();;) {
    space();
    switch (*s) {
        case '+': s++; x += term(); break;
        case '-': s++; x -= term(); break;
        default: return x;
    }
    }
}

double term()
{
    double x, y;

    for (x=factor();;) {
    space();
    switch (*s) {
        case '*': s++; x *= factor(); break;
        case '/': s++; x /= factor(); break;
        case '%': s++; y = factor(); x = x-floor(x/y)*y; break;
        default: return x;
    }
    }
}

double factor()
{
    double x;

    for (x=signednumber();;) {
    space();
    switch (*s) {
        case '^': s++; return pow(x, factor()); /* right-associative */
        default: return x;
    }
    }
}

double signednumber()
{
    space();
    switch (*s) {
    case '-': s++; return -signednumber();
    case '+': s++; return signednumber();
    default: return number();
    }
}

double number()
{
    char *func;
    int n;
    double x, y;

    space();
    if (isdigit(*s) || *s=='.') return posconst();
    if (*s=='(') return paren();

    if (isalpha(*s)) {
    func = s;
    for (s++; isalpha(*s) || isdigit(*s); s++);
    n = s-func; /* length of funcname */

    if (eq(n, func, "pi"))      return M_PI;
    if (eq(n, func, "e"))       return exp(1.);

    if (eq(n, func, "sqrt"))    return sqrt(paren());
    if (eq(n, func, "exp"))     return exp(paren());
    if (eq(n, func, "log"))     return log(paren());
    if (eq(n, func, "pow"))     {paren2(&x, &y); return pow(x, y);}

    if (eq(n, func, "sin"))     return sin(paren());
    if (eq(n, func, "cos"))     return cos(paren());
    if (eq(n, func, "tan"))     return tan(paren());
    if (eq(n, func, "asin"))    return asin(paren());
    if (eq(n, func, "acos"))    return acos(paren());
    if (eq(n, func, "atan"))    return atan(paren());
    if (eq(n, func, "atan2"))   {paren2(&x, &y); return atan2(x, y);}

    if (eq(n, func, "sind"))    return sin(DEG_TO_RAD(paren()));
    if (eq(n, func, "cosd"))    return cos(DEG_TO_RAD(paren()));
    if (eq(n, func, "tand"))    return tan(DEG_TO_RAD(paren()));
    if (eq(n, func, "dasin"))   return RAD_TO_DEG(asin(paren()));
    if (eq(n, func, "dacos"))   return RAD_TO_DEG(acos(paren()));
    if (eq(n, func, "datan"))   return RAD_TO_DEG(atan(paren()));
    if (eq(n, func, "datan2"))  {paren2(&x, &y);
                    return RAD_TO_DEG(atan2(x, y));}

    if (eq(n, func, "floor"))   return floor(paren());
    if (eq(n, func, "ceil"))    return ceil(paren());

    error(func, n, "bad numerical expression");
    return 0.;
    }

    error(s, 1, "syntax error");
    return 0.;
}

/* paren: '(' expr ')' */

double paren()
{
    double x;

    space();
    if (*s!='(') error(s, 1, "expected '('");
    s++;
    x = expr();
    space();
    if (*s!=')') error(s, 1, "expected ')'");
    s++;
    return x;
}

/* paren2: '(' expr ',' expr ')' */

void paren2(double *x, double *y)
{
    space();
    if (*s!='(') error(s, 1, "expected '('");
    s++;
    *x = expr();
    space();
    if (*s!=',') error(s, 1, "expected ','");
    s++;
    *y = expr();
    space();
    if (*s!=')') error(s, 1, "expected ')'");
    s++;
}

/*
 * posconst: given a string beginning at s, return floating point value.
 * like atof but it uses and modifies the global ptr s
 */

double posconst()
{
    int base, exp, pos, d;
    double x, y;

    space();
    if (*s=='0') {      /* change base: 10 = 012 = 0xa = 0b2:1010 */
    s++;
    switch (*s) {
        case 'b':
        s++;
        for (base=0; isdigit(*s); s++)
            base = base*10+*s-'0';  /* base is in base 10! */
        if (*s!=':') error(s, 1, "expecting ':'");
        s++;
        break;
        case 'x': s++; base = 16; break;
        case 't': s++; base = 10; break;
        case '.': base = 10; break;     /* a float, e.g.: 0.123 */
        default:  base = 8; break;
    }
    }
    else base = 10;

    x = 0.;
    for (; d = digit(*s), d>=0 && d<base; s++)
    x = x*base+d;
    if (*s=='.') {
    s++;
    for (y=1.; d = digit(*s), d>=0 && d<base; s++) {
        x = x*base+d;       /* fraction is in variable base */
        y *= base;
    }
    x /= y;
    }
    if (*s=='e' || *s=='E') {
    s++;
    if (*s=='-')      {s++; pos = 0;}
    else if (*s=='+') {s++; pos = 1;}
    else pos = 1;
    for (exp=0; isdigit(*s); s++)
        exp = exp*10+*s-'0';    /* exponent is in base 10 */
    y = expt(base, exp);
    if (pos) x *= y;
    else x /= y;
    }
    return x;
}

int digit(int c)
{
    return isdigit(c) ? c-'0' :
    c>='a'&&c<='z' ? c-'a'+10 : c>='A'&&c<='Z' ? c-'A'+10 : -1;
}

/* expt: a^n for n>=0 */

double expt(int a, int n)
{
    double t, x;

    if (n<0) {
    fprintf(stderr, "expt: can't do negative exponents\n");
    return 1.;
    }
    if (n==0) return 1.;
    for (t=a, x=1.; n>0; n>>=1) {
    if (n&1) x *= t;
    t *= t;
    }
    return x;
}

/* eq: test equality of string a, of length n, with null-terminated string b */

int eq(int n, char *a, char *b)
{
    char c;
    int ret;

    c = a[n];
    a[n] = 0;
    ret = str_eq(a, b);
    a[n] = c;
    return ret;
}

void error(char *s, int len, char *err)
{
    if (*s==0) s[len] = 0;  /* just in case */
    printf("expr: %s: ", err);
    prints(s-s0, s0);
    printf("[");
    prints(len, s);
    printf("]");
    prints(s+strlen(s)-s0-len, s+len);
    printf("\n");
    if (expr_error!=EXPR_BAD)
    expr_error = s==s0 ? EXPR_BAD : EXPR_SOSO;
}

/* prints: print string s of length n */

void prints(int n, char *s)
{
    char c;

    c = s[n];
    s[n] = 0;
    printf("%s", s);
    s[n] = c;
}

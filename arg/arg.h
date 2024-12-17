/* arg.h: definitions for argument parsing package */

#ifndef ARG_HDR
#define ARG_HDR

/* $Header: /home/src/cvsroot/src/pub/arg/arg.h,v 1.1 2007/10/15 22:32:38 src
 * Exp $ */

#include <stdio.h>
#include <expr.h>
#include <stdarg.h>

typedef struct arg_form
{ /* ARGUMENT FORM */

    /* a "form" contains the format, doc string, and miscellaneous internal */
    /* info about an argument.  It's an argument descriptor, basically */

    struct arg_form* next;    /* next in linked list */
    char* format;             /* scanf-style format:    "-size %d %F" */
    char* flag;               /* flag portion of format:"-size" */
    char* code;               /* just the format codes: "dF" */
    char* doc;                /* documentation string:  "set widget size" */
    short type;               /* REGULAR | SIMPFLAG | PARAMFLAG |
                                      SUBRFLAG | SUBLISTFLAG | NOP */
    short nparam;             /* number of parameters to flag */
    int parammask;            /* bit i says ok to stop before param i, i=0..*/
    int** param;              /* parameter pointer list */
    int (*subr)();            /* subroutine to call for action (if any) */
    struct arg_form* sublist; /* subordinate list (if any) */
    short rep;                /* # times this flag repeated in arglist */
} Arg_form;

/* form type values */
#define ARG_REGULAR 1     /* a regular argument */
#define ARG_SIMPFLAG 2    /* a simple flag (no parameters) */
#define ARG_PARAMFLAG 3   /* a flag with parameters */
#define ARG_SUBRFLAG 4    /* a flag with subroutine action */
#define ARG_SUBLISTFLAG 5 /* a sub-formlist */
#define ARG_NOP 6         /* no arg or flag, just a doc string */

/* the following must be impossible pointer values (note: machine-dependent) */
#if defined(__x86_64__) || defined(__aarch64__)
#define ARG_MASKNEXT 0x8000000000000000 /* mask for these NEXT flags */
#define ARG_FLAGNEXT 0x8000000000000001
#define ARG_SUBRNEXT 0x8000000000000002
#define ARG_LISTNEXT 0x8000000000000003
#else
#define ARG_MASKNEXT 0x80000000 /* mask for these NEXT flags */
#define ARG_FLAGNEXT 0x80000001
#define ARG_SUBRNEXT 0x80000002
#define ARG_LISTNEXT 0x80000003
#endif

/* varargs tricks */
#define ARG_FLAG(ptr) ARG_FLAGNEXT, (ptr)    /* for SIMPFLAG */
#define ARG_SUBR(ptr) ARG_SUBRNEXT, (ptr)    /* for SUBRFLAG */
#define ARG_SUBLIST(ptr) ARG_LISTNEXT, (ptr) /* for SUBLISTFLAG */

/* error codes: BADCALL is a programmer error, the others are user errors */
#define ARG_BADCALL -1 /* arg_parse call itself is bad */
#define ARG_BADARG -2  /* bad argument given */
#define ARG_MISSING -3 /* argument or parameter missing */
#define ARG_EXTRA -4   /* extra argument given */

extern int arg_debug, arg_doccol;
extern int arg_warning; /* print warnings about repeated flags? */
Arg_form* arg_to_form1(va_list ap);
Arg_form* arg_find_flag(char* arg, Arg_form* form);
Arg_form* arg_find_reg();
void arg_init(Arg_form* form);
void space(FILE* fp, int c, int c1);
void av_print(char* str, int ac, char** av);

#ifdef __cplusplus
extern "C"
{
    int arg_parse(int ac, char** av, ...);
    int arg_parse_argv(int ac, char** av, Arg_form* form);
    int arg_parse_stream(FILE* fp, Arg_form* form);
    Arg_form* arg_to_form(void* xxx, ...);
    int arg_form_print(Arg_form* form);
    int checkstr(char* s, char* name, char* prev);
    int arg_format(Arg_form* f);
    int arg_parse_form1(int ac, char** av, Arg_form* form);
    int arg_do(int ac, char** av, Arg_form* f);
    int nargs(int ac, char** av, Arg_form* f, int* skip);
    int arg_done();
    int scan(int narg, char** arg, Arg_form* f);
}
#else
int scan(int narg, char** arg, Arg_form* f);
int arg_format(Arg_form* f);
int arg_done();
int nargs(int ac, char** av, Arg_form* f, int* skip);
int arg_do(int ac, char** av, Arg_form* f);
int arg_parse_form1(int ac, char** av, Arg_form* form);
int checkstr(char* s, char* name, char* prev);
int arg_parse_stream(FILE* fp, Arg_form* form);
Arg_form* arg_to_form(void* xxx, ...);
int arg_parse_argv(int ac, char** av, Arg_form* form);
Arg_form* arg_to_form(void* xxx, ...);
void arg_form_print(Arg_form* form);
#endif

#endif

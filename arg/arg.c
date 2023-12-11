/*
 * arg_parse: Command line argument parser.
 *
 * notable features:
 *      arbitrary order of flag arguments
 *      automatic argument conversion and type checking
 *      multiple-character flag names
 *      required, optional, and flag arguments
 *      automatic usage message
 *      subroutine call for exotic options (variable number of parameters)
 *      modularized parsers encourage standardized options
 *      expression evaluation
 *      works either from argv or in interactive mode,
 *          as a primitive language parser and interpreter
 *      concise specification
 *      easy to use
 *
 * Paul Heckbert        ph@cs.cmu.edu
 *
 * 19 April 1988 - written at UC Berkeley
 *
 * simpler version written at Pacific Data Images, Aug 1985.
 * Ideas borrowed from Ned Greene's ARGS at New York Inst. of Tech.
 * and Alvy Ray Smith's AARG at Pixar.
 */

static char rcsid[] = "$Header: /home/src/cvsroot/src/pub/arg/arg.c,v 1.2 2008/01/28 20:58:29 src Exp $";

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "simple.h"
#include "arg.h"

#define CHECKTYPE(form, keyword) \
    if (form->type!=0) { \
        fprintf(stderr, "arg: %s doesn't belong in %s paramlist\n", \
            keyword, form->format); \
        return 0; \
    } \
    else

/* recognize a valid numeric constant or expression by its first char: */
#define NUMERIC(s) (isdigit(*(s)) || \
    *(s)=='.' || *(s)=='-' || *(s)=='+' || *(s)=='(')

int arg_debug = 0;              /* debugging level; 0=none */
int arg_doccol = 24;            /* column at which to print doc string*/
int arg_warning = 1;            /* print warnings about repeated flags? */

static Arg_form *regf;          /* advancing form ptr used by arg_find_reg */



/* Apparently, you aren't allowed to return a va_list on 64-bit platforms,
   so we pass it as a pointer argument.  However, this doesn't work in 32-
   bit for some reason
*/
#ifdef __x86_64__
void arg_doc_parse(Arg_form *f, va_list ap, va_list *returnAp);
#else
va_list arg_doc_parse(Arg_form *f, va_list ap);
#endif



/*
 * arg_parse(ac, av, varargs_list)
 * Parse the arguments in av according to the varargs list, which contains
 * format strings, parameter and subroutine ptrs, and other stuff.
 * The varargs list must be terminated by a 0.
 * Returns an error code.
 */

int arg_parse(int ac, char **av, ...)
{
    int ret;
    va_list ap;
    Arg_form *form;

    va_start(ap, av);
    if (ac<1) {
        fprintf(stderr,
            "arg_parse: first arg to arg_parse (%d) doesn't look like argc\n",
            ac);
        return ARG_BADCALL;
    }

    /* convert varargs to formlist */
    form = arg_to_form1(ap);
    if (!form)
        return ARG_BADCALL;

    /* parse args according to form */
    if (ac==2 && str_eq(av[1], "-")) /* no args, run interactive version */
        ret = arg_parse_stream(stdin, form);
    else                                /* args supplied, parse av */
        ret = arg_parse_argv(ac, av, form);
    return ret;
}

/*----------------------------------------------------------------------*/

/*
 * arg_to_form: convert varargs to formlist
 * not called by arg_parse, but sometimes called from outside to build sublists


 * NOTE: varargs.h supports functions with only variable arguments (i.e.
         f(...) ), but stdarg.h requires at least one fixed argument.  Since
         this function is seldom-used, I just slapped a NULL pointer here.
         -Mike @ Tweak  5.15.2003
 */


Arg_form *arg_to_form(void *xxx, ...)
{
    va_list ap;
    if( xxx != NULL )
    {
        fprintf( stderr, "\nWARNING: Move from varargs.h to stdargs.h changed "
                "function prototype of arg_to_form!  See %s, line %d.\n\n",
                __FILE__,__LINE__ );
        exit(-1);
    }
    va_start(ap,xxx);
    return arg_to_form1(ap);
}

/*
 * arg_to_form1: convert varargs to formlist recursively.
 * assumes va_start has already been called
 * calls va_end when done to clean up
 * returns 0 on error.
 */

Arg_form *arg_to_form1( va_list ap )
{
    char *s;
    char *prevs;
    int pi;
    int t;
    Arg_form *form;
    Arg_form *prevform;
    Arg_form *rootform;

    /*
     * varargs syntax is:
     *     formatstr [KEYWORD val] paramptr* docstr docargptr*
     * where there are as many paramptrs as %'s in the format string
     * and as many docargptrs as %'s in the doc string
     */
    rootform = 0;
    prevs = "";
    for (prevform=0; (s = va_arg(ap, char *)) != 0; prevform=form) {

        /* first, read the format string */
        if (checkstr(s, "format string", prevs)) return 0;
        
        ALLOC(form, Arg_form, 1);

        form->next = NULL;
        form->format = s;
        form->flag = NULL;
        form->type = 0;
        form->param = NULL;
        form->parammask = 0;
        form->subr = NULL;
        form->sublist = NULL;
        if( prevform ) 
        {   
            prevform->next = form;
        }
        else 
        {
            rootform = form;
        }

        /* parse format to create flag and code strings, compute #params */
        t = arg_format( form );
        if( t )
        {
            return NULL;
        }

        /* next, read the parameters and keywords */
        pi = 0;
        if (form->nparam>0) {
            form->type = form->flag[0]=='-' ? ARG_PARAMFLAG : ARG_REGULAR;
            assert(form->param = (int **)malloc(form->nparam*sizeof(int *)));
        }
        for (; ( s = va_arg( ap, char * ) ) != NULL; )
        {
            long action = (long)s;
            
            /* note that we continue (not break) in all cases except one */
            switch(action)  /* TODO */
            {
                case ARG_FLAGNEXT:              /* ptr to flag vbl */
                    CHECKTYPE(form, "FLAG");
                    form->type = ARG_SIMPFLAG;
                    ALLOC(form->param, int *, 1);
                    *form->param = va_arg(ap, int *);
                    continue;
                case ARG_SUBRNEXT:              /* ptr to action subr */
                    CHECKTYPE(form, "SUBR");
                    form->type = ARG_SUBRFLAG;
                    form->subr = (int (*)())va_arg(ap, int *);
                    /* append dots to end of format string */
                    assert(s = (char *)malloc(strlen(form->format)+5));
                    sprintf(s, "%s ...", form->format);
                    form->format = s;
                    continue;
                case ARG_LISTNEXT:              /* ptr to sub-formlist */
                    CHECKTYPE(form, "SUBLIST");
                    form->type = ARG_SUBLISTFLAG;
                    form->sublist = va_arg(ap, Arg_form *);
                    continue;
                default:                        /* ptr to param */
                    if( pi >= form->nparam )
                    {
                        break;
                    }
                    form->param[pi++] = (int *)s;
                    continue;
            }
            break;                              /* end of params/keywords */
        }

        if (!form->flag[0] && form->type==ARG_SUBLISTFLAG) {
            fprintf(stderr, "arg: sublist must be given a flag name\n");
            return 0;
        }
        if (!form->type)                        /* just a doc string */
            form->type = ARG_NOP;
        /* finally, read the doc string */
        if (checkstr(s, "doc string", form->format)) return 0;
        form->doc = prevs = s;

        /* skip over doc args */
                
#ifdef __x86_64__
        va_list apx;
        arg_doc_parse(form, ap, &apx);
#ifdef __va_copy
        __va_copy( ap, apx );
#else
        va_copy( ap, apx );
#endif
#else
        ap = arg_doc_parse(form, ap);
#endif                

    }
    va_end(ap);
    return rootform;
}

/* checkstr: check that s is a valid string */

int checkstr( char *s, char *name, char *prev )
{
    char *delim = prev ? "\"" : "";
    long sValue = (long)s;
    
/*
 *  Windows 8 blows up if we use the MASKNEXT test below, so just ignore that
 *  for now.
 */
#ifdef PLATFORM_WINDOWS
    if( s == NULL) 
#else
    if( s == NULL || sValue & ARG_MASKNEXT ) 
#endif
    {
        fprintf( stderr, "bad arg call: missing %s after %s%s%s\n",
                 name, delim, prev, delim);
        abort();
        return 1;
    }
    return 0;
}

/*
 * arg_format: parse the format string to create flag string,
 * code string, parammask, and count the number of params.
 * e.g.: format="-size %d %F"  =>  flag="-size", code="dF", nparam=2
 */

int arg_format(Arg_form *f)
{
    char *s, *c;
    int n, np;

    if (f->format[0]=='-') {            /* flag string present */
        /* find the end of the flag string, put flag string in f->flag */
        for (s= &f->format[1]; *s && *s!=' ' && *s!='%' && *s!='['; s++);
        n = s-f->format;
        assert(f->flag = (char *)malloc(n+1));
        memmove(f->flag, f->format, n);
        f->flag[n] = 0;
    }
    else {
        s = f->format;                  /* no flag string: probably a reg arg */
        f->flag = "";                   /* or maybe a flagless subrflag */
    }

    /* extract scanf codes from remainder of format string, put in f->code */
    n = (f->format+strlen(f->format)-s)/2;      /* overestimate # of % codes */
    assert(f->code = (char *)malloc(n+1));
    for (c=f->code, np=0;; np++, s++) {
        for (; *s==' ' || *s=='['; s++)
            if (*s=='[') f->parammask |= 1<<np;
        if (!*s || *s==']') break;
        if (*s!='%' || !s[1]) {
            fprintf(stderr, "arg: bad format string (%s)\n", f->format);
            return ARG_BADCALL;
        }
        *c++ = *++s;
    }
    for (; *s; s++)
        if (*s!=' ' && *s!=']') {
            fprintf(stderr, "bad format (%s), nothing allowed after ']'s\n",
                f->format);
            return ARG_BADCALL;
        }
    f->parammask |= 1<<np;
    if (np>=8*sizeof(int)) {
        fprintf(stderr, "out of bits in parammask! too many params to %s\n",
            f->flag);
        return ARG_BADCALL;
    }

    /* number of parameters to flag = number of '%'s in format string */
    f->nparam = np;
    *c = 0;
    if (c-f->code!=f->nparam) fprintf(stderr, "OUCH!\n");
    return 0;
}

/*
 * arg_doc_parse: Find the '%' format codes in f->doc and increment varargs
 * ptr ap over the doc string parameters.  Updates f->doc to be the formatted
 * documentation string and returns the new ap.
 */

#ifdef __x86_64__
void arg_doc_parse(Arg_form *f, va_list ap, va_list *ap0)
#else
va_list arg_doc_parse(Arg_form *f, va_list ap)
#endif
{
    char *s, buf[256];
    int size, gotparam;

#ifdef __x86_64__
#ifdef __va_copy
    __va_copy( (*ap0), ap );
#else
    va_copy( (*ap0), ap );
#endif
#else
    va_list ap0;
    ap0 = ap;
#endif

    gotparam = 0;
    for (s=f->doc; *s; s++) {
        for (; *s; s++)                 /* search for next format code */
            if (s[0]=='%')
                if (s[1]=='%') s++;     /* skip over %% */
                else break;
        if (!*s) break;
        /* skip over numerical parameters */
        for (s++; *s && *s=='-' || *s>'0'&&*s<='9' || *s=='.'; s++);
        /* now *s points to format code */
        switch (*s) {
            case 'h': size = 0; s++; break;     /* half */
            case 'l': size = 2; s++; break;     /* long */
            default : size = 1;      break;     /* normal size */
        }
        if (!*s) {
            fprintf(stderr, "arg: premature end of string in (%s)\n", f->doc);
            break;
        }
        gotparam = 1;
        /*
         * simulate printf's knowledge of type sizes
         * (it's too bad we have to do this)
         */
        switch (*s) {
            case 'd': case 'D':
            case 'o': case 'O':
            case 'x': case 'X':
            case 'c':
                if (size==2 || *s>='A' && *s<='Z') va_arg(ap, long);
                else va_arg(ap, int);
                break;
            case 'e':
            case 'f':
            case 'g':
                /* note: float args are converted to doubles by MOST compilers*/
                va_arg(ap, double);
                break;
            case 's':
                va_arg(ap, char *);
                break;
            default:
                fprintf(stderr, "arg: unknown format code %%%c in %s\n",
                    *s, f->doc);
                va_arg(ap, int);
                break;
        }
    }
    if (gotparam) {     /* there are doc parameters, format a new doc string */
#ifdef __x86_64__
        vsprintf(buf, f->doc, (*ap0));
#else
        vsprintf(buf, f->doc, ap0);
#endif        
        assert(f->doc = (char *)malloc(sizeof(buf)+1));
        strcpy(f->doc, buf);
    }

#ifndef __x86_64__
    return ap;          /* varargs ptr past end of doc params */
#endif    
}

/*----------------------------------------------------------------------*/

#define LINEMAX 256
#define ACMAX 128

typedef enum {SPACE, TOKEN, END} Token_type;
Token_type token_char();

/*
 * arg_parse_stream: parse from an input stream (not from an arg vector)
 * parse args in stream fp, line by line, according to formlist in form
 * Returns 0 on success, negative on failure.
 */

int arg_parse_stream(FILE *fp, Arg_form *form)
{
    char c, *av[ACMAX], line[LINEMAX], *p;
    Token_type type;
    int i, ac, ret, err;
    Arg_form *oldregf;

    oldregf = regf;
    regf = form;
    arg_init(form);

    av[0] = "hi";
    ret = 0;
    for (;;) {                          /* read and process line */
        p = line;
        while ((type = token_char(fp, &c))==SPACE);
        for (ac=1; type!=END && ac<ACMAX;) {    /* split line into tokens */
            av[ac++] = p;               /* save ptr to beginning of token */
            do {
                *p++ = c;
                if (p >= line+LINEMAX) {
                    fprintf(stderr, "input line too long\n");
                    exit(1);
                }
            } while ((type = token_char(fp, &c))==TOKEN);
            *p++ = 0;                   /* terminate this token in line[] */
            if (type==END) break;
            while ((type = token_char(fp, &c))==SPACE);
        }
        if (feof(fp)) break;
        if (arg_debug) {
            fprintf(stderr, "ac=%d: ", ac);
            for (i=1; i<ac; i++) fprintf(stderr, "(%s) ", av[i]);
            fprintf(stderr, "\n");
        }

        err = arg_parse_form1(ac, av, form);
        if (!ret) ret = err;
    }
    if (!ret) ret = arg_done();
    regf = oldregf;
    return ret;
}

/*
 * token_char: is next char in stream fp part of a token?
 * returns TOKEN if char in token, SPACE if whitespace, END if end of input line
 * *p gets new char.
 * handles quoted strings and escaped characters.
 */

Token_type token_char(FILE *fp, char *p)
{
    int c, old_mode;
    Token_type type;
    static int mode = 0;        /* = '"' or '\'' if inside quoted string */

    type = TOKEN;
    do {
        old_mode = mode;
        c = getc(fp);
        switch (c) {
            case EOF:
                type = END;
                break;
            case '\\':
                switch (c = getc(fp)) {
                    case 'b': c = '\b'; break;
                    case 'f': c = '\f'; break;
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case 'v': c = '\v'; break;
                    case '0': c = '\0'; break;
                }
                break;
            case '"':
                switch (mode) {
                    case 0: mode = '"'; break;          /* begin " */
                    case '"': mode = 0; break;          /* end " */
                }
                break;
            case '\'':
                switch (mode) {
                    case 0: mode = '\''; break;         /* begin ' */
                    case '\'': mode = 0; break;         /* end ' */
                }
                break;
            case '\n':
                switch (mode) {
                    case 0: type = END; break;
                }
                break;
        }
        /* loop until we read a literal character */
    } while (old_mode != mode);
    *p = c;

    if (type!=END && mode==0 && (c==' ' || c=='\t' || c=='\n'))
        type = SPACE;
    return type;
}

/*
 * arg_parse_argv: do the actual parsing!
 * parse the arguments in av according to the formlist in form
 * Returns 0 on success, negative on failure.
 */

int arg_parse_argv(int ac, char **av, Arg_form *form)
{
    int ret;
    Arg_form *oldregf;

    oldregf = regf;
    regf = form;
    arg_init(form);
    ret = arg_parse_form1(ac, av, form);
    if (!ret) ret = arg_done();
    regf = oldregf;
    return ret;
}

int arg_parse_form1(int ac, char **av, Arg_form *form)
{
    int i, di;
    Arg_form *f;

    for (i=1; i<ac; i+=di) {
        if (arg_debug)
            fprintf(stderr, "arg %d: (%s)\n", i, av[i]);
        if (av[i][0]=='-' && !NUMERIC(&av[i][1])) {     /* flag argument */
            f = arg_find_flag(av[i], form);
            if (!f) {
                if (strcmp(av[i], "-help"))
                    /*if (av[i][1])*/
                {
                    fprintf(stderr, "unrecognized arg: %s\n", av[i]);
                    fprintf(stderr, "use the -help option for usage information\n");
                }
                else            /* arg was "-help"; print usage message */
                {
                    arg_form_print( form );
                }
                return ARG_EXTRA;
            }
            di = arg_do(ac-i-1, &av[i+1], f);
            if (di<0) return di;
            di++;
        }
        else {                  /* regular argument */
            f = arg_find_reg();
            if (!f) {
                /* regular args exhausted, see if any flagless subrflags */
                f = arg_find_flag("", form);
                if (!f) {
                    fprintf(stderr, "extra arg: %s\n", av[i]);
                    return ARG_EXTRA;
                }
            }
            di = arg_do(ac-i, &av[i], f);
            if (di<0) return di;
        }
    }

    return 0;
}

/*
 * arg_init: initialize formlist before parsing arguments
 * Set simple flags and repeat counts to 0.
 */

void arg_init(Arg_form *form)
{
    Arg_form *f;

    for (f=form; f; f=f->next)
        if (f->type==ARG_SUBLISTFLAG) arg_init(f->sublist);     /* recurse */
        else {
            f->rep = 0;
            /*if (f->type==ARG_SIMPFLAG) **f->param = 0;*/
        }
}

int arg_done()
{
    for (; regf; regf=regf->next)       /* any required reg args remaining? */
        if (regf->type==ARG_REGULAR && !(regf->parammask&1)) {
            fprintf(stderr, "regular arg %s (%s) not set\n",
                regf->format, regf->doc);
            return ARG_MISSING;
        }
    return 0;
}

/*
 * arg_find_flag: find the flag matching arg in the form list (tree)
 * returns form ptr if found, else 0
 */

Arg_form *arg_find_flag(char *arg, Arg_form *form)
{
    Arg_form *f, *t;

    for (f=form; f; f=f->next) {
        if (f->type!=ARG_REGULAR && f->type!=ARG_NOP && str_eq(f->flag, arg))
            return f;
        if (f->type==ARG_SUBLISTFLAG) {
            t = arg_find_flag(arg, f->sublist);         /* recurse */
            if (t) return t;
        }
    }
    return NULL;
}

/*
 * arg_find_reg: find next regular argument
 * each call advances the global pointer regf through the formlist
 */

Arg_form *arg_find_reg()
{
    Arg_form *f;

    for (; regf; regf=regf->next) {
        if (regf->type==ARG_REGULAR) {
            f = regf;
            regf = regf->next;
            return f;
        }
    }
    return NULL;
}

/*
 * arg_do: process one form by parsing arguments in av according to the
 * single form in f
 *
 * f was found by arg_find_flag or arg_find_reg,
 *     so if f is a flag then we know av[-1] matches f->flag
 *
 * examine av[0]-av[ac-1] to determine number of parameters supplied
 *     if simpleflag, set flag parameter and read no args
 *     if subrflag, call subroutine on sub-args
 *     if sublist, call arg_parse_form on sub-args
 *     else it's a paramflag or regular arg, do arg-to-param assignments
 * return number of arguments gobbled, or negative error code
 */

int arg_do(int ac, char **av, Arg_form *f)
{
    int narg, skip, used, err, i;

    if (arg_debug)
        av_print("  arg_do", ac, av);
    if (f->type==ARG_SIMPFLAG || f->type==ARG_PARAMFLAG) {
        /* don't complain about repeated subrflags or sublists */
        assert(str_eq(av[-1], f->flag));
        f->rep++;
        if (f->rep>1 && arg_warning)
            fprintf(stderr, "warning: more than one %s flag in arglist\n",
                f->flag);
    }

    narg = nargs(ac, av, f, &skip);

    used = 0;
    switch (f->type) {
        case ARG_SIMPFLAG:
            **f->param = 1;
            break;
        case ARG_SUBRFLAG:
            (*f->subr)(narg, av);
            break;
        case ARG_SUBLISTFLAG:
            arg_parse_argv(narg+1, &av[-1], f->sublist);        /* recurse */
            used = narg;
            break;
        default:                        /* convert parameters */
            err = scan(narg, av, f);
            if (err) return err;
            used = narg<f->nparam ? narg : f->nparam;
            break;
    }

    if ((f->type==ARG_REGULAR || f->type==ARG_PARAMFLAG) && used!=narg) {
        fprintf(stderr, "warning: %d unused arg%s to %s: ",
            narg-used, narg-used>1 ? "s" : "", av[-1]);
        for (i=used; i<narg; i++)
            fprintf(stderr, "%s ", av[i]);
        fprintf(stderr, "\n");
    }
    return skip;
}

/*
 * nargs: Count number of parameters in arg vector av before the next flag.
 * Arguments can be grouped and "escaped" using -{ and -}.
 * NOTE: modifies av
 * Returns number of valid args in new av.
 * Sets *skip to number of args to skip in old av to get to next flag.
 *
 * This is the most complex code in arg_parse.
 * Is there a better way?
 *
 * examples:
 *     input:  ac=3, av=(3 4 -go)
 *     output: av unchanged, skip=2, return 2
 *
 *     input:  ac=4, av=(-{ -ch r -})
 *     output: av=(-ch r X X), skip=4, return 2
 *
 *     input:  ac=4, av=(-{ -foo -} -go)
 *     output: av=(-foo X X -go), skip=3, return 1
 *
 *     input:  ac=6, av=(-{ -ch -{ -rgb -} -})
 *     output: av=(-ch -{ -rgb -} X X), skip=6, return 4
 *
 *     where X stands for junk
 */

int nargs(int ac, char **av, Arg_form *f, int *skip)
{
    char *flag, **au, **av0;
    int i, j, level, die, np, mask, voracious;

    np = f->nparam;
    mask = f->parammask;
    flag = f->type==ARG_REGULAR ? f->format : f->flag;
    voracious = f->type==ARG_SUBRFLAG || f->type==ARG_SUBLISTFLAG;
        /* subrs&sublists want all the args they can get */

    level = 0;
    av0 = au = av;
    if (voracious) np = 999;
    for (die=0, i=0; i<np && i<ac && !die; ) {
        if (voracious) j = 999;
        else for (j=i+1; !(mask>>j&1); j++);    /* go until we can stop */
        /* try to grab params i through j-1 */
        for (; i<j && i<ac || level>0; i++, au++, av++) {
            if (au!=av) *au = *av;
            if (str_eq(*av, "-{")) {
                if (level<=0) au--;             /* skip "-{" in av if level 0 */
                level++;                        /* push a level */
            }
            else if (str_eq(*av, "-}")) {
                level--;                        /* pop a level */
                if (level<=0) au--;             /* skip "-}" in av if level 0 */
                if (level<0)
                    fprintf(stderr, "ignoring spurious -}\n");
            }
            else if (level==0 && av[0][0]=='-' && !NUMERIC(&av[0][1])) {
                die = 1;                /* break out of both loops */
                break;                  /* encountered flag at level 0 */
            }
        }
    }
    if (arg_debug) {
        fprintf(stderr, "    %s: requested %d, got %d args: ",
                flag, np, (int)(au-av0));
        for (j=0; j<au-av0; j++)
            fprintf(stderr, "%s ", av0[j]);
        fprintf(stderr, "\n");
    }
    *skip = i;
    return au-av0;
}

/*
 * scan: call sscanf to read args into param array and do conversion
 * returns error code (0 on success)
 */

int scan(int narg, char **arg, Arg_form *f)
{
    static char str[]="%X";
    char *s;
    int i, **p;
    double x;

    if (f->nparam<narg) narg = f->nparam;
    if (!(f->parammask>>narg&1)) {
        fprintf(stderr, "you can't give %s just %d params\n",
            f->format, narg);
        return ARG_MISSING;
    }
    for (p=f->param, i=0; i<narg; i++, p++) {
        str[1] = f->code[i];
        switch (str[1]) {
            case 'S':
                /*
                 * dynamically allocate memory for string
                 * for arg_parse_argv: in case argv gets clobbered (rare)
                 * for arg_parse_stream: since line[] buffer is reused (always)
                 */
                ALLOC(s, char, strlen(arg[i])+1);
                strcpy(s, arg[i]);
                *(char **)*p = s;
                break;
            case 's':           /* scanf "%s" strips leading, trailing blanks */
                strcpy((char*)*p, arg[i]);
                break;
            case 'd':
                *(int *)*p = expr_eval_int(arg[i]);
                if (expr_error==EXPR_BAD) {     /* expression is garbage */
                    fprintf(stderr, "bad %s param\n", f->flag);
                    return ARG_BADARG;
                }
                break;
            case 'D':
                *(long *)*p = expr_eval_long(arg[i]);
                if (expr_error==EXPR_BAD) {     /* expression is garbage */
                    fprintf(stderr, "bad %s param\n", f->flag);
                    return ARG_BADARG;
                }
                break;
            case 'f': case 'F':
                x = expr_eval(arg[i]);
                if (expr_error==EXPR_BAD) {     /* expression is garbage */
                    fprintf(stderr, "bad %s param\n", f->flag);
                    return ARG_BADARG;
                }
                if (str[1]=='f') *(float *)*p = x;
                else            *(double *)*p = x;
                break;
            default:
                if (sscanf(arg[i], str, *p) != 1) {
                    fprintf(stderr, "bad %s param: \"%s\" doesn't match %s\n",
                        f->flag, arg[i], str);
                    return ARG_BADARG;
                }
                break;
        }
    }
    return 0;                   /* return 0 on success */
}

static char *bar = "==================================================\n";

/* arg_form_print: print Arg_form as usage message to stderr */

void arg_form_print( Arg_form *form )
{
    Arg_form *f;

    for (f=form; f; f=f->next) {
        if (f->type!=ARG_NOP || f->format[0]) {
            fprintf(stderr, "%s", f->format);
            space(stderr, strlen(f->format), arg_doccol);
        }
        fprintf(stderr, "%s\n", f->doc);
        if (arg_debug)
            fprintf(stderr, "   %d (%s) [%s][%s]%x (%s)\n",
            f->type, f->format, f->flag, f->code, f->parammask, f->doc);
        if (f->type==ARG_SUBLISTFLAG) {
            fputs(bar, stderr);
            arg_form_print( f->sublist );
            fputs(bar, stderr);
        }
    }
}

/*
 * space: currently in column c; tab and space over to column c1
 * assumes 8-space tabs
 */

void space(FILE *fp, int c, int c1)
{
    if (c>=c1) {
        putc('\n', fp);
        c = 0;
    }
    for (; c<c1&~7; c=(c+7)&~7) putc('\t', fp);
    for (; c<c1; c++) putc(' ', fp);
}

void av_print(char *str, int ac, char **av)
{
    int i;

    fprintf(stderr, "%s: ", str);
    for (i=0; i<ac; i++)
        fprintf(stderr, "%s ", av[i]);
    fprintf(stderr, "\n");
}

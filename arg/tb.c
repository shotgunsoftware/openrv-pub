/* tb.c - arg_parse test program */

static char rcsid[] = "$Header: /home/src/cvsroot/src/pub/arg/tb.c,v 1.1 "
                      "2007/10/15 22:32:38 src Exp $";

#include <stdlib.h>
#include <stdio.h>
double atof();

#include "arg.h"
#include "expr.h"
static double dxs = 1., dys = .75;
static int x1 = 0, y1 = 0, x2 = 99, y2 = 99;
static char* chanlist = "rgba";
Arg_form* fb_init();

arg_people(ac, av) int ac;
char** av;
{
    int i;

    for (i = 0; i < ac; i++)
        printf("person[%d]=%s\n", i, av[i]);
}

arg_dsize(ac, av) int ac;
char** av;
{
    if (ac < 1 || ac > 3)
    {
        fprintf(stderr, "-dsize wants 1 or 2 args\n");
        exit(1);
    }
    /* illustrate two methods for argument conversion */
    dxs = atof(av[0]); /* constant conversion */
    if (ac > 1)
        dys = expr_eval(av[1]); /* expression conversion */
    else
        dys = .75 * dxs;
}

main(ac, av) int ac;
char** av;
{
    int fast, xs = 512, ys = 486;
    double scale = 1.;
    char *fromfile, tofile[80], *child = "jim";
    Arg_form* arg_fb;

    arg_fb = fb_init();
    if (arg_parse(ac, av, "", "Usage: %s [options]", av[0], "",
                  "This program does nothing but test arg_parse", "%S %s",
                  &fromfile, tofile, "fromfile and tofile", "[%F]", &scale,
                  "set scale [default=%g]", scale, "", ARG_SUBR(arg_people),
                  "names of people", "-fast", ARG_FLAG(&fast), "do it faster",
                  "-ch %S", &child, "set child name", "-srcsize %d[%d]", &xs,
                  &ys, "set source size [default=%d,%d]", xs, ys, "-dstsize",
                  ARG_SUBR(arg_dsize), "set dest size", "-fb",
                  ARG_SUBLIST(arg_fb), "FB COMMANDS", 0)
        < 0)
        exit(1);

    printf("from=%s to=%s scale=%g fast=%d child=%s src=%dx%d dst=%gx%g\n",
           fromfile, tofile, scale, fast, child, xs, ys, dxs, dys);
    printf("window={%d,%d,%d,%d} chan=%s\n", x1, y1, x2, y2, chanlist);
    return 0;
}

Arg_form* fb_init()
{
    return arg_to_form(NULL, "-w%d%d%d%d", &x1, &y1, &x2, &y2,
                       "set screen window", "-ch%S", &chanlist,
                       "set channels [default=%s]", chanlist, 0);
}

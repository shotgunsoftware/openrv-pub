/* : //depot/branches/rmanprod/rman-11.5/tiff/libtiff/tif_pixarfilm.c#12 $  (Pixar - RenderMan Division)  $Date: 2007/07/30 $ */
/*
 * Copyright (c) 1988, 1990 by Sam Leffler.
 * Copyright (c) 1992 by Pixar.
 * All rights reserved.
 *
 * This file is provided for unrestricted use provided that this
 * legend is included on all tape media and as a part of the
 * software program in whole or part.  Users may copy, modify or
 * distribute this file at will.
 */

/*
 * TIFF Library.
 * Modified Rev 5.0 Lempel-Ziv & Welch Compression for film output.
 *
 * This code is derived from Sam Leffler's TIFF LZW code which was
 * derived from the compress program whose code is
 * derived from software contributed to Berkeley by James A. Woods,
 * derived from original work by Spencer Thomas and Joseph Orost.
 *
 * The original Berkeley copyright notice appears at the end in its entirety.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "tiffiop.h"
/* We do our own horizontal diff, so turn off predictor stuff. */
#include "tif_predict.h"

#define REALLY_ENCODE

#define	   N(a)	(sizeof(a)/sizeof(a[0]))

#define MAXCODE(n)    ((1 << (n)) - 1)
/*
 * The TIFF spec specifies that encoded bit strings range
 * from 9 to 12 bits.  We use 11 to 14 bits since our data is 10 bits.
 */
#define    BITS_MIN    11	/* start with 11 bits */
#define    BITS_MAX    14	/* max of 14 bit strings */
/* predefined codes */
#define    CODE_CLEAR    1024	/* code to clear string table */
#define    CODE_EOI    1025	/* end-of-information code */
#define CODE_FIRST    1026	/* first free code entry */
#define    CODE_MAX    MAXCODE(BITS_MAX)
#define    HSIZE	32831    /* output hash table: 50% occupancy */
#define    HSHIFT	(15-10)

/*
 * NB: The 5.0 spec describes a different algorithm than Aldus
 *     implements.  Specifically, Aldus does code length transitions
 *     one code earlier than should be done (for real LZW).
 *     Earlier versions of this library implemented the correct
 *     LZW algorithm, but emitted codes in a bit order opposite
 *     to the TIFF spec.  Thus, to maintain compatibility w/ Aldus
 *     we interpret MSB-LSB ordered codes to be images written w/
 *     old versions of this library, but otherwise adhere to the
 *     Aldus "off by one" algorithm.
 *
 * Future revisions to the TIFF spec are expected to "clarify this issue".
 * 
 * NB: ALL compatibility with old codes has been removed.
 *
 */

/*
 * Decoding-specific state.
 */
struct code_ent {
    struct code_ent  *next;	/* link to rest of prefix string */
    short    value;		/* pixel value */
    short    length;		/* string length including this token */
    short    firstchar;	    /* first token of this string */
    short   padding[3];	    /* struct index mult is a shift */
};
struct decode {
    struct code_ent    codetab[CODE_MAX+1];
    struct code_ent    *codep;
    struct code_ent    *oldcodep;
    struct code_ent    *free_entp;
    struct code_ent    *maxcodep;
};

/*
 * Encoding-specific state.
 */
struct hash_ent {
    long    hash;
    long    code;
};
struct encode {
    u_char     *outp;	    /* output byte pointer */
    long    checkpoint;	/* point at which to clear table */
#define CHECK_GAP    10000	/* enc_ratio check interval */
    long    ratio;	    /* current compression ratio */
    long    incount;	/* (input) data bytes encoded */
    long    outcount;	/* encoded (output) bytes */
    struct hash_ent  hashtab[HSIZE];	/* hash table */
};

/*
 * State block for each open TIFF
 * file using LZW compression/decompression.
 */
typedef    struct {
    TIFFPredictorState predict;
    int    lzw_oldcode;	/* last code encountered */
    long    lzw_restart;    /* restart interrupted decode */
    u_short    lzw_nbits;	/* number of bits/code */
    u_short    lzw_stride;	/* horizontal diferencing stride */
    int    lzw_maxcode;	/* maximum code for lzw_nbits */
    int    lzw_free_ent;	/* next free entry in hash table */
    long    lzw_nextdata;    /* next few bits in the input or output buffer */
    long    lzw_nextbits;    /* # valid bits in nextdata */
    long    lzw_nbitsmask;    /* nbits 1 bits, right adjusted */
    u_short *lzw_tbuf;	/* temp buffer for accum/diff calculations */
    union {
	struct    decode dec;
	struct    encode enc;
    } u;

    short   lzw_user_datafmt;    /* type of sample data user expects */
    TIFFVSetMethod vgetparent;   /* super-class method */
    TIFFVSetMethod vsetparent;   /* super-class method */
    float * ditherseed;          /* current dither seed 
				    (also an index into the table) */
} LZWState;


#define DITHER_TABLE_SIZE	7907	/* fairly big, fairly prime */

/* Global ditherTable */
static float ditherTable[DITHER_TABLE_SIZE];      /* large shared base of random numbers */
static float * dtend;                   /* end of array of dither values */
static int ditherInitialized = 0;

#define    dec_codetab	u.dec.codetab
#define    dec_codep	u.dec.codep
#define    dec_oldcodep    u.dec.oldcodep
#define    dec_free_entp    u.dec.free_entp
#define    dec_maxcodep    u.dec.maxcodep

#define    enc_outp	u.enc.outp
#define    enc_checkpoint    u.enc.checkpoint
#define    enc_ratio	u.enc.ratio
#define    enc_incount	u.enc.incount
#define    enc_outcount    u.enc.outcount
#define    enc_hashtab	u.enc.hashtab

#define  PutNextCode(code)			\
    { nextdata = (nextdata << nbits) | code;    \
    nextbits += nbits;				\
    *bp++ = nextdata >> (nextbits-8);		\
    nextbits -= 8;		   		\
    if (nextbits >= 8)  {			\
	*bp++ = nextdata >> (nextbits-8);	\
	nextbits -= 8;				\
    }						\
    outcount += nbits;				\
    }

/*
 * Clear hash table.
 */
#define  CL_HASH				\
{				   		\
    register struct hash_ent *htab_p		\
		    = sp->enc_hashtab+HSIZE-1;  \
    register long i;				\
				   		\
    i = HSIZE - 8;				\
     do {					\
	i -= 8;					\
	htab_p[-7].hash = -1;		   	\
	htab_p[-6].hash = -1;		   	\
	htab_p[-5].hash = -1;		   	\
	htab_p[-4].hash = -1;		   	\
	htab_p[-3].hash = -1;		   	\
	htab_p[-2].hash = -1;		   	\
	htab_p[-1].hash = -1;		   	\
	htab_p[0].hash = -1;		   	\
	htab_p -= 8;				\
    } while (i >= 0);				\
    for (i += 8; i > 0; i--, htab_p--)		\
	htab_p->hash = -1;		   	\
}

/* Tables for converting to/from 10 bit coded values */
#define  TSIZE  1024	    /* decode table size    */
#define  NNEG   32	    /* negative linear part */
#define  NLIN   177	    /* linear part	  */
#define  NMID   752	    /* e^x part	     */
#define  NGE1   (TSIZE-NNEG-NLIN-NMID)  /* e^(x^2) part (63)    */

#define  NEGI	(float).0016	/* smallest negative value     */
#define  MINNEGV  (-NNEG*NEGI)  /* greatest negative value     */

/* MINV is rounded up so that it's ever so slightly larger than a bin
 * rather than ever so slightly smaller than a bin. (.00008 can 
 * not be exactly represented in floating point ) */
#define  MINV (float) .000080000002  /* minimum representable value */
#define  MAXV	(float)20.	/* maximum representable value */    

#define  FLTSIZE  12500		/* size of p<=1 encode table	 */
				/* FLTSIZE = 1./MINV		 */
#define  FGESIZE  3344		/* size of p>1 encode table	  */
				/* FGESIZE = (MAXV-1)/(1-ratio_at_1) */

static float    ToLinearF[1025];
static u_short  ToLinear16[1025];
static short    ToLinear12[1025];
static u_char   ToLinear8[1025];

#ifdef REALLY_ENCODE
static float  Fltsize;
static u_short  FromGE1[FGESIZE], FromLT1[FLTSIZE];
static u_short  From12[4096], From8[256];

static	  int PixarFilmSetupEncode(TIFF *tif);
static    int PixarFilmPreEncode(TIFF*, tsample_t);
static    int PixarFilmEncode(TIFF*, tidata_t, tsize_t, tsample_t);
static    int PixarFilmPostEncode(TIFF*);
static	  int PixarFilmEncodeBlock(TIFF *tif, u_short *wp, int wc, int *nencoded);
#endif /* REALLY_ENCODE */
static	  int PixarFilmSetupDecode(TIFF *tif);
static    int PixarFilmDecode(TIFF*, tidata_t, tsize_t, tsample_t);
static    int PixarFilmPreDecode(TIFF*, tsample_t);
static	  void PixarFilmCleanup(TIFF*);
static	  void PixarFilmMakeTables();

#define PIXARLOGDATAFMT_UNKNOWN	-1

/* amplitude is baked into the dither table in MakeDitherTable */
static float Dither(float f, float ** dtpp) {
    float * dtp = *dtpp;
    float v = f + *dtp++;
    if (dtp >= dtend) dtp = ditherTable;
    *dtpp = dtp;
    return v;
}

static int
PixarFilmGuessDataFmt(TIFFDirectory *td)
{
	int guess = PIXARLOGDATAFMT_UNKNOWN;
	int format = td->td_sampleformat;
	
	int defaultFormat = 
	    (format == SAMPLEFORMAT_VOID) || (format == SAMPLEFORMAT_UINT);
	/* If the user didn't tell us his datafmt,
	 * take our best guess from the bitspersample.
	 */
	switch (td->td_bitspersample) {
	 case 32:
	     if (defaultFormat || format==SAMPLEFORMAT_IEEEFP)
			guess = PIXARLOGDATAFMT_FLOAT;
		break;
	 case 16:
		if (defaultFormat)
			guess = PIXARLOGDATAFMT_16BIT;
		break;
	 case 12:
		if (defaultFormat || format == SAMPLEFORMAT_INT)
			guess = PIXARLOGDATAFMT_12BITPICIO;
		break;
	 case 11:
		if (defaultFormat)
			guess = PIXARLOGDATAFMT_11BITLOG;
		break;
	 case 8:
		if (defaultFormat)
			guess = PIXARLOGDATAFMT_8BIT;
		break;
	}

	return guess;
}

static int
PixarFilmVSetField(TIFF* tif, ttag_t tag, va_list ap)
{
    LZWState      *sp = (LZWState *)tif->tif_data;
    int result;

    switch (tag) {
     case TIFFTAG_PIXARLOGDATAFMT:
	sp->lzw_user_datafmt = va_arg(ap, int);
	/* Tweak the TIFF header so that the rest of libtiff knows what
	 * size of data will be passed between app and library, and
	 * assume that the app knows what it is doing and is not
	 * confused by these header manipulations...
	 */
	switch (sp->lzw_user_datafmt) {
	 case PIXARLOGDATAFMT_8BIT:
	 case PIXARLOGDATAFMT_8BITABGR:
	    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
	    TIFFSetField(tif, TIFFTAG_DATATYPE, SAMPLEFORMAT_UINT);
	    break;
	 case PIXARLOGDATAFMT_11BITLOG:
	    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
	    TIFFSetField(tif, TIFFTAG_DATATYPE, SAMPLEFORMAT_UINT);
	    break;
	 case PIXARLOGDATAFMT_12BITPICIO:
	    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
	    /* Not really, but we lie. */
	    TIFFSetField(tif, TIFFTAG_DATATYPE, SAMPLEFORMAT_INT);
	    break;
	 case PIXARLOGDATAFMT_16BIT:
	    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
	    TIFFSetField(tif, TIFFTAG_DATATYPE, SAMPLEFORMAT_INT);
	    break;
	 case PIXARLOGDATAFMT_FLOAT:
	    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 32);
	    TIFFSetField(tif, TIFFTAG_DATATYPE, SAMPLEFORMAT_IEEEFP);
	    break;
	}
	/* Update scanlinesize to match bitspersample. */
	tif->tif_scanlinesize = TIFFScanlineSize(tif);
	result = 1;		/* NB: pseudo tag */
	break;
     default:
	result = (*sp->vsetparent)(tif, tag, ap);
    }
    return (result);
}

static int
PixarFilmVGetField(TIFF* tif, ttag_t tag, va_list ap)
{
    LZWState      *sp = (LZWState *)tif->tif_data;

    switch (tag) {
     case TIFFTAG_PIXARLOGDATAFMT:
	*va_arg(ap, int *) = sp->lzw_user_datafmt;
	break;
     default:
	return (*sp->vgetparent)(tif, tag, ap);
    }
    return (1);
}

/*ARGSUSED*/
static void
PixarFilmClose(TIFF* tif)
{
#if 0
	TIFFDirectory *td = &tif->tif_dir;

	/* In a really sneaky maneuver, on close, we covertly modify both
	 * bitspersample and sampleformat in the directory to indicate
	 * 8-bit linear.  This way, the decode "just works" even for
	 * readers that don't know about PixarLog, or how to set
	 * the PIXARLOGDATFMT pseudo-tag.
	 */
	td->td_bitspersample = 8;
	td->td_sampleformat = SAMPLEFORMAT_UINT;
#endif
}

static const TIFFFieldInfo pixarfilmFieldInfo[] = {
    { TIFFTAG_PIXARLOGDATAFMT, 0, 0, TIFF_ANY, 0, FALSE, FALSE, "" }
};

/*ARGSUSED*/
int
TIFFInitPixarFilm(TIFF *tif, int scheme)
{
    LZWState      *sp = (LZWState *)tif->tif_data;

    assert(scheme == COMPRESSION_PIXARFILM);

    tif->tif_setupdecode = PixarFilmSetupDecode;
    tif->tif_predecode = PixarFilmPreDecode;
    tif->tif_decoderow = PixarFilmDecode;
    tif->tif_decodestrip = PixarFilmDecode;
    tif->tif_decodetile = PixarFilmDecode;

#ifdef REALLY_ENCODE
    tif->tif_setupencode = PixarFilmSetupEncode;
    tif->tif_preencode = PixarFilmPreEncode;
    tif->tif_postencode = PixarFilmPostEncode;
    tif->tif_encoderow = PixarFilmEncode;
    tif->tif_encodestrip = PixarFilmEncode;
    tif->tif_encodetile = PixarFilmEncode;
#endif /* REALLY_ENCODE */

    tif->tif_close = PixarFilmClose;
    tif->tif_cleanup = PixarFilmCleanup;

    /*
     * Allocate state block so tag methods have storage to record values.
     */
    tif->tif_data = (tidata_t) _TIFFmalloc(sizeof (LZWState));
    if (tif->tif_data == NULL) {
	TIFFError("TIFFInitPixarFilm", "No space for LZW state block");
	return (0);
    }
    sp = (LZWState *)tif->tif_data;
    memset(sp, 0, sizeof(LZWState));

    /* Override SetField so we can handle our private pseudo-tag */
    _TIFFMergeFieldInfo(tif, pixarfilmFieldInfo, N(pixarfilmFieldInfo));
    sp->vgetparent = tif->tif_tagmethods.vgetfield;
    tif->tif_tagmethods.vgetfield = PixarFilmVGetField;	/* hook for codec tags */
    sp->vsetparent = tif->tif_tagmethods.vsetfield;
    tif->tif_tagmethods.vsetfield = PixarFilmVSetField;	/* hook for codec tags */

    sp->lzw_user_datafmt = PIXARLOGDATAFMT_UNKNOWN;

    /* we don't wish to use the predictor,
     * the default is none.
     */
    (void) TIFFPredictorInit(tif);

    PixarFilmMakeTables();
    return (1);
}

int StrHash(char * filename, int range) 
{
    unsigned int i;
    int h = 0;

    /* string path:  we don't care WHERE the file is rendered, we
       want the seed to be the same */
    char * file = strrchr(filename, '/');
    if (file == NULL) {
	/* no path, so use filename as is */
	file = filename;
    } else {
	/* found '/' mark, advance pointer to one past that char */
	file ++;
    }

    for (i=0; i<strlen(file); i++) {
	h+=2;
	h = (h % range) + (int)floor(h/range) + (int)file[i];
    }

    return h % range;
}

#ifdef _WINDOWS
/* rand() on windows returns a max of 0x7fff */
#define RANDMASK	0x7FFF
#define RANDMASKPLUS1   0x8000
#else
#define RANDMASK	0x3FFFF
#define RANDMASKPLUS1   0x40000
#endif
static void
MakeDitherTable(int n, float maxdither)
{
	float *tp;
	float scale;

#ifdef _WINDOWS
	srand((int) 7883);
#else
	srandom((int) 7883);
#endif

	scale = maxdither/RANDMASKPLUS1;

	for (tp = ditherTable; n > 0; n--) {
#ifdef _WINDOWS
		*tp++ = scale * (rand() & RANDMASK);
#else
		*tp++ = scale * (random() & RANDMASK);		
#endif
	    }
}

#undef RANDMASK
#undef RANDMASKPLUS1
static void
PixarFilmMakeTables()
{

/*
 *  We make several tables here to convert between various external
 *  representations (float, 12-bit Pixar, and 8-bit) and the internal
 *  10-bit companded representation.  The 10-bit representation has
 *    five distinct regions.  A linear negative region from -.05120
 *  to -.0016 in steps of .0016, zero, a linear bottom end up through
 *  .01416 in steps of .00008, a central region of constant ratio up
 *    through 1.0, and a top end of quadratic ratio up to 20.0.  These
 *    floating point numbers are stored in the main table ToLinearF. 
 *    All other tables are derived from this one.  The tables (and the
 *    ratios) are continuous at all internal seams.
 */

#ifdef REALLY_ENCODE
    double dv;
#endif /* REALLY_ENCODE */
    double  k, v, r, lr2, r2;
    int  i, j;

    j = 0;
    for (i = 1; i <= NNEG; i++)  {
	v = i * NEGI;
	ToLinearF[NNEG-1-j++] = -v;
    }

    for (i = 0; i < NLIN; i++)  {
	v = i * MINV;
	ToLinearF[j++] = v;
    }

    k = -log10(v)/NMID;	 /* .00246196... */
    r = pow(10.,k);
    for (i = 0; i < NMID; i++)  {
	v *= r;
	ToLinearF[j++] = v;
    }

    lr2 = (log10(MAXV)-log10(v)-NGE1*log10(r))*2./(NGE1*(NGE1+1));
    r2 = pow (10., lr2);
    for (i = 0; i < NGE1; i++)  {
	r *= r2;
	v *= r;
	ToLinearF[j++] = v;
    }
    ToLinearF[1024] = ToLinearF[1023];

    for (i = 0; i < 1024; i++)  {
	v = ToLinearF[i]*65535. + .5;
	ToLinear16[i] = (v < 0) ? 0 : (v > 65535.) ? 65535 : v;
	v = ToLinearF[i]*2048. + .5;
	ToLinear12[i] = (v > 3071.) ? 3071 : v;
	v = ToLinearF[i]*255.  + .5;
	ToLinear8[i]  = (v < 0) ? 0 : (v > 255.) ? 255 : v;
    }

#ifdef REALLY_ENCODE
    j = NNEG;
    for (i = 0; i < FLTSIZE; i++)  {
	if ((i*MINV)*(i*MINV) > ToLinearF[j]*ToLinearF[j+1])
	    j++;
	FromLT1[i] = j;
    }

    dv = (MAXV-1.)/FGESIZE;
    v = 1.;
    for (i = 0; i < FGESIZE; i++, v+=dv)  {
	if (v*v > ToLinearF[j]*ToLinearF[j+1])
	    j++;
	FromGE1[i] = j;
    }

    j = 0;
    for (i = -1024; i < 3072; i++)  {
	while ((i/2048.)*(i/2048.) > ToLinearF[j]*ToLinearF[j+1])
	    j++;
	From12[i+1024] = j;
    }

    j = NNEG;
    for (i = 0; i < 256; i++)  {
	while ((i/255.)*(i/255.) > ToLinearF[j]*ToLinearF[j+1])
	    j++;
	From8[i] = j;
    }

    Fltsize = FLTSIZE;
#endif /* REALLY_ENCODE */
}



/*
 * LZW Decoder.
 */
static int PixarFilmSetupDecode(TIFF *tif)
{
    TIFFDirectory *td = &tif->tif_dir;
    LZWState      *sp = (LZWState *)tif->tif_data;

    if (sp->lzw_user_datafmt == PIXARLOGDATAFMT_UNKNOWN)
	sp->lzw_user_datafmt = PixarFilmGuessDataFmt(td);
    if (sp->lzw_user_datafmt == PIXARLOGDATAFMT_UNKNOWN) {
	TIFFError("PixarFilmSetupDecode", 
	    "PixarFilm compression can't handle %d bit linear encodings",
	    td->td_bitspersample);
	return (0);
    }
    return 1;
}

/*
 * Setup state for decoding a strip.
 */
/*ARGSUSED*/
static int
PixarFilmPreDecode(TIFF *tif, tsample_t x)
{
    TIFFDirectory *td = &tif->tif_dir;
    LZWState      *sp = (LZWState *)tif->tif_data;
    register int code;

    /* Make sure no byte swapping happens on the data after decompression  
     * I think that by setting this here (rather than it PixarFilmSetupDecode)
     * it will work even if people call SetField(TIFFTAG_BITSPERSAMPLE) 
     * which sets the byteswapper.
     */
    tif->tif_postdecode = _TIFFNoPostDecode;
    
    /* Update scanlinesize to match bitspersample. */
    tif->tif_scanlinesize = TIFFScanlineSize(tif);

    if (sp->lzw_tbuf == NULL) {
	sp->lzw_stride = (td->td_planarconfig == PLANARCONFIG_CONTIG ?
	    td->td_samplesperpixel : 1);
	sp->lzw_tbuf = (u_short *)
	    _TIFFmalloc(td->td_imagewidth * sp->lzw_stride * 
			td->td_rowsperstrip * sizeof(u_short));
	/*
	 * Pre-load the table.
	 */
	for (code = 0; code <= 1023; code++)  {
	    sp->dec_codetab[code].value     = code;
	    sp->dec_codetab[code].firstchar = code;
	    sp->dec_codetab[code].length    = 1;
	    sp->dec_codetab[code].next      = NULL;
	}
    } 
    sp->lzw_restart = 0;
    sp->lzw_nbits = BITS_MIN;
    sp->lzw_nextbits = 0;
    sp->lzw_nextdata = 0;
    sp->lzw_nbitsmask = MAXCODE(BITS_MIN);
    sp->dec_free_entp = sp->dec_codetab + CODE_FIRST;
    sp->dec_oldcodep = sp->dec_codetab;
    sp->dec_oldcodep--;
    sp->dec_maxcodep = sp->dec_codetab + sp->lzw_nbitsmask - 1;
    return (1);
}

static void
horizontalAccumulateF(u_short *wp, int n, int stride, float *op)
{
    register unsigned int  cr, cg, cb, ca, mask;
    register float  t0, t1, t2, t3;

    if (n > stride) {
	mask = 0x3FF;
	n -= stride;
	if (stride == 3)  {
	    t0 = ToLinearF[cr = wp[0]];
	    t1 = ToLinearF[cg = wp[1]];
	    t2 = ToLinearF[cb = wp[2]];
	    op[0] = t0;
	    op[1] = t1;
	    op[2] = t2;
	    do {
		wp += 3;
		op += 3;
		n -= 3;
		t0 = ToLinearF[(cr += wp[0]) & mask];
		t1 = ToLinearF[(cg += wp[1]) & mask];
		t2 = ToLinearF[(cb += wp[2]) & mask];
		op[0] = t0;
		op[1] = t1;
		op[2] = t2;
	    } while (n > 0); }
	else if (stride == 4)  {
	    t0 = ToLinearF[cr = wp[0]];
	    t1 = ToLinearF[cg = wp[1]];
	    t2 = ToLinearF[cb = wp[2]];
	    t3 = ToLinearF[ca = wp[3]];
	    op[0] = t0;
	    op[1] = t1;
	    op[2] = t2;
	    op[3] = t3;
	    do {
		wp += 4;
		op += 4;
		n -= 4;
		t0 = ToLinearF[(cr += wp[0]) & mask];
		t1 = ToLinearF[(cg += wp[1]) & mask];
		t2 = ToLinearF[(cb += wp[2]) & mask];
		t3 = ToLinearF[(ca += wp[3]) & mask];
		op[0] = t0;
		op[1] = t1;
		op[2] = t2;
		op[3] = t3;
	    } while (n > 0); }
	else  {
	    /* loop through channels. */
	    int j, k, m;
	    for (j=0; j<stride; j++) {
		k = j;
		m = n;
		t0 = ToLinearF[cr = wp[k]];
		op[k] = t0;
		do {
		    k += stride;
		    m -= stride;
		    t0 = ToLinearF[(cr += wp[k]) & mask];
		    op[k] = t0;
		} while (m > 0);
	    }
	}
    }
}


static void
horizontalAccumulate16(u_short *wp, int n, int stride, short *op)  
{
    register unsigned int  cr, cg, cb, ca, mask;

    if (n > stride) {
	mask = 0x3FF;
	n -= stride;
	if (stride == 3)  {
	    op[0] = ToLinear16[cr = wp[0]];
	    op[1] = ToLinear16[cg = wp[1]];
	    op[2] = ToLinear16[cb = wp[2]];
	    do {
		wp += 3;
		op += 3;
		n -= 3;
		op[0] = ToLinear16[(cr += wp[0]) & mask];
		op[1] = ToLinear16[(cg += wp[1]) & mask];
		op[2] = ToLinear16[(cb += wp[2]) & mask];
	    } while (n > 0); }
	else if (stride == 4)  {
	    op[0] = ToLinear16[cr = wp[0]];
	    op[1] = ToLinear16[cg = wp[1]];
	    op[2] = ToLinear16[cb = wp[2]];
	    op[3] = ToLinear16[ca = wp[3]];
	    do {
		wp += 4;
		op += 4;
		n -= 4;
		op[0] = ToLinear16[(cr += wp[0]) & mask];
		op[1] = ToLinear16[(cg += wp[1]) & mask];
		op[2] = ToLinear16[(cb += wp[2]) & mask];
		op[3] = ToLinear16[(ca += wp[3]) & mask];
	    } while (n > 0); }
	else  {
	    /* loop through channels. */
	    int j, k, m;
	    for (j=0; j<stride; j++) {
		k = j;
		m = n;
		op[k] = ToLinear16[cr = wp[k]];
		do {
		    k += stride;
		    m -= stride;
		    op[k] = ToLinear16[(cr += wp[k]) & mask];
		} while (m > 0);
	    }
	}
    }
}


static void
horizontalAccumulate12(u_short *wp, int n, int stride, short *op)  
{
    register unsigned int  cr, cg, cb, ca, mask;

    if (n > stride) {
	mask = 0x3FF;
	n -= stride;
	if (stride == 3)  {
	    op[0] = ToLinear12[cr = wp[0]];
	    op[1] = ToLinear12[cg = wp[1]];
	    op[2] = ToLinear12[cb = wp[2]];
	    do {
		wp += 3;
		op += 3;
		n -= 3;
		op[0] = ToLinear12[(cr += wp[0]) & mask];
		op[1] = ToLinear12[(cg += wp[1]) & mask];
		op[2] = ToLinear12[(cb += wp[2]) & mask];
	    } while (n > 0); }
	else if (stride == 4)  {
	    op[0] = ToLinear12[cr = wp[0]];
	    op[1] = ToLinear12[cg = wp[1]];
	    op[2] = ToLinear12[cb = wp[2]];
	    op[3] = ToLinear12[ca = wp[3]];
	    do {
		wp += 4;
		op += 4;
		n -= 4;
		op[0] = ToLinear12[(cr += wp[0]) & mask];
		op[1] = ToLinear12[(cg += wp[1]) & mask];
		op[2] = ToLinear12[(cb += wp[2]) & mask];
		op[3] = ToLinear12[(ca += wp[3]) & mask];
	    } while (n > 0); }
	else  {
	    /* loop through channels. */
	    int j, k, m;
	    for (j=0; j<stride; j++) {
		k = j;
		m = n;
		op[k] = ToLinear12[cr = wp[k]];
		do {
		    k += stride;
		    m -= stride;
		    op[k] = ToLinear12[(cr += wp[k]) & mask];
		} while (m > 0);
	    }
	}
    }
}


static void
horizontalAccumulate10(u_short *wp, int n, int stride, u_short *op)
{
    register unsigned int  cr, cg, cb, ca, mask;

    if (n > stride) {
	mask = 0x3FF;
	n -= stride;
	if (stride == 3)  {
	    op[0] = cr = wp[0];  op[1] = cg = wp[1];  op[2] = cb = wp[2];
	    do {
		wp += 3;
		op += 3;
		n -= 3;
		op[0] = (cr += wp[0]) & mask;
		op[1] = (cg += wp[1]) & mask;
		op[2] = (cb += wp[2]) & mask;
	    } while (n > 0);  }
	else if (stride == 4)  {
	    op[0] = cr = wp[0];  op[1] = cg = wp[1];
	    op[2] = cb = wp[2];  op[3] = ca = wp[3];
	    do {
		wp += 4;
		op += 4;
		n -= 4;
		op[0] = (cr += wp[0]) & mask;
		op[1] = (cg += wp[1]) & mask;
		op[2] = (cb += wp[2]) & mask;
		op[3] = (ca += wp[3]) & mask;
	    } while (n > 0);  }
	else  {
	    /* loop through channels. */
	    int j, k, m;
	    for (j=0; j<stride; j++) {
		k = j;
		m = n;
		op[k] = cr = wp[k];
		do {
		    k += stride;
		    m -= stride;
		    op[k] = (cr += wp[k]) & mask;
		} while (m > 0);
	    }
	}
    }
}

static void
horizontalAccumulate8(u_short *wp, int n, int stride, u_char *op)
{
    register unsigned int  cr, cg, cb, ca, mask;

    if (n > stride) {
	mask = 0x3FF;
	n -= stride;
	if (stride == 3)  {
	    op[0] = ToLinear8[cr = wp[0]];
	    op[1] = ToLinear8[cg = wp[1]];
	    op[2] = ToLinear8[cb = wp[2]];
	    do {
		n -= 3;
		wp += 3;
		op += 3;
		op[0] = ToLinear8[(cr += wp[0]) & mask];
		op[1] = ToLinear8[(cg += wp[1]) & mask];
		op[2] = ToLinear8[(cb += wp[2]) & mask];
	    } while (n > 0);  }
	else if (stride == 4)  {
	    op[0] = ToLinear8[cr = wp[0]];
	    op[1] = ToLinear8[cg = wp[1]];
	    op[2] = ToLinear8[cb = wp[2]];
	    op[3] = ToLinear8[ca = wp[3]];
	    do {
		n -= 4;
		wp += 4;
		op += 4;
		op[0] = ToLinear8[(cr += wp[0]) & mask];
		op[1] = ToLinear8[(cg += wp[1]) & mask];
		op[2] = ToLinear8[(cb += wp[2]) & mask];
		op[3] = ToLinear8[(ca += wp[3]) & mask];
	    } while (n > 0);  }
	else  {
	    /* loop through channels. */
	    int j, k, m;
	    for (j=0; j<stride; j++) {
		k = j;
		m = n;
		op[k] = ToLinear8[cr = wp[k]];
		do {
		    k += stride;
		    m -= stride;
		    op[k] = ToLinear8[(cr += wp[k]) & mask];
		} while (m > 0);
	    }
	}
    }
}


static void
horizontalAccumulate8abgr(u_short *wp, int n, int stride, u_char *op)
{
    register unsigned int  cr, cg, cb, ca, mask;
    register u_char  t0, t1, t2, t3;

    if (n > stride) {
	mask = 0x3FF;
	n -= stride;
	if (stride == 3)  {
	    op[0] = 0;
	    t1 = ToLinear8[cb = wp[2]];
	    t2 = ToLinear8[cg = wp[1]];
	    t3 = ToLinear8[cr = wp[0]];
	    op[1] = t1;
	    op[2] = t2;
	    op[3] = t3;
	    do {
		n -= 3;
		wp += 3;
		op += 4;
		op[0] = 0;
		t1 = ToLinear8[(cb += wp[2]) & mask];
		t2 = ToLinear8[(cg += wp[1]) & mask];
		t3 = ToLinear8[(cr += wp[0]) & mask];
		op[1] = t1;
		op[2] = t2;
		op[3] = t3;
	    } while (n > 0);  }
	else if (stride == 4)  {
	    t0 = ToLinear8[ca = wp[3]];
	    t1 = ToLinear8[cb = wp[2]];
	    t2 = ToLinear8[cg = wp[1]];
	    t3 = ToLinear8[cr = wp[0]];
	    op[0] = t0;
	    op[1] = t1;
	    op[2] = t2;
	    op[3] = t3;
	    do {
		n -= 4;
		wp += 4;
		op += 4;
		t0 = ToLinear8[(ca += wp[3]) & mask];
		t1 = ToLinear8[(cb += wp[2]) & mask];
		t2 = ToLinear8[(cg += wp[1]) & mask];
		t3 = ToLinear8[(cr += wp[0]) & mask];
		op[0] = t0;
		op[1] = t1;
		op[2] = t2;
		op[3] = t3;
	    } while (n > 0);  }
	else  {
	    /* no special channel swapping for other than rgb, rgba */
	    /* loop through channels. */
	    int j, k, m;
	    for (j=0; j<stride; j++) {
		k = j;
		m = n;
		op[k] = ToLinear8[cr = wp[k]];
		do {
		    k += stride;
		    m -= stride;
		    op[k] = ToLinear8[(cr += wp[k]) & mask];
		} while (m > 0);
	    }
	}
    }
}


/*
 * Decode the next scanline.
 */
/*ARGSUSED*/
static int
PixarFilmDecode(TIFF *tif, tidata_t out, tsize_t opc0, tsample_t x)
{
    register LZWState *sp = (LZWState *)tif->tif_data;
    TIFFDirectory *td = &tif->tif_dir;
    register u_char  *bp, *bplimit;
    register int  opc;
    register int  code;
    register int  nbits, nextbits;
    register int  residue, t;
    register long  nextdata, nbitsmask;
    register u_short  *op, *tp;
    register struct code_ent  *codep, *free_entp, *maxcodep, *oldcodep;
    int llen, ssize;

    op = sp->lzw_tbuf;
    llen = td->td_imagewidth * sp->lzw_stride;
    switch (sp->lzw_user_datafmt)  {
     case PIXARLOGDATAFMT_FLOAT:
	ssize = 4; break;
     case PIXARLOGDATAFMT_16BIT:
     case PIXARLOGDATAFMT_12BITPICIO:
     case PIXARLOGDATAFMT_11BITLOG:
	ssize = 2; break;
     case PIXARLOGDATAFMT_8BIT:
     case PIXARLOGDATAFMT_8BITABGR:
     default:
	ssize = 1; break;
    }
    opc0 /= ssize;

    /*
     * Restart interrupted output operation.
     */
    opc = opc0;
    if (sp->lzw_restart) {
	codep = sp->dec_codep;
	residue = codep->length - sp->lzw_restart;
	if (residue > opc)  {	    /* it still doesn't fit */
	    sp->lzw_restart += opc;	/* add what we're about to output */
	    do  {			/* skip down to start of data */
		codep = codep->next;
	    }  while (--residue > opc);
	    tp = op + opc;
	    do  {
		opc--;
		*--tp = codep->value;
		codep = codep->next;
	    }  while (opc > 0);
	    return (1);  }
	else  {			    /* it fits ok */
	    tp = (op += residue);
	    opc -= residue;
	    do  {
		*--tp = codep->value;
		codep = codep->next;
	    }  while (--residue);
	    sp->lzw_restart = 0;
	}
    }
    bp	= (u_char *) tif->tif_rawcp;
    bplimit    = (u_char *) tif->tif_rawdata + tif->tif_rawdatasize;
    nbits     = sp->lzw_nbits;
    nextdata  = sp->lzw_nextdata;
    nextbits  = sp->lzw_nextbits;
    nbitsmask = sp->lzw_nbitsmask;
    oldcodep  = sp->dec_oldcodep;
    free_entp = sp->dec_free_entp;
    maxcodep  = sp->dec_maxcodep;

#define GetNextCode	{					\
	    nextdata = (nextdata << 8) | *bp++;			\
	    nextbits += 8;					\
	    if (nextbits < nbits)  {				\
		nextdata = (nextdata << 8) | *bp++;		\
		nextbits += 8;					\
	    }							\
	    nextbits -= nbits;					\
	    code = (nextdata >> nextbits) & nbitsmask;		\
    }

    while (opc > 0) {
	if (bp >= bplimit) {
	    TIFFError(tif->tif_name,
	    "PixarFilmDecode: early end of data at scanline %d",
		tif->tif_row);
	    return (0);
	}
	GetNextCode
	if (code == CODE_EOI)
	    break;

	if (code == CODE_CLEAR) {
	    free_entp = sp->dec_codetab + CODE_FIRST;
	    nbits = BITS_MIN;
	    nbitsmask = MAXCODE(BITS_MIN);
	    maxcodep = sp->dec_codetab + nbitsmask - 1;

	    GetNextCode
	    if (code == CODE_EOI)
		break;
	    *op++ = code, opc--;
	    oldcodep = sp->dec_codetab + code;
	    continue;
	}

	codep = sp->dec_codetab + code;
	if (codep > free_entp) {
	    TIFFError(tif->tif_name,
	    "PixarFilmDecode: Illegal token [%d] in scanline %d",
		code, tif->tif_row);
	    return (0);
	}

	/*
	 * Add the new entry to the code table.
	 */
	free_entp->next      = oldcodep;
	free_entp->firstchar = free_entp->next->firstchar;
	free_entp->length    = free_entp->next->length+1;
	free_entp->value     = (codep < free_entp)
			      ? codep->firstchar
			      : free_entp->firstchar;
	free_entp++;

	if (free_entp > maxcodep) {
	    nbits++;
	    if (nbits > BITS_MAX)	/* should never happen */
		nbits = BITS_MAX;
	    nbitsmask = MAXCODE(nbits);
	    maxcodep = sp->dec_codetab + nbitsmask - 1;
	}

	oldcodep = codep;

	if (code < 512)  {	/* a pixel value */
	    *op++ = code;
	    opc--;  }
	else  {
	    /*
	     * Generate output string (written in reverse).
	     */
	    if (codep->length > opc)  {	/* it doesn't fit */
		sp->dec_codep = codep;
		do  {			/* skip down to what fits */
		    codep = codep->next;
		}  while (codep->length > opc);
		sp->lzw_restart = opc;	/* save how many we did */
		tp = op + opc;
		do  {
		    opc--;
		    *--tp = codep->value;
		    codep = codep->next;
		}  while (opc > 0);
		break;  }
	    else  {			    /* it fits ok */
		tp = (op += codep->length);
		opc -= codep->length;
		do  {
		    --tp;
		    t = codep->value;
		    codep = codep->next;
		    *tp = t;
		}  while (codep);
	    }
	}
    }
    tif->tif_rawcp    = (tidata_t) bp;
    sp->lzw_nbits     = nbits;
    sp->lzw_nextdata  = nextdata;
    sp->lzw_nextbits  = nextbits;
    sp->lzw_nbitsmask = nbitsmask;
    sp->dec_oldcodep  = oldcodep;
    sp->dec_free_entp = free_entp;
    sp->dec_maxcodep  = maxcodep;

 { int i;
      for (i=0; i< opc0; i+=llen) 
    switch (sp->lzw_user_datafmt)  {
    case PIXARLOGDATAFMT_FLOAT:
      for (i=0; i< opc0; i+=llen) 
	horizontalAccumulateF     (sp->lzw_tbuf + i, llen, sp->lzw_stride,
				  (float *)out + i);
	break;
    case PIXARLOGDATAFMT_16BIT:
      for (i=0; i< opc0; i+=llen) 
	horizontalAccumulate16    (sp->lzw_tbuf + i, llen, sp->lzw_stride,
				  (short *)out + i);
	break;
    case PIXARLOGDATAFMT_12BITPICIO:
      for (i=0; i< opc0; i+=llen) 
	horizontalAccumulate12    (sp->lzw_tbuf + i, llen, sp->lzw_stride,
				  (short *)out + i);
	break;
    case PIXARLOGDATAFMT_11BITLOG:
      for (i=0; i< opc0; i+=llen) 
	horizontalAccumulate10    (sp->lzw_tbuf + i, llen, sp->lzw_stride,
				  (u_short *)out + i);
	break;
    case PIXARLOGDATAFMT_8BIT:
      for (i=0; i< opc0; i+=llen) 
	horizontalAccumulate8     (sp->lzw_tbuf + i, llen, sp->lzw_stride,
				  (u_char *)out + i);
	break;
    case PIXARLOGDATAFMT_8BITABGR:
      for (i=0; i< opc0; i+=llen) 
      	horizontalAccumulate8abgr (sp->lzw_tbuf + i, llen, sp->lzw_stride,
				  (u_char *)out + i);
	break;
    default:
	TIFFError(tif->tif_name,
	"PixarFilmDecode: unsupported bits/sample: %d", td->td_bitspersample);
	return (0);
    }
  }

    if (opc > 0) {
	TIFFError(tif->tif_name,
	"PixarFilmDecode: Not enough data for scanline %d (decode %d bytes)",
	    tif->tif_row, opc*2);
	return (0);
    }
    return (1);
}


#ifdef REALLY_ENCODE
/*
 * LZW Encoding.
 */

static int PixarFilmSetupEncode(TIFF *tif)
{
    int seed;
    char * filename;
    TIFFDirectory *td = &tif->tif_dir;
    LZWState      *sp = (LZWState *)tif->tif_data;

    /* Create Dither Table if not already done so */
    if (ditherInitialized == 0) {
	MakeDitherTable(DITHER_TABLE_SIZE, MINV);
	dtend = ditherTable + DITHER_TABLE_SIZE;
	ditherInitialized = 1;
    }

    /* make seed off of filename so rerendering the same frame
       will have the same dither 
    */
    filename = (char *)TIFFFileName(tif);
    
    /* second arg is range of seed values, just a large prime number */
    seed = StrHash(filename, DITHER_TABLE_SIZE);
#ifdef _WINDOWS
    srand(seed);
    sp->ditherseed = ditherTable + (rand() & 0x7FFFFFFF) % DITHER_TABLE_SIZE;
#else
    srandom(seed);
    sp->ditherseed = ditherTable + (random() & 0x7FFFFFFF) % DITHER_TABLE_SIZE;
#endif
    if (sp->lzw_user_datafmt == PIXARLOGDATAFMT_UNKNOWN)
	sp->lzw_user_datafmt = PixarFilmGuessDataFmt(td);
    if (sp->lzw_user_datafmt == PIXARLOGDATAFMT_UNKNOWN) {
	TIFFError("PixarFilmSetupEncode", 
	    "PixarFilm compression can't handle %d bit linear encodings",
	    td->td_bitspersample);
	return (0);
    }
    return 1;
}
/*
 * Reset encoding state at the start of a strip.
 */
/*ARGSUSED*/
static int
PixarFilmPreEncode(TIFF *tif, tsample_t x)
{
    TIFFDirectory *td = &tif->tif_dir;
    LZWState      *sp = (LZWState *)tif->tif_data;

    /* Update scanlinesize to match bitspersample. */
    tif->tif_scanlinesize = TIFFScanlineSize(tif);

    if (sp->lzw_tbuf == NULL) {
	sp->lzw_restart = 0;
	sp->lzw_stride = (td->td_planarconfig == PLANARCONFIG_CONTIG ?
	    td->td_samplesperpixel : 1);
	sp->lzw_tbuf = (u_short *)
	    _TIFFmalloc(td->td_imagewidth * sp->lzw_stride * 
			td->td_rowsperstrip * sizeof(u_short));
    }
    sp->enc_ratio = 0;
    sp->enc_outcount = 0;
    sp->enc_incount = 0;
    sp->enc_checkpoint = CHECK_GAP;
    sp->enc_outp = (u_char *) tif->tif_rawdata;
    sp->lzw_nbits = BITS_MIN;
    sp->lzw_maxcode = MAXCODE(BITS_MIN);
    sp->lzw_free_ent = CODE_FIRST;
    sp->lzw_nextbits = 0;
    sp->lzw_nextdata = 0;
    CL_HASH;		/* clear hash table */
    sp->lzw_oldcode = -1;    /* generates CODE_CLEAR in PixarFilmEncode */
    return (1);
}

static int ClampAndDither(float v, float ** dtpp) {
    int l;
 
    if (v<(float)0.) {
        l = (v < MINNEGV) ? 0: NNEG+(int)(v/NEGI);
    } else if (v < (float)1.) {
        /* Only dither in the low-end linear section. */
        if (v < (float)NLIN / Fltsize) {
            v = Dither(v, dtpp);
	    l = (int) (v*Fltsize);
	} else { /* else center the value in the bucket */
	    v += .49999 * MINV;
	    l = (int) (v*Fltsize);
	    if (l >= FLTSIZE)
	      l = FLTSIZE-1;
	}
        l = FromLT1[l];
    } else if (v < (float)20.) {
        l = FromGE1[(int)((v-(float)1.)*FGESIZE/(float)19.)];
    } else {      
        l = 1023;
    }
    return l;
}


static void
horizontalDifferenceF(float *ip, int n, int stride, u_short *wp,
		      float ** ditherSeedp)
{

    register int  r1, g1, b1, a1, r2, g2, b2, a2, mask;
    float * dtp;
    float * ditherseed = *ditherSeedp;
    mask = 0x3FF;
    n -= stride;
    
    /* Initialize pointer into global dither table - the
       1.25e4 is to account for the fact that all values in 
       the ditherTable are between 0 and 8e-5.  To avoid 
       having to check dtp for wraparound, we use a value
       slightly less than 1.25e4. */

    dtp = ditherTable + (int) (*ditherseed * 1.24e4 * DITHER_TABLE_SIZE);

    ditherseed++;
    if (ditherseed >= dtend) ditherseed = ditherTable;
    *ditherSeedp = ditherseed;

    if (stride == 3)  {
	r2 = wp[0] = ClampAndDither(ip[0], &dtp);  g2 = wp[1] = ClampAndDither(ip[1], &dtp);
	b2 = wp[2] = ClampAndDither(ip[2], &dtp);
	do {
	    n -= 3;
	    r1 = ClampAndDither(ip[3], &dtp); wp[3] = (r1-r2) & mask; r2 = r1;
	    g1 = ClampAndDither(ip[4], &dtp); wp[4] = (g1-g2) & mask; g2 = g1;
	    b1 = ClampAndDither(ip[5], &dtp); wp[5] = (b1-b2) & mask; b2 = b1;
	    wp += 3;
	    ip += 3;
	} while (n > 0);  }
    else if (stride == 4)  {
	r2 = wp[0] = ClampAndDither(ip[0], &dtp);  g2 = wp[1] = ClampAndDither(ip[1], &dtp);
	b2 = wp[2] = ClampAndDither(ip[2], &dtp);  a2 = wp[3] = ClampAndDither(ip[3], &dtp);
	do {
	    n -= 4;
	    r1 = ClampAndDither(ip[4], &dtp); wp[4] = (r1-r2) & mask; r2 = r1;
	    g1 = ClampAndDither(ip[5], &dtp); wp[5] = (g1-g2) & mask; g2 = g1;
	    b1 = ClampAndDither(ip[6], &dtp); wp[6] = (b1-b2) & mask; b2 = b1;
	    a1 = ClampAndDither(ip[7], &dtp); wp[7] = (a1-a2) & mask; a2 = a1;
	    wp += 4;
	    ip += 4;
	} while (n > 0);  }
    else  {
	/* loop through channels. */
	int j, k, m;
	for (j=0; j<stride; j++) {
	    k = j;
	    m = n;
	    r2 = wp[k] = ClampAndDither(ip[k], &dtp);
	    do {
		k += stride;
		m -= stride;
		r1 = ClampAndDither(ip[k], &dtp);
		wp[k] = (r1-r2) & mask;
		r2 = r1;
	    } while (m > 0);
	}
    }
}


static void
horizontalDifference16(u_short *ip, int n, int stride, u_short *wp)
{
    register int  r1, g1, b1, a1, r2, g2, b2, a2, mask;

#undef   CLAMP
#define  CLAMP(v) ( From12[(((((v)>>3)+1)>>1)&0xFFF)+1024] )

    mask = 0x3FF;
    n -= stride;

    if (stride == 3)  {
	r2 = wp[0] = CLAMP(ip[0]);  g2 = wp[1] = CLAMP(ip[1]);
	b2 = wp[2] = CLAMP(ip[2]);
	do {
	    n -= 3;
	    r1 = CLAMP(ip[3]); wp[3] = (r1-r2) & mask; r2 = r1;
	    g1 = CLAMP(ip[4]); wp[4] = (g1-g2) & mask; g2 = g1;
	    b1 = CLAMP(ip[5]); wp[5] = (b1-b2) & mask; b2 = b1;
	    wp += 3;
	    ip += 3;
	} while (n > 0);  }
    else if (stride == 4)  {
	r2 = wp[0] = CLAMP(ip[0]);  g2 = wp[1] = CLAMP(ip[1]);
	b2 = wp[2] = CLAMP(ip[2]);  a2 = wp[3] = CLAMP(ip[3]);
	do {
	    n -= 4;
	    r1 = CLAMP(ip[4]); wp[4] = (r1-r2) & mask; r2 = r1;
	    g1 = CLAMP(ip[5]); wp[5] = (g1-g2) & mask; g2 = g1;
	    b1 = CLAMP(ip[6]); wp[6] = (b1-b2) & mask; b2 = b1;
	    a1 = CLAMP(ip[7]); wp[7] = (a1-a2) & mask; a2 = a1;
	    wp += 4;
	    ip += 4;
	} while (n > 0);  }
    else  {
	/* loop through channels. */
	int j, k, m;
	for (j=0; j<stride; j++) {
	    k = j;
	    m = n;
	    r2 = wp[k] = CLAMP(ip[k]);
	    do {
		k += stride;
		m -= stride;
		r1 = CLAMP(ip[k]);
		wp[k] = (r1-r2) & mask;
		r2 = r1;
	    } while (m > 0);
	}
    }
}


static void
horizontalDifference12(short *ip, int n, int stride, u_short *wp)
{
    register int  r1, g1, b1, a1, r2, g2, b2, a2, mask;

#undef   CLAMP
#define  CLAMP(v) ( (v<-1024) ? 0		\
		  : (v<=3071) ? From12[v+1024]	\
		  : 1023)			\

    mask = 0x3FF;
    n -= stride;

    if (stride == 3)  {
	r2 = wp[0] = CLAMP(ip[0]);  g2 = wp[1] = CLAMP(ip[1]);
	b2 = wp[2] = CLAMP(ip[2]);
	do {
	    n -= 3;
	    r1 = CLAMP(ip[3]); wp[3] = (r1-r2) & mask; r2 = r1;
	    g1 = CLAMP(ip[4]); wp[4] = (g1-g2) & mask; g2 = g1;
	    b1 = CLAMP(ip[5]); wp[5] = (b1-b2) & mask; b2 = b1;
	    wp += 3;
	    ip += 3;
	} while (n > 0);  }
    else if (stride == 4)  {
	r2 = wp[0] = CLAMP(ip[0]);  g2 = wp[1] = CLAMP(ip[1]);
	b2 = wp[2] = CLAMP(ip[2]);  a2 = wp[3] = CLAMP(ip[3]);
	do {
	    n -= 4;
	    r1 = CLAMP(ip[4]); wp[4] = (r1-r2) & mask; r2 = r1;
	    g1 = CLAMP(ip[5]); wp[5] = (g1-g2) & mask; g2 = g1;
	    b1 = CLAMP(ip[6]); wp[6] = (b1-b2) & mask; b2 = b1;
	    a1 = CLAMP(ip[7]); wp[7] = (a1-a2) & mask; a2 = a1;
	    wp += 4;
	    ip += 4;
	} while (n > 0);  }
    else  {
	/* loop through channels. */
	int j, k, m;
	for (j=0; j<stride; j++) {
	    k = j;
	    m = n;
	    r2 = wp[k] = CLAMP(ip[k]);
	    do {
		k += stride;
		m -= stride;
		r1 = CLAMP(ip[k]);
		wp[k] = (r1-r2) & mask;
		r2 = r1;
	    } while (m > 0);
	}
    }
}


static void
horizontalDifference8(u_char *ip, int n, int stride, u_short *wp)
{
    register int  r1, g1, b1, a1, r2, g2, b2, a2, mask;

#undef   CLAMP
/* No < 0 test because v is unsigned */
#define  CLAMP(v) ((v<=255) ? From8[v]	: 1023)

    mask = 0x3FF;
    n -= stride;
    
    if (stride == 3)  {
	r2 = wp[0] = From8[ip[0]];  g2 = wp[1] = From8[ip[1]];
	b2 = wp[2] = From8[ip[2]];
	do {
	    n -= 3;
	    r1 = From8[ip[3]]; wp[3] = (r1-r2) & mask; r2 = r1;
	    g1 = From8[ip[4]]; wp[4] = (g1-g2) & mask; g2 = g1;
	    b1 = From8[ip[5]]; wp[5] = (b1-b2) & mask; b2 = b1;
	    wp += 3;
	    ip += 3;
	} while (n > 0);  }
    else if (stride == 4)  {
	r2 = wp[0] = From8[ip[0]];  
	g2 = wp[1] = From8[ip[1]];
	b2 = wp[2] = From8[ip[2]];  
	a2 = wp[3] = From8[ip[3]];
	do {
	    n -= 4;
	    r1 = From8[ip[4]]; wp[4] = (r1-r2) & mask; r2 = r1;
	    g1 = From8[ip[5]]; wp[5] = (g1-g2) & mask; g2 = g1;
	    b1 = From8[ip[6]]; wp[6] = (b1-b2) & mask; b2 = b1;
	    a1 = From8[ip[7]]; wp[7] = (a1-a2) & mask; a2 = a1;
	    wp += 4;
	    ip += 4;
	} while (n > 0);  }
    else  {
	/* loop through channels. */
	int j, k, m;
	for (j=0; j<stride; j++) {
	    k = j;
	    m = n;
	    r2 = wp[k] = From8[ip[k]];  
	    do {
		k += stride;
		m -= stride;
		r1 = From8[ip[k]];
		wp[k] = (r1-r2) & mask;
		r2 = r1;
	    } while (m > 0);
	}
    }
}
/*
 * Encode a scanline of pixels.
 *
 * Uses an open addressing double hashing (no chaining) on the 
 * prefix code/next character combination.  We do a variant of
 * Knuth's algorithm D (vol. 3, sec. 6.4) along with G. Knott's
 * relatively-prime secondary probe.  Here, the modular division
 * first probe is gives way to a faster exclusive-or manipulation. 
 * Also do block compression with an adaptive reset, whereby the
 * code table is cleared when the compression ratio decreases,
 * but after the table fills.  The variable-length output codes
 * are re-sized at this point, and a CODE_CLEAR is generated
 * for the decoder. 
 */
/*ARGSUSED*/
static int
PixarFilmEncode(TIFF *tif, tidata_t cp, tsize_t n, tsample_t x)
{
    LZWState      *sp = (LZWState *)tif->tif_data;
    TIFFDirectory *td = &tif->tif_dir;
    int  nencoded;
    u_short  *tp;
    int i;
    int llen;

    llen = td->td_imagewidth * sp->lzw_stride;

    switch (sp->lzw_user_datafmt)  {
    case PIXARLOGDATAFMT_FLOAT:
	n /= sizeof(float );
	break;
    case PIXARLOGDATAFMT_16BIT:
	n /= sizeof(u_short );
	break;
    case PIXARLOGDATAFMT_12BITPICIO:
	n /= sizeof(short );
	break;
    case PIXARLOGDATAFMT_8BIT:
	n /= sizeof(u_char );
	break;
    }

    for (i=0; i < n; i+=llen) 
    {
	switch (sp->lzw_user_datafmt)  {
	case PIXARLOGDATAFMT_FLOAT:
	    horizontalDifferenceF ((float *)cp + i,  llen, 
				   sp->lzw_stride, sp->lzw_tbuf + i, 
				   &sp->ditherseed);
	    break;
	case PIXARLOGDATAFMT_16BIT:
	    horizontalDifference16((u_short *)cp + i, llen,
				   sp->lzw_stride, sp->lzw_tbuf + i);
	    break;
	case PIXARLOGDATAFMT_12BITPICIO:
	    horizontalDifference12((short *)cp + i,  llen,
				   sp->lzw_stride, sp->lzw_tbuf + i);
	    break;
	case PIXARLOGDATAFMT_8BIT:
	    horizontalDifference8 ((u_char *)cp + i, llen,
				   sp->lzw_stride, sp->lzw_tbuf + i);
	    break;
	}
    }

    tp = sp->lzw_tbuf;
    

    while (n > 0)  {
	if (PixarFilmEncodeBlock (tif, tp, n, &nencoded))  {
	    sp->enc_outp = (u_char *)tif->tif_rawdata;    /* reset output pointer */
	    TIFFFlushData1 (tif);		/* buffer full */
	}
	tp += nencoded;
	n  -= nencoded;
    }
    return 1;
}

/******************************************************/

static int
PixarFilmEncodeBlock(TIFF *tif, u_short *wp, int wc, int *nencoded)
{

    LZWState *sp = (LZWState *)tif->tif_data;
    int  wc0 = wc;
    register long  fcode;
    register int  h, c, ent, disp;
    register u_char  *bp, *bplimit;
    register long  nbits, nextdata, nextbits;
    register long  incount, outcount, checkpoint, free_ent, maxcode;
    register long  rat;
    register struct hash_ent  *hbase, *hp;

    ent	= sp->lzw_oldcode;
    bp	 = (u_char *) sp->enc_outp;
    bplimit    = (u_char *) tif->tif_rawdata + tif->tif_rawdatasize - 6;
    incount    = sp->enc_incount;
    outcount   = sp->enc_outcount;
    checkpoint = sp->enc_checkpoint;
    nextdata   = sp->lzw_nextdata;
    nextbits   = sp->lzw_nextbits;
    nbits      = sp->lzw_nbits;
    free_ent   = sp->lzw_free_ent;
    maxcode    = sp->lzw_maxcode;
    hbase      = sp->enc_hashtab;

    if (ent == -1 && wc > 0) {
	PutNextCode(CODE_CLEAR);
	ent = *wp++; wc--; incount++;
    }
    while (wc > 0) {
	c = *wp++;
	wc--;
	incount++;
	fcode = ((long)c << BITS_MAX) + ent;
	h = (c << HSHIFT) ^ ent;    /* xor hashing */
	hp = hbase + h;
	if (hp->hash != fcode) {
	    if (hp->hash >= 0) {
		/*
		 * Primary hash failed, check secondary hash.
		 */
		disp = HSIZE - h;
		if (h == 0)
		    disp = 1;
		do {
		    if ((hp-=disp) < hbase)
			hp += HSIZE;
		    if (hp->hash == fcode) {
			ent = hp->code;
			/* continue; */
			goto hit;
		    }
		} while (hp->hash >= 0);
	    }
	    /*
	     * New entry, emit code and add to table.
	     */
	    PutNextCode(ent);
	    ent = c;
	    hp->code = free_ent++;
	    hp->hash = fcode;
	    if (free_ent == CODE_MAX-1) {
		/* table is full, emit clear code and reset */
		sp->enc_ratio = 0;
		CL_HASH;
		free_ent = CODE_FIRST;
		PutNextCode(CODE_CLEAR);
		nbits = BITS_MIN;
		maxcode = MAXCODE(BITS_MIN);
	    } else {
    
		/*
		 * If the next entry is going to be too big for
		 * the code size, then increase it, if possible.
		 */
		if (free_ent > maxcode) {
		    nbits++;
		    maxcode = MAXCODE(nbits);
		}  else if (incount >= checkpoint)  {
		    /*
		     * Check compression ratio and, if things seem to
		     * be slipping, clear the hash table and reset state.
		     */
		    checkpoint = incount + CHECK_GAP;
		    if (incount > 0x007fffff) {    /* shift will overflow */
			rat = outcount >> 8;
			rat = (rat == 0 ? 0x7fffffff : incount / rat);
		    } else
			rat = (incount<<8)/outcount; /* 8 fr bits */
		    if (rat <= sp->enc_ratio) {
			sp->enc_ratio = 0;
			CL_HASH;
			free_ent = CODE_FIRST;
			PutNextCode(CODE_CLEAR);
			nbits = BITS_MIN;
			maxcode = MAXCODE(BITS_MIN);  }
		    else
			sp->enc_ratio = rat;
		}
	    }
	    if (bp > bplimit)
		break;
hit:;
	} else
	    ent = hp->code;
    }
    sp->lzw_oldcode = ent;
    sp->lzw_nextdata = nextdata;
    sp->lzw_nextbits = nextbits;
    sp->lzw_nbits = nbits;
    sp->lzw_free_ent = free_ent;
    sp->lzw_maxcode = maxcode;
    sp->enc_outp = bp;
    sp->enc_incount = incount;
    sp->enc_outcount = outcount;
    sp->enc_checkpoint = checkpoint;
    tif->tif_rawcc = (char *) bp - (char *) tif->tif_rawdata;
    *nencoded = wc0 - wc;
    return (bp >= bplimit);	/* buffer full? */
}

/*
 * Finish off an encoded strip by flushing the last
 * string and tacking on an End Of Information code.
 */
static int
PixarFilmPostEncode(TIFF *tif)
{
    LZWState *sp = (LZWState *)tif->tif_data;
    register u_char *bp;
    register long  outcount;
    register long  nextdata, nextbits, nbits;

    bp = sp->enc_outp;
    nextdata = sp->lzw_nextdata;
    nextbits = sp->lzw_nextbits;
    nbits = sp->lzw_nbits;
    outcount = sp->enc_outcount;

    if (sp->lzw_oldcode != -1)  {
	PutNextCode(sp->lzw_oldcode);
	sp->lzw_oldcode = -1;
    }

    PutNextCode(CODE_EOI);

    if (nextbits > 0)  {
	*bp++ = (u_char)(nextdata << (8-nextbits));
    }
    tif->tif_rawcc = (char *) bp - (char *) tif->tif_rawdata;
    return (1);
}
#endif /* REALLY_ENCODE */

static void
PixarFilmCleanup(TIFF *tif)
{
    LZWState      *sp = (LZWState *)tif->tif_data;

    if (tif->tif_data) {
	_TIFFfree((char *)sp->lzw_tbuf);
	_TIFFfree(tif->tif_data);
	tif->tif_data = NULL;
    }
}

/*
 * Copyright (c) 1985, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods, derived from original work by Spencer Thomas
 * and Joseph Orost.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

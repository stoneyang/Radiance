/* Copyright (c) 1999 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Radiance holodeck picture generator
 */

#include "rholo.h"
#include "view.h"
#include "resolu.h"

char	*progname;		/* our program name */
char	*hdkfile;		/* holodeck file name */

VIEW	myview = STDVIEW;	/* current output view */
int	xres = 512, yres = 512;	/* max. horizontal and vertical resolution */
char	*outspec = NULL;	/* output file specification */
double	pixaspect = 1.;		/* pixel aspect ratio */
int	seqstart = 0;		/* sequence start frame */
double	expval = 1.;		/* exposure value */

COLOR	*mypixel;		/* pixels being rendered */
float	*myweight;		/* weights (used to compute final pixels) */
int	hres, vres;		/* current horizontal and vertical res. */

extern int	nowarn;		/* turn warnings off? */


main(argc, argv)
int	argc;
char	*argv[];
{
	int	i, rval;

	progname = argv[0];			/* get arguments */
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		rval = getviewopt(&myview, argc-i, argv+i);
		if (rval >= 0) {		/* view option */
			i += rval;
			continue;
		}
		switch (argv[i][1]) {
		case 'w':			/* turn off warnings */
			nowarn++;
			break;
		case 'p':			/* pixel aspect/exposure */
			if (badarg(argc-i-1,argv+i+1,"f"))
				goto userr;
			if (argv[i][1] == 'a')
				pixaspect = atof(argv[++i]);
			else if (argv[i][1] == 'e') {
				expval = atof(argv[++i]);
				if (argv[i][0] == '-' | argv[i][0] == '+')
					expval = pow(2., expval);
			} else
				goto userr;
			break;
		case 'x':			/* horizontal resolution */
			if (badarg(argc-i-1,argv+i+1,"i"))
				goto userr;
			xres = atoi(argv[++i]);
			break;
		case 'y':			/* vertical resolution */
			if (badarg(argc-i-1,argv+i+1,"i"))
				goto userr;
			yres = atoi(argv[++i]);
			break;
		case 'o':			/* output file specificaiton */
			if (badarg(argc-i-1,argv+i+1,"s"))
				goto userr;
			outspec = argv[++i];
			break;
		case 'S':			/* sequence start */
			if (badarg(argc-i-1,argv+i+1,"i"))
				goto userr;
			seqstart = atoi(argv[++i]);
			break;
		case 'v':			/* view file */
			if (argv[i][1]!='f' || badarg(argc-i-1,argv+i+1,"s"))
				goto userr;
			rval = viewfile(argv[++i], &myview, NULL);
			if (rval < 0) {
				sprintf(errmsg, "cannot open view file \"%s\"",
						argv[i]);
				error(SYSTEM, errmsg);
			} else if (rval == 0) {
				sprintf(errmsg, "bad view file \"%s\"",
						argv[i]);
				error(USER, errmsg);
			}
			break;
		default:
			goto userr;
		}
	}
						/* open holodeck file */
	if (i >= argc)
		goto userr;
	hdkfile = argv[i++];
	initialize();
						/* render picture(s) */
	if (seqstart <= 0)
		dopicture(0);
	else
		while (nextview(&myview, stdin) != EOF)
			dopicture(seqstart++);
	quit(0);				/* all done! */
userr:
	fprintf(stderr,
"Usage: %s [-w][-pa pa][-pe ex][-x hr][-y vr][-S stfn][-o outp][view] input.hdk\n",
			progname);
	quit(1);
}


dopicture(fn)			/* render view from holodeck */
int	fn;
{
	char	*err;
	int	rval;
	BEAMLIST	blist;

	if ((err = setview(&myview)) != NULL) {
		sprintf(errmsg, "%s -- skipping frame %d", err, fn);
		error(WARNING, errmsg);
		return;
	}
	startpicture(fn);		/* open output picture */
					/* determine relevant beams */
	viewbeams(&myview, hres, vres, &blist);
					/* render image */
	if (blist.nb > 0) {
		render_frame(blist.bl, blist.nb);
		free((char *)blist.bl);
	} else {
		sprintf(errmsg, "no section visible in frame %d", fn);
		error(WARNING, errmsg);
	}
	rval = endpicture();		/* write pixel values */
	if (rval < 0) {
		sprintf(errmsg, "error writing frame %d", fn);
		error(SYSTEM, errmsg);
	}
	if (blist.nb > 0 & rval > 0) {
		sprintf(errmsg, "%.1f%% unrendered pixels in frame %d",
				100.*rval/(hres*vres), fn);
		error(WARNING, errmsg);
	}
}


render_frame(bl, nb)		/* render frame from beam values */
register PACKHEAD	*bl;
int	nb;
{
	extern int	render_beam();
	register HDBEAMI	*bil;
	register int	i;

	if (nb <= 0) return;
	if ((bil = (HDBEAMI *)malloc(nb*sizeof(HDBEAMI))) == NULL)
		error(SYSTEM, "out of memory in render_frame");
	for (i = nb; i--; ) {
		bil[i].h = hdlist[bl[i].hd];
		bil[i].b = bl[i].bi;
	}
	hdloadbeams(bil, nb, render_beam);
	free((char *)bil);
}


startpicture(fn)		/* initialize picture for rendering & output */
int	fn;
{
	extern char	VersionID[];
	double	pa = pixaspect;
	char	fname[256];
				/* compute picture resolution */
	hres = xres; vres = yres;
	normaspect(viewaspect(&myview), &pa, &hres, &vres);
				/* prepare output */
	if (outspec != NULL) {
		sprintf(fname, outspec, fn);
		if (freopen(fname, "w", stdout) == NULL) {
			sprintf(errmsg, "cannot open output \"%s\"", fname);
			error(SYSTEM, errmsg);
		}
	}
				/* write header */
	newheader("RADIANCE", stdout);
	printf("SOFTWARE= %s\n", VersionID);
	printf("%s %s\n", progname, hdkfile);
	if (fn)
		printf("FRAME=%d\n", fn);
	fputs(VIEWSTR, stdout);
	fprintview(&myview, stdout);
	fputc('\n', stdout);
	if (pa < 0.99 | pa > 1.01)
		fputaspect(pa, stdout);
	if (expval < 0.99 | expval > 1.01)
		fputexpos(expval, stdout);
	fputformat(COLRFMT, stdout);
	fputc('\n', stdout);
				/* write resolution (standard order) */
	fprtresolu(hres, vres, stdout);
				/* prepare image buffers */
	bzero((char *)mypixel, hres*vres*sizeof(COLOR));
	bzero((char *)myweight, hres*vres*sizeof(float));
}


int
endpicture()			/* finish and write out pixels */
{
	int	nunrend = 0;
	int	v;
	register double	d;
	register int	p;
				/* compute final pixel values */
	for (p = hres*vres; p--; ) {
		if (myweight[p] <= FTINY) {
			nunrend++;
			continue;
		}
		d = expval/myweight[p];
		scalecolor(mypixel[p], d);
	}
				/* write each scanline */
	for (v = vres; v--; )
		if (fwritescan(mypixel+v*hres, hres, stdout) < 0)
			return(-1);
	if (fflush(stdout) == EOF)
		return(-1);
	return(nunrend);
}


initialize()			/* initialize holodeck and buffers */
{
	extern long	ftell();
	int	fd;
	FILE	*fp;
	int	n;
	int4	nextloc;
					/* open holodeck file */
	if ((fp = fopen(hdkfile, "r")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\" for reading", hdkfile);
		error(SYSTEM, errmsg);
	}
					/* check header format */
	checkheader(fp, HOLOFMT, NULL);
					/* check magic number */
	if (getw(fp) != HOLOMAGIC) {
		sprintf(errmsg, "bad magic number in holodeck file \"%s\"",
				hdkfile);
		error(USER, errmsg);
	}
	nextloc = ftell(fp);			/* get stdio position */
	fd = dup(fileno(fp));			/* dup file descriptor */
	fclose(fp);				/* done with stdio */
	for (n = 0; nextloc > 0L; n++) {	/* initialize each section */
		lseek(fd, (long)nextloc, 0);
		read(fd, (char *)&nextloc, sizeof(nextloc));
		hdinit(fd, NULL);
	}
					/* allocate picture buffer */
	mypixel = (COLOR *)bmalloc(xres*yres*sizeof(COLOR));
	myweight = (float *)bmalloc(xres*yres*sizeof(float));
	if (mypixel == NULL | myweight == NULL)
		error(SYSTEM, "out of memory in initialize");
}


eputs(s)			/* put error message to stderr */
register char  *s;
{
	static int  midline = 0;

	if (!*s)
		return;
	if (!midline++) {	/* prepend line with program name */
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (s[strlen(s)-1] == '\n') {
		fflush(stderr);
		midline = 0;
	}
}
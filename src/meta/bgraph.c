#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 *  bgraph.c - program to send plots to metafile graphics programs.
 *
 *     6/25/86
 *
 *     Greg Ward Larson
 */

#include  <stdio.h>

#include  "meta.h"
#include  "mgvars.h"
#include  "mgraph.h"

#define  istyp(s)	(s[0] == '-')

#define  isvar(s)	(s[0] == '+')

char  *progname;

char  *libpath[4];

static void dofile(int  optc, char  *optv[], char  *file);


int
main(
	int  argc,
	char  *argv[]
)
{
	char  *getenv();
	int  i, file0;
	progname = argv[0];
	libpath[0] = "";
	if ((libpath[i=1] = getenv("MDIR")) != NULL)
		i++;
	libpath[i++] = MDIR;
	libpath[i] = NULL;

	for (file0 = 1; file0 < argc; )
		if (istyp(argv[file0]))
			file0++;
		else if (isvar(argv[file0]) && file0 < argc-1)
			file0 += 2;
		else
			break;

	if (file0 >= argc)
		dofile(argc-1, argv+1, NULL);
	else
		for (i = file0; i < argc; i++)
			dofile(file0-1, argv+1, argv[i]);

	quit(0);
	return 0; /* pro forma return */
}


static void
dofile(		/* plot a file */
	int  optc,
	char  *optv[],
	char  *file
)
{
	char  stmp[256];
	int  i;
						/* start fresh */
	mgclearall();
						/* type options first */
	for (i = 0; i < optc; i++)
		if (istyp(optv[i])) {
			sprintf(stmp, "include=%s.plt", optv[i]+1);
			setmgvar(progname, stdin, stmp);
		} else
			i++;
						/* file next */
	mgload(file);
						/* variable options last */
	for (i = 0; i < optc; i++)
		if (isvar(optv[i])) {
			sprintf(stmp, "%s=%s", optv[i]+1, optv[i+1]);
			setmgvar(progname, stdin, stmp);
			i++;
		}
						/* graph it */
	mgraph();
}


void
quit(code)				/* quit program */
int  code;
{
	mdone();
	exit(code);
}

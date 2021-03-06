#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Return file date (UNIX seconds as returned by time(2) call)
 *
 *  External symbols declared in rtio.h
 */

#include "copyright.h"

#include  "rtio.h"
#include  <sys/stat.h>
#if defined(_WIN32) || defined(_WIN64)
  #include  <sys/utime.h>
#else
  #include  <utime.h>
#endif


time_t
fdate(fname)				/* get file date */
char  *fname;
{
	struct stat  sbuf;

	if (stat(fname, &sbuf) == -1)
		return(0);

	return(sbuf.st_mtime);
}


int
setfdate(fname, ftim)			/* set file date */
char  *fname;
long  ftim;
{
	struct utimbuf utb;

	utb.actime = utb.modtime = ftim;
	return(utime(fname, &utb));

#ifdef NOTHING /* XXX does this work anywhere? */
	time_t  ftm[2];

	ftm[0] = ftm[1] = ftim;
	return(utime(fname, ftm));
#endif
}

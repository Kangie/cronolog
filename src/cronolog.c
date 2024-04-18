/* ====================================================================
 * Copyright (c) 1995-1999 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */
/*
 * cronolog -- simple Apache log rotation program
 *
 * Copyright (c) 1996-1999 by Ford & Mason Ltd
 *
 * This software was submitted by Ford & Mason Ltd to the Apache
 * Software Foundation in December 1999.  Future revisions and
 * derivatives of this source code must acknowledge Ford & Mason Ltd
 * as the original contributor of this module.  All other licensing
 * and usage conditions are those of the Apache Software Foundation.
 *
 * Originally written by Andrew Ford <A.Ford@ford-mason.co.uk>
 *
 * cronolog is loosly based on the rotatelogs program, which is part of the
 * Apache package written by Ben Laurie <ben@algroup.co.uk>
 *
 * The argument to this program is the log file name template as an
 * strftime format string.  For example to generate new error and
 * access logs each day stored in subdirectories by year, month and day add
 * the following lines to the httpd.conf:
 *
 *	TransferLog "|/www/etc/cronolog /www/logs/%Y/%m/%d/access.log"
 *	ErrorLog    "|/www/etc/cronolog /www/logs/%Y/%m/%d/error.log"
 *
 * The option "-x file" specifies that debugging messages should be
 * written to "file" (e.g. /dev/console) or to stderr if "file" is "-".
 */

#include "config.h"
#include "cronoutils.h"
#include "getopt.h"

#include <signal.h>
#include <time.h>

/* Forward function declaration */

int	new_log_file(const char *, const char *, mode_t, const char *,
		     PERIODICITY, int, int, char *, size_t, time_t, time_t *);

void  terminate_self(int);
/* Definition of version and usage messages */

#ifndef _WIN32
#define VERSION_MSG   	PACKAGE " version " VERSION "\n"
#else
#define VERSION_MSG      "cronolog version 0.1\n"
#endif

#ifndef _WIN32
#define SETUGID_USAGE	"   -u USER,   --set-uid=USER  change to USER before doing anything (name or UID)\n" \
			"   -g GROUP,  --set-gid=GROUP change to GROUP before doing anything (name or GID)\n" \
			"   -U OCTAL,  --umask=OCTAL   sets umask of file/directory creation\n"
#else
#define SETUGID_USAGE	""
#endif

#define USAGE_MSG 	"usage: %s [OPTIONS] logfile-spec\n" \
			"\n" \
			"   -H NAME,   --hardlink=NAME maintain a hard link from NAME to current log\n" \
			"   -S NAME,   --symlink=NAME  maintain a symbolic link from NAME to current log\n" \
			"   -P NAME,   --prev-symlink=NAME  maintain a symbolic link from NAME to previous log\n" \
			"   -l NAME,   --link=NAME     same as -S/--symlink\n" \
			"   -h,        --help          print this help, then exit\n" \
			"   -p PERIOD, --period=PERIOD set the rotation period explicitly\n" \
			"   -d DELAY,  --delay=DELAY   set the rotation period delay\n" \
			"   -o,        --once-only     create single output log from template (not rotated)\n" \
			"   -x FILE,   --debug=FILE    write debug messages to FILE\n" \
			"                              ( or to standard error if FILE is \"-\")\n" \
			"   -a,        --american         American date formats\n" \
			"   -e,        --european      European date formats (default)\n" \
			"   -s TIME,   --start-time=TIME   starting time\n" \
			"   -z TZ,     --time-zone=TZ  use TZ for timezone\n" \
			SETUGID_USAGE \
			"   -V,        --version       print version number, then exit\n"


/* Definition of the short and long program options */

#ifndef _WIN32
char          *short_options = "ad:eop:s:z:H:P:S:l:hVx:u:g:U:";
#else
char          *short_options = "ad:eop:s:z:H:P:S:l:hVx:";
#endif

#ifndef _WIN32
struct option long_options[] =
{
    { "american",	no_argument,		NULL, 'a' },
    { "european",	no_argument,		NULL, 'e' },
    { "start-time", 	required_argument,	NULL, 's' },
    { "time-zone",  	required_argument,	NULL, 'z' },
    { "hardlink",  	required_argument, 	NULL, 'H' },
    { "symlink",   	required_argument, 	NULL, 'S' },
    { "prev-symlink",  	required_argument, 	NULL, 'P' },
    { "link",      	required_argument, 	NULL, 'l' },
    { "period",		required_argument,	NULL, 'p' },
    { "delay",		required_argument,	NULL, 'd' },
    { "set-uid",      	required_argument,     	NULL, 'u' },
    { "set-gid",      	required_argument,     	NULL, 'g' },
    { "once-only", 	no_argument,       	NULL, 'o' },
    { "help",      	no_argument,       	NULL, 'h' },
    { "umask",     	required_argument, 	NULL, 'U' },
    { "version",   	no_argument,       	NULL, 'V' },
    { NULL,		0,			NULL, 0 }
};
#endif

#ifndef _WIN32
static mode_t saved_umask = 0;
static mode_t new_umask = 0;
#endif

/* Main function.
 */
int
main(int argc, char **argv)
{
    PERIODICITY	periodicity = UNKNOWN;
    PERIODICITY	period_delay_units = UNKNOWN;
    int		period_multiple = 1;
    int		period_delay  = 0;
    int		use_american_date_formats = 0;
    char 	read_buf[BUFSIZE];
    char 	tzbuf[BUFSIZE];
    char	filename[MAX_PATH];
    char	*start_time = NULL;
    char	*template;
    char	*linkname = NULL;
    char	*prevlinkname = NULL;
#ifndef _WIN32
    uid_t	new_uid = 0;
    gid_t	new_gid = 0;
    int		change_uid = 0;
    int		change_gid = 0;
#endif
    mode_t	linktype = 0;
    int 	n_bytes_read;
    int		ch;
    time_t	time_now;
    time_t	time_offset = 0;
    time_t	next_period = 0;
    int 	log_fd = -1;

#ifndef _WIN32
    new_umask=umask(037); // default to 640
    while ((ch = getopt_long(argc, argv, short_options, long_options, NULL)) != EOF)
	umask(new_umask); // Set umask to our default or whatever the user provided
#else
    while ((ch = getopt(argc, argv, short_options)) != EOF)
#endif        
    {
	switch (ch)
	{
	case 'a':
	    use_american_date_formats = 1;
	    break;
	    
	case 'e':
	    use_american_date_formats = 0;
	    break;
	    
	case 's':
	    start_time = optarg;
	    break;

	case 'z':
	    sprintf(tzbuf, "TZ=%s", optarg);
	    putenv(tzbuf);
	    break;

	case 'H':
	    linkname = optarg;
	    linktype = S_IFREG;
	    break;

	case 'l':
	case 'S':
	    linkname = optarg;
#ifndef _WIN32
	    linktype = S_IFLNK;
#endif        
	    break;
	    
	case 'P':
	    if (linkname == NULL)
	    {
		fprintf(stderr, "A current log symlink is needed to mantain a symlink to the previous log\n", argv[0]);
		exit(1);
	    }
	    prevlinkname = optarg;
	    break;
	    

	case 'd':
	    period_delay_units = parse_timespec(optarg, &period_delay);
	    break;

	case 'p':
	    periodicity = parse_timespec(optarg, &period_multiple);
	    if (   (periodicity == INVALID_PERIOD)
		|| (periodicity == PER_SECOND) && (60 % period_multiple)
		|| (periodicity == PER_MINUTE) && (60 % period_multiple)
		|| (periodicity == HOURLY)     && (24 % period_multiple)
		|| (periodicity == DAILY)      && (period_multiple > 365)
		|| (periodicity == WEEKLY)     && (period_multiple > 52)
		|| (periodicity == MONTHLY)    && (12 % period_multiple)) {
		fprintf(stderr, "%s: invalid explicit period specification (%s)\n", argv[0], start_time);
		exit(1);
	    }		
	    break;
	    
#ifndef _WIN32
	case 'u':
	    new_uid = parse_uid(optarg, argv[0]);
	    change_uid = 1;
	    break;
	case 'g':
	    new_gid = parse_gid(optarg, argv[0]);
	    change_gid = 1;
	    break;
#endif
	case 'o':
	    periodicity = ONCE_ONLY;
	    break;
	    
	case 'x':
	    if (strcmp(optarg, "-") == 0)
	    {
		debug_file = stderr;
	    }
	    else
	    {
		debug_file = fopen(optarg, "a+");
	    }
	    break;
	    
	case 'V':
	    fprintf(stderr, VERSION_MSG);
	    exit(0);
	    
	case 'h':
	case '?':
	    fprintf(stderr, USAGE_MSG, argv[0]);
	    exit(1);
	}
    }

    if ((argc - optind) != 1)
    {
	fprintf(stderr, USAGE_MSG, argv[0]);
	exit(1);
    }

#ifndef _WIN32
    if (change_gid && setgid(new_gid) == -1) {
	fprintf(stderr, "setgid: unable to change to gid: %d\n", new_gid);
       	exit(1);
    }
    if (change_uid && setuid(new_uid) == -1) {
	fprintf(stderr, "setuid: unable to change to uid: %d\n", new_uid);
       	exit(1);
    }
#endif

    DEBUG((VERSION_MSG "\n"));

    if (start_time)
    {
	time_now = parse_time(start_time, use_american_date_formats);
	if (time_now == -1)
	{
	    fprintf(stderr, "%s: invalid start time (%s)\n", argv[0], start_time);
	    exit(1);
	}
	time_offset = time_now - time(NULL);
	DEBUG(("Using offset of %d seconds from real time\n", time_offset));
    }

    /* The template should be the only argument.
     * Unless the -o option was specified, determine the periodicity.
     */
    
    template = argv[optind];
    if (periodicity == UNKNOWN)
    {
	periodicity = determine_periodicity(template);
    }


    DEBUG(("periodicity = %d %s\n", period_multiple, periods[periodicity]));

    if (period_delay) {
	if (   (period_delay_units > periodicity)
	    || (   period_delay_units == periodicity
		&& abs(period_delay)  >= period_multiple)) {
	    fprintf(stderr, "%s: period delay cannot be larger than the rollover period\n", argv[0], start_time);
	    exit(1);
	}		
	period_delay *= period_seconds[period_delay_units];
    }

    DEBUG(("Rotation period is per %d %s\n", period_multiple, periods[periodicity]));


    /* set up signal handlers to catch USR1 and HUP when restarting Apache */
    signal(SIGUSR1, terminate_self);
    signal(SIGHUP, terminate_self);

    /* Loop, waiting for data on standard input */

    for (;;)
    {
	/* Read a buffer's worth of log file data, exiting on errors
	 * or end of file.
	 */
	n_bytes_read = read(0, read_buf, sizeof read_buf);
	if (n_bytes_read == 0)
	{
	    exit(3);
	}
	if (errno == EINTR)
	{
	    continue;
	}
	else if (n_bytes_read < 0)
	{
	    exit(4);
	}

	time_now = time(NULL) + time_offset;
	
	/* If the current period has finished and there is a log file
	 * open, close the log file
	 */
	if ((time_now >= next_period) && (log_fd >= 0))
	{
	    close(log_fd);
	    log_fd = -1;
	}
	
	/* If there is no log file open then open a new one.
	 */
	if (log_fd < 0)
	{
	    log_fd = new_log_file(template, linkname, linktype, prevlinkname,
				  periodicity, period_multiple, period_delay,
				  filename, sizeof (filename), time_now, &next_period);
	}

	DEBUG(("%s (%d): wrote message; next period starts at %s (%d) in %d secs\n",
	       timestamp(time_now), time_now, 
	       timestamp(next_period), next_period,
	       next_period - time_now));

	/* Write out the log data to the current log file.
	 */
	if (write(log_fd, read_buf, n_bytes_read) != n_bytes_read)
	{
	    perror(filename);
	    exit(5);
	}
    }

    /* NOTREACHED */
    return 1;
}

void terminate_self(int sig)
{
    exit(6);
}

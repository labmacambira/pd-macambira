/* Copyright (c) 1999 Guenter Geiger and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*
 * This file implements the loader for linux, which includes
 * a little bit of path handling.
 *
 * Generalized by MSP to provide an open_via_path function
 * and lists of files for all purposes.
 */ 

/* #define DEBUG(x) x */
#define DEBUG(x)
void readsf_banana( void);    /* debugging */

#include <stdlib.h>
#ifdef UNIX
#include <unistd.h>
#include <sys/stat.h>
#endif
#ifdef NT
#include <io.h>
#endif

#include <string.h>
#include "m_imp.h"
#include <stdio.h>
#include <fcntl.h>

static t_namelist *pd_path;

/* Utility functions */

/* copy until delimiter and return position after delimiter in string */
/* if it was the last substring, return NULL */

static const char* strtokcpy(char *to, const char *from, int delim)
{
    int size = 0;

    while (from[size] != (char)delim && from[size] != '\0')
    	size++;

    strncpy(to,from,size);
    to[size] = '\0';
    if (from[size] == '\0') return NULL;
    if (size) return from+size+1;
    else return NULL;
}

/* add a colon-separated list of names to a namelist */

#ifdef NT
#define SEPARATOR ';'
#else
#define SEPARATOR ':'
#endif

static t_namelist *namelist_doappend(t_namelist *listwas, const char *s)
{
    t_namelist *nl = listwas, *rtn = listwas, *nl2;
    nl2 = (t_namelist *)(getbytes(sizeof(*nl)));
    nl2->nl_next = 0;
    nl2->nl_string = (char *)getbytes(strlen(s) + 1);
    strcpy(nl2->nl_string, s);
    sys_unbashfilename(nl2->nl_string, nl2->nl_string);
    if (!nl)
        nl = rtn = nl2;
    else
    {
        while (nl->nl_next)
    	    nl = nl->nl_next;
        nl->nl_next = nl2;
    }
    return (rtn);

}

t_namelist *namelist_append(t_namelist *listwas, const char *s)
{
    const char *npos;
    char temp[MAXPDSTRING];
    t_namelist *nl = listwas, *rtn = listwas;
    
    npos = s;
    do
    {
	npos = strtokcpy(temp, npos, SEPARATOR);
	if (! *temp) continue;
	nl = namelist_doappend(nl, temp);
    }
	while (npos);
    return (nl);
}

void namelist_free(t_namelist *listwas)
{
    t_namelist *nl, *nl2;
    for (nl = listwas; nl; nl = nl2)
    {
    	nl2 = nl->nl_next;
	t_freebytes(nl->nl_string, strlen(nl->nl_string) + 1);
	t_freebytes(nl, sizeof(*nl));
    }
}

void sys_addpath(const char *p)
{
     pd_path = namelist_append(pd_path, p);
}

#ifdef NT
#define NTOPENFLAG (bin ? _O_BINARY : _O_TEXT)
#else
#define NTOPENFLAG 0
#endif

/* search for a file in a specified directory, then along the globally
defined search path, using ext as filename extension.  Exception:
if the 'name' starts with a slash or a letter, colon, and slash in NT,
there is no search and instead we just try to open the file literally.  The
fd is returned, the directory ends up in the "dirresult" which must be at
least "size" bytes.  "nameresult" is set to point to the filename, which
ends up in the same buffer as dirresult. */

int open_via_path(const char *dir, const char *name, const char* ext,
    char *dirresult, char **nameresult, unsigned int size, int bin)
{
    t_namelist *nl, thislist;
    int fd = -1;
    char listbuf[MAXPDSTRING];

    if (name[0] == '/' 
#ifdef NT
    	|| (name[1] == ':' && name[2] == '/')
#endif
    	    )
    {
    	thislist.nl_next = 0;
    	thislist.nl_string = listbuf;
    	listbuf[0] = 0;
    }
    else
    {
    	thislist.nl_string = listbuf;
	thislist.nl_next = pd_path;
	strncpy(listbuf, dir, MAXPDSTRING);
	listbuf[MAXPDSTRING-1] = 0;
	sys_unbashfilename(listbuf, listbuf);
    }
    for (nl = &thislist; nl; nl = nl->nl_next)
    {
    	if (strlen(nl->nl_string) + strlen(name) + strlen(ext) + 4 >
	    size)
	    	continue;
	strcpy(dirresult, nl->nl_string);
	if (*dirresult && dirresult[strlen(dirresult)-1] != '/')
	       strcat(dirresult, "/");
	strcat(dirresult, name);
	strcat(dirresult, ext);
	sys_bashfilename(dirresult, dirresult);

	DEBUG(post("looking for %s",dirresult));
	    /* see if we can open the file for reading */
	if ((fd=open(dirresult,O_RDONLY | NTOPENFLAG)) >= 0)
	{
	    	/* in UNIX, further check that it's not a directory */
#ifdef UNIX
    	    struct stat statbuf;
	    int ok =  ((fstat(fd, &statbuf) >= 0) &&
	    	!S_ISDIR(statbuf.st_mode));
	    if (!ok)
	    {
	    	if (sys_verbose) post("tried %s; stat failed or directory",
		    dirresult);
	    	close (fd);
		fd = -1;
    	    }
	    else
#endif
    	    {
	    	char *slash;
		if (sys_verbose) post("tried %s and succeeded", dirresult);
		sys_unbashfilename(dirresult, dirresult);
		slash = strrchr(dirresult, '/');
		if (slash)
		{
		    *slash = 0;
		    *nameresult = slash + 1;
		}
		else *nameresult = dirresult;
		
	    	return (fd);  
	    }
	}
	else
	{
	    if (sys_verbose) post("tried %s and failed", dirresult);
	}
    }
    *dirresult = 0;
    *nameresult = dirresult;
    return (-1);
}

/* Startup file reading for linux */

#ifdef __linux__

#define STARTUPNAME ".pdrc"
#define NUMARGS 1000

int sys_argparse(int argc, char **argv);

int sys_rcfile(void)
{
    FILE* file;
    int i;
    int k;
    int rcargc;
    char* rcargv[NUMARGS];
    char* buffer;
    char  fname[MAXPDSTRING], buf[1000];

    /* parse a startup file */
    
    *fname = '\0'; 

    strncat(fname, getenv("HOME"), MAXPDSTRING-10);
    strcat(fname, "/");

    strcat(fname, STARTUPNAME);

    if (!(file = fopen(fname, "r")))
    	return 1;

    post("reading startup file: %s", fname);

    rcargv[0] = ".";	/* this no longer matters to sys_argparse() */

    for (i = 1; i < NUMARGS-1; i++)
    {
    	if (fscanf(file, "%999s", buf) < 0)
	    break;
	buf[1000] = 0;
	if (!(rcargv[i] = malloc(strlen(buf) + 1)))
	    return (1);
	strcpy(rcargv[i], buf);
    }
    if (i >= NUMARGS-1)
    	fprintf(stderr, "startup file too long; extra args dropped\n");
    rcargv[i] = 0;

    rcargc = i;

    /* parse the options */

    fclose(file);
    if (sys_verbose)
    {
    	if (rcargv)
	{
    	    post("startup args from RC file:");
	    for (i = 1; i < rcargc; i++)
	    	post("%s", rcargv[i]);
    	}
	else post("no RC file arguments found");
    }
    if (sys_argparse(rcargc, rcargv))
    {
    	post("error parsing RC arguments");
	return (1);
    }
    return (0);
}
#endif /* __linux__ */



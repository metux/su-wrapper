/*

    SU wrapper main file.
    
    Copyright (c) Enrico Weigelt <weigelt@nibiru.thur.de> (nekrad)
		  Werner Fink    <werner@suse.de>
    
    This code is released under the terms of the 
    GNU Public License.

*/

/*	
    2002-06-29	moved out static names to build-time 
                configurable defines
							--nekrad
    2001-10-25	added special handling for shell commands 
		(these are written in "")	
							--nekrad
    2001-10-26	fixed parameter matching		--nekrad
*/

//#define DEBUG		/* show debugging output */
//#define DUMMY		/* test only - no su */

#define CF_VERSION	"1.2.3"

/* shell for executing stuff in "" */
#define CF_EXEC_SHELL	"/bin/sh"

/* config file where to get the table from */
#define CF_CONFIGFILE	"/etc/su-wrapper.conf"

/* my own application name for logging */
#define CF_LOG_NAME	"su-wrapper"

/* fallback user+group if uid lookup fails */
#define CF_FALLBACK_USER	"nobody"
#define CF_FALLBACK_GROUP	"nogroup"

#define CF_FALLBACK_PWD		"/tmp"
#define CF_FALLBACK_SHELL	"/bin/false"

#define CF_PATH_PRIVILEGED	"/sbin:/usr/sbin:/bin:/usr/bin:/usr/bin/X11"
#define CF_PATH_UNPRIVILEGED	"/usr/local/bin:/bin:/usr/bin:/usr/bin/X11"

#define CF_SUPER_USER		"root"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <limits.h>
#include <sys/types.h>
extern char **environ;

typedef const char * CHR;
static int verbose = 0;

static uid_t euid, ruid;
static gid_t egid, rgid;

#ifdef DEBUG
# define dprintf(fmt, arg...)  fprintf(stderr, fmt, ## arg)
#else
# define dprintf(fmt, arg...)
#endif

/*
 * Internal logger, we may use vsyslog() instead vfprintf() here.
 */
static char *myname = CF_LOG_NAME;
static void _logger (const char *fmt, va_list ap)
{
	char buf[strlen(myname)+2+strlen(fmt)+1];
	strcat(strcat(strcpy(buf, myname), ": "), fmt);
	vfprintf(stderr, buf, ap);
	return;
}

static inline char* my_basename ( const char* name )
{
	char * ret = "";

	if (name)
		ret = basename(name);
	return ret;
}


/*
 * Cry and exit.
 */
static void error (const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	_logger(fmt, ap);
	va_end(ap);
	exit (1);
}

/*
 * Warn the user.
 */
static void warn (const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	_logger(fmt, ap);
	va_end(ap);
	return;
}

struct entry_struct 
{
        char * called_user;
        char * called_group;
        char * called_command;
        char * called_cmdline;
        char * run_user;
        char * run_group;
        char * run_command;
        char * params[_POSIX_ARG_MAX + 1];
};
typedef struct entry_struct entry_t;


static entry_t entry;

static int wildcard_match ( CHR mask, CHR value )
{
	int ret = 0;

	if (strlen(mask) == 1) 
	{
		if (*mask == '-') ret = 1;	/* ignore -> match any */
		if (*mask == '*') ret = 1;	/* match any */
	} else 
	{				/* real wildcards not yet implemented, see regex(3) */
		if (!strcmp(mask, value))	/* direct match */
			ret = 1;
	}
	return ret;
}

static int cmdline_match (char * const mask, char * const argv[])
{
	int ret = 0;
	char* m_arg[_POSIX_ARG_MAX + 1]; /* HERE */
	int walk;
    
	if (strlen(mask) == 1) 
	{
		if (*mask == '-') 		/* match none */
			if (!*argv) ret = 1;
		if (*mask == '*')   ret = 1;	/* match any */
	} 					/* real wildcards not yet implemented, see regex(3) */

/* GRUETZE!

    verschiedene options kommen dann mit regex ...
    parameter-uebergabe kommt dann spaeter mit variablen ($0 $1 usw)
    
//		char *l = strdupa(mask);
//		char *oslot;
//		size_t max = _POSIX_ARG_MAX - 1;	//no real argv[0] 
//		int n, o[max];			// o is our option vector for found options
//
//		ret = 1;
//		if (!*argv)
//			goto out;
//
//		memset(o, 0, sizeof(o));
//		while ((oslot = strsep(&l, ",:"))) {
//			if (!*oslot)		// empty slot means no slot 
//				continue;
//			for (n = 0; n < max && argv[n]; n++) {
//				if (!strcmp(oslot, argv[n]))
//					o[n]++;
//			}
//		}
//
//						// -lf is not -fl and not -l -f or -f -l 
//		ret = 1;
//		for (n = 0; n < max && argv[n]; n++)
//			if (!o[n])
//				ret = 0;
//	}
//out:
//	return ret;
*/
    /* FIXME!!! derzeit nur ein parameter supported */
    /* sollte irgentwie aufgesplittet werden ... */
    m_arg[0] = mask;
    m_arg[1] = NULL;

    walk=0;
    while ((m_arg[walk]) && (argv[walk])) 
    {
	if (strcmp(m_arg[walk],"*")==0) 	goto matched;
	if (strcmp(m_arg[walk],argv[walk])) 	goto nomatch;
	walk++;
    }
    if ((m_arg[walk]) && (!(strcmp(m_arg[walk],"*")))) 	goto matched;
    if ((m_arg[walk])||(argv[walk])) 			goto nomatch;    

matched:
    dprintf("MATCH\n");
    return 1;
        
nomatch:
    dprintf("NOMATCH\n");    
    return 0;
}
			
static inline char * get_current_user ()
{
	struct passwd * pw = getpwuid(getuid());
	char * ret = CF_FALLBACK_USER;

	if (pw)
		ret = pw->pw_name;
	else
		warn("could not determine current user name ... defaulting to \"" CF_FALLBACK_USER "\"\n");

	return ret;
}

static inline char * get_current_group ()
{
	struct group * gr = getgrgid(getgid());
	char * ret = CF_FALLBACK_GROUP;

	if (gr)
		ret = gr->gr_name;
	else
		warn("could not determine current group name ... defaulting to \"" CF_FALLBACK_GROUP "\"\n");

	return ret;
}

static inline uid_t lookup_uid ( CHR name )
{
	struct passwd * pw = getpwnam(name);
	uid_t uid = -1;

	if (pw)
		uid = pw->pw_uid;
	else
		warn("could not determine current user id ... defaulting to \"" CF_FALLBACK_USER "\"\n");

	return uid;
}

static inline gid_t lookup_gid ( CHR name )
{
	struct group * gr = getgrnam(name);
	gid_t gid = -1;

	if (gr)
		gid = gr->gr_gid;
	else
		warn("could not determine current group id ... defaulting to \"" CF_FALLBACK_GROUP "\"\n");

	return gid;
}

static inline char * lookup_pwd ( CHR name )
{
	struct passwd * pw = getpwnam(name);
	char * pwd = CF_FALLBACK_PWD;

	if (pw)
		pwd = pw->pw_dir;
	else
		warn("could not determine current home ... defaulting to \"" CF_FALLBACK_PWD "\"\n");

	return pwd;
}

static inline char * lookup_shell ( CHR name )
{
	struct passwd * pw = getpwnam(name);
	char * shell = CF_FALLBACK_SHELL;

	if (pw)
		shell = pw->pw_shell;
	else
		warn("could not determine current shell ... defaulting to \"" CF_FALLBACK_SHELL "\"\n");

	return shell;
}

static int xputenv(char * const name, char * const value)
{
	char *buf = (char*) malloc(strlen(name) + 1 + strlen(value) + 1);

	if (!buf)
		error("malloc() failed: %s\n", strerror(errno));

	strcpy(buf, name);
	strcat(buf, "=");
	strcat(buf, value);

	return putenv(buf);
}

static void do_su (entry_t * entry)
{
	uid_t new_uid;
	gid_t new_gid;
	char * new_pwd;
	char * new_shell;

	if ((new_uid = lookup_uid(entry->run_user)) < 0)
		error("no such user \"%s\"\n", entry->run_user);

	if ((new_gid = lookup_gid(entry->run_group)) < 0)
		error("no such group \"%s\"\n", entry->run_group);

	if ((new_pwd = lookup_pwd(entry->run_user)) < 0)
		error("no such user \"%s\"\n", entry->run_user);

	if ((new_shell = lookup_shell(entry->run_user)) < 0)
		error("no such user \"%s\"\n", entry->run_user);

	if (verbose)
		fprintf(stderr, "Calling command : cmdline=\"%s\" user=\"%s\"(%d:%d) \n",
			entry->run_command, entry->run_user, new_uid, new_gid );

	/*
	 * Environment
	 */
	if (xputenv("USER", entry->run_user))
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("LOGNAME", entry->run_user))
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("PWD", new_pwd))
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("GROUP",   entry->run_group))
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("HOME", new_pwd))
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("SHELL", new_shell))
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("FOO", "foo123_knolle"))
		error("putenv() failed: %s\n", strerror(errno));
	
	/*
	 * Get real root back
	 */
	if (seteuid(euid) < 0) error("seteuid() failed: %s\n", strerror(errno));
	if (setegid(egid) < 0) error("setegid() failed: %s\n", strerror(errno));

#ifdef DUMMY	
	dprintf ( "dummy\n" );
#else
	if (setregid(new_gid, new_gid))
		error("setregid() failed: %s\n", strerror(errno));

	if (initgroups(entry->run_user, new_gid))
		error("initgroups() failed: %s\n", strerror(errno));

	if (setreuid(new_uid, new_uid))
		error("setreuid() failed: %s\n", strerror(errno));

	if (chdir(new_pwd))
		error("chdir() failed: %s\n", strerror(errno));
#endif
	execve(entry->run_command, entry->params, environ);
	error("execve() failed: %s\n", strerror(errno));
}

static char line[LINE_MAX];
static entry_t* check_against_table (CHR filename, char *argv[])
{
	FILE *table;
	char *l = line;
	char *cslot;
	int   nslot, argc = 1, found = 0, at = 0;
	char *user  = get_current_user();
	char *group = get_current_group();
	entry_t *e = &entry;
	char *  cmdname = my_basename(argv[0]);
	char ** cmdline = argv+1;

	dprintf ("cmdname=\"%s\" cmdline=\"%s\"\n", cmdname, *cmdline );
	
	if (!(table = fopen (filename, "r")))
		error("Could not open %s: %s\n", filename, strerror(errno));

	while (fgets(line, LINE_MAX, table)) 
	{
		at++;

		if (*(l = (char*)line) == '#') continue;
		if (*l == '\n') continue;

		memset((void*)e, 0, sizeof(e));
		nslot = 0;
		argc = 1;
		dprintf("Table check at line %d\n", at);
		while ((cslot = strsep(&l, " \t\n"))) {
			if (!*cslot)		/* empty slot means no slot */
				continue;
			switch (++nslot) {
			case 1:
				e->called_user = cslot;
				dprintf("called_user=%s\n", cslot);
				break;
			case 2:
				e->called_group = cslot;
				dprintf("called_group=%s\n", cslot);
				break;
			case 3:
				dprintf("called_command=%s\n", cslot);
				e->called_command = cslot;
				break;
			case 4:
				dprintf("called_cmdline=%s\n", cslot);
				e->called_cmdline = cslot;
				break;
			case 5:
				dprintf("run_user=%s\n", cslot);
				e->run_user = cslot;
				break;
			case 6:
				dprintf("run_group=%s\n", cslot);
				e->run_group = cslot;
		
				/* special handling for string with "" -- goes to shell */
				if (*l == '"') 
				{
				    int i;
				    l++;
				    
				    i = strlen(l);
				    while (i>0) 
				    {
					if (l[i]=='"') 
					{
					    l[i] = 0;
					    goto __x;
					}
					i--;
				    }
				    	    
				__x:				    
				    dprintf ( "run_shellcmd=\"%s\"\n", l );
				    e->run_command = CF_EXEC_SHELL;
				    e->params[argc++] = "-c";
				    e->params[argc++] = l;
				    goto do_check;
				}	    
				break;
			case 7:
				dprintf("run_cslot=%s\n", cslot);
				e->run_command = cslot;
				break;	
			default:
				dprintf("run_params[%d]=%s\n", argc, cslot);
				if (argc > _POSIX_ARG_MAX)
					break;
				e->params[argc++] = cslot;
				
				break;
			}
		}
		if (nslot < 7)			/* One field is empty */
			goto err;

do_check:

		if (!wildcard_match(e->called_command, cmdname))
			continue;
		if (!wildcard_match(e->called_user,    user))
			continue;
		if (!wildcard_match(e->called_group,   group))
			continue;
		if (!cmdline_match(e->called_cmdline,  cmdline))
			continue;
		
		if (!strcmp(e->run_command, "-"))
			error("explicitly forbidden\n");

		found++;
		break;
err:
		warn("wrong entry in " CF_CONFIGFILE " at line %d\n", at);
	}

	if (!found)
		e = NULL;
	else {
		e->params[0] = e->called_command; /* first argument */
		if (*(argv+1)) 
		{
			size_t max = _POSIX_ARG_MAX - argc;
			int n;

			for (n = 0; n < max && cmdline[n] ; n++)
				e->params[argc++] = cmdline[n];
		}
		e->params[argc] = NULL;		  /* last argument */
	}

	fclose(table);
	return e;
}

int main (int argc, char * argv[])
{
	entry_t *entry = NULL;
	char * term;

	egid = getegid(); rgid = getgid();
	if (setegid(rgid) < 0) error("setegid() failed: %s\n", strerror(errno));
	euid = geteuid(); ruid = getuid();
	if (seteuid(ruid) < 0) error("seteuid() failed: %s\n", strerror(errno));
	
	if (!argv[0])
		error("permission denied (no argv0)\n");
    
	dprintf("checking against table \n");
	
	entry = check_against_table(CF_CONFIGFILE, argv);
	if (!entry)
		error("permission denied\n");

#ifdef DEBUG
	verbose++;
#else
	if (getenv("SUWRAP_DEBUG"))
		verbose++;
#endif

	term = getenv("TERM");

#if 0
	/*
	 * Clear sensitive environment variables.
	 * Overwriting of personal environment variables will be done by do_su.
	 */
	unsetenv("LD_LIBRARY_PATH");	/* runtime linker */
	unsetenv("LD_PRELOAD");
	unsetenv("LD_DEBUG");
	unsetenv("LD_DEBUG_OUTPUT");
	unsetenv("LD_AOUT_LIBRARY_PATH");
	unsetenv("LD_AOUT_PRELOAD");
	unsetenv("BASH_ENV");		/* bash source file */
	unsetenv("ENV");		/* sh source file */
	unsetenv("DEFAULT_ENV");	/* ksh source file */
	unsetenv("SHINIT");		/* ash source file */
	unsetenv("MAIL");		/* mail default folder */
	unsetenv("MALLOC_CHECK_");	/* malloc(3) */
	/* something forgotten? */
#else
	/*
	 * Overwrite environment with a new and valid pointer of strings.
	 * Overwriting of personal environment variables will be done by do_su.
	 */
	environ = (char **) malloc (2 * sizeof (char *));
	if (!environ)
		error("malloc() failed: %s\n", strerror(errno));
	memset(environ, 0, 2 * sizeof (char *));
#endif

	/*
	 * overwrite PATH environment variable.
	 */
	if (!strcmp(entry->run_user, CF_SUPER_USER))
		xputenv("PATH", CF_PATH_PRIVILEGED);
	else
		xputenv("PATH", CF_PATH_UNPRIVILEGED);
	if (errno)
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("TERM", term))
		error("putenv() failed: %s\n", strerror(errno));

	do_su (entry);

	return 0; /* not reached */
}

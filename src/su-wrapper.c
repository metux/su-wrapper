/*

    SU wrapper main file.
    
    copyrigh (c) enrico weigelt <weigelt@nibiru.thur.de>
		 Werner Fink <werner@suse.de>
    
    this code is released under the terms of the 
    GNU Publics License.

*/

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

#define CONFIGFILE	"/etc/su-wrapper.conf"
typedef const char * CHR;
static int verbose = 0;

static uid_t euid, ruid;
static gid_t egid, rgid;

/*
 * Internal logger, we may use vsyslog() instead vfprintf() here.
 */
static char *myname = "su-wrapper";
static void _logger (const char *fmt, va_list ap)
{
	char buf[strlen(myname)+2+strlen(fmt)+1];
	strcat(strcat(strcpy(buf, myname), ": "), fmt);
	vfprintf(stderr, buf, ap);
	return;
}

static inline const char* my_basename ( const char* name )
{
	if (name) return basename(name);
	return "";
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

struct entry_struct {
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

	if (strlen(mask) == 1) {
		if (*mask == '-') ret = 1;	/* ignore -> match any */
		if (*mask == '*') ret = 1;	/* match any */
	} else {				/* real wildcards not yet implemented, see regex(3) */
		if (!strcmp(mask, value))	/* direct match */
			ret = 1;
	}
	return ret;
}

static int cmdline_match (char * const mask, char * const argv[])
{
	int ret = 0;
    
	if (!mask) { printf("FAULT!\n"); return 0; }

	if (strlen(mask) == 1) {
		if (*mask == '-') 		/* match none */
			if (!*argv) ret = 1;
		if (*mask == '*')   ret = 1;	/* match any */
	} else {				/* real wildcards not yet implemented, see regex(3) */
		char *l = strdupa(mask);
		char *oslot;
		int n, o[_POSIX_ARG_MAX];	/* o is our option vector for found options */

		ret = 1;
		if (!*argv)
			goto out;

		memset(o, 0, sizeof(o));
		while ((oslot = strsep(&l, ",:")) && *oslot) {
			for (n = 0; n < _POSIX_ARG_MAX && argv[n]; n++) {
				if (!strcmp(oslot, argv[n]))
					o[n]++;
			}
		}

						/* -lf is not -fl and not -l -f or -f -l */
		ret = 1;
		for (n = 0; n <= _POSIX_ARG_MAX && argv[n]; n++)
			if (!o[n])
				ret = 0;
	}
out:
	return ret;
}
			
static inline char * get_current_user ()
{
	struct passwd * pw = getpwuid(getuid());
	char * ret = "nobody";

	if (pw)
		ret = pw->pw_name;
	else
		warn("could not determine current user name ... defaulting to \"nobody\"\n");

	return ret;
}

static inline char * get_current_group ()
{
	struct group * gr = getgrgid(getgid());
	char * ret = "nogroup";

	if (gr)
		ret = gr->gr_name;
	else
		warn("could not determine current group name ... defaulting to \"nogroup\"\n");

	return ret;
}

static inline uid_t lookup_uid ( CHR name )
{
	struct passwd * pw = getpwnam(name);
	uid_t uid = -1;

	if (pw)
		uid = pw->pw_uid;
	else
		warn("could not determine current user id ... defaulting to \"nobody\"\n");

	return uid;
}

static inline gid_t lookup_gid ( CHR name )
{
	struct group * gr = getgrnam(name);
	gid_t gid = -1;

	if (gr)
		gid = gr->gr_gid;
	else
		warn("could not determine current group id ... defaulting to \"nogroup\"\n");

	return gid;
}

static inline char * lookup_pwd ( CHR name )
{
	struct passwd * pw = getpwnam(name);
	char * pwd = "/tmp";

	if (pw)
		pwd = pw->pw_dir;
	else
		warn("could not determine current home ... defaulting to \"/tmp\"\n");

	return pwd;
}

static inline char * lookup_shell ( CHR name )
{
	struct passwd * pw = getpwnam(name);
	char * shell = "/bin/false";

	if (pw)
		shell = pw->pw_shell;
	else
		warn("could not determine current shell ... defaulting to \"/bin/false\"\n");

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

	/*
	 * Get real root back
	 */
	if (seteuid(euid) < 0) error("seteuid() failed: %s\n", strerror(errno));
	if (setegid(egid) < 0) error("setegid() failed: %s\n", strerror(errno));

	if (setregid(new_gid, new_gid))
		error("setregid() failed: %s\n", strerror(errno));

	if (initgroups(entry->run_user, new_gid))
		error("initgroups() failed: %s\n", strerror(errno));

	if (setreuid(new_uid, new_uid))
		error("setreuid() failed: %s\n", strerror(errno));

	if (chdir(new_pwd))
		error("chdir() failed: %s\n", strerror(errno));

	execve(entry->run_command, entry->params, environ);
	error("execve() failed: %s\n", strerror(errno));
}

static char line[LINE_MAX];
static entry_t* check_against_table (CHR filename, char *argv[])
{
	FILE *table;
	char  *l = line;
	char *cslot;
	int   nslot, argc = 1, found = 0;
	char *user  = get_current_user();
	char *group = get_current_group();
	entry_t *e = &entry;

	if (!(table = fopen (filename, "r")))
		error("Could not open %s: %s\n", filename, strerror(errno));

	while (fgets(line, LINE_MAX, table)) {

		if (*(l = (char*)line) == '#') continue;
		if (*l == '\n') continue;

		memset((void*)e, 0, sizeof(e));
		nslot = 0;
		argc = 1;
		printf("====\n");
		while ((cslot = strsep(&l, " \t\n")) && *cslot) {
			switch (++nslot) {
			case 1:
				e->called_user = cslot;
				printf("called_user=%s\n", cslot);
				break;
			case 2:
				e->called_group = cslot;
				printf("called_group=%s\n", cslot);
				break;
			case 3:
				printf("called_command=%s\n", cslot);
				e->called_command = cslot;
				break;
			case 4:
				printf("called_cmdline=%s\n", cslot);
				e->called_cmdline = cslot;
				break;
			case 5:
				printf("run_user=%s\n", cslot);
				e->run_user = cslot;
				break;
			case 6:
				printf("run_group=%s\n", cslot);
				e->run_group = cslot;
				break;
			case 7:
				printf("run_cslot=%s\n", cslot);
				e->run_command = cslot;
				break;
			default:
				printf("run_params=%s\n", cslot);
				e->params[argc++] = cslot;
				break;
			}
		}
		if (!wildcard_match(e->called_command, my_basename(argv[0])))
			continue;
		if (!wildcard_match(e->called_user,    user))
			continue;
		if (!wildcard_match(e->called_group,   group))
			continue;
//		if (!cmdline_match(e->called_cmdline,  (argv+1)))
//			continue;
		if (!cmdline_match(e->called_cmdline, argv))
			continue;
		
		if (e->run_command && (!strcmp(e->run_command, "-")))
			error("explicitly forbidden\n");

		found++;
		break;
	}

	if (!found)
		e = NULL;
	else {
		e->params[0] = e->called_command; /* first argument */
		if (*(argv+1)) {
			char ** ptr = (argv+1);
			int n;

			for (n = 0; n < _POSIX_ARG_MAX && ptr[n] ; n++)
				e->params[argc++] = ptr[n];
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

	
	if (!strcmp(myname, my_basename(argv[0]))) {
		argc--;
		argv++;
	}

	entry = check_against_table(CONFIGFILE, argv);
	if (!entry)
		error("permission denied\n");

	if (getenv("SUWRAP_DEBUG"))
		verbose++;

	term = getenv("TERM");

	verbose++;
	

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
	if (!strcmp(entry->run_user, "root"))
		xputenv("PATH", "/sbin:/usr/sbin:/bin:/usr/bin:/usr/bin/X11");
	else
		xputenv("PATH", "/usr/local/bin:/bin:/usr/bin:/usr/bin/X11");
	if (errno)
		error("putenv() failed: %s\n", strerror(errno));
	if (xputenv("TERM", term))
		error("putenv() failed: %s\n", strerror(errno));

	do_su (entry);

	return 0; /* not reached */
}

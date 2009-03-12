/* TODO: use exec() insted of system -- 
    this will overwrite the wrapper's process image and so save memory 
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include "cmdline.h"

#define CONFIGFILE	"/etc/su-wrapper.conf"
typedef const char * CHR;

int verbose = 0;

CHR extract ( CHR fn )
{
	CHR key;

	key = fn + strlen(fn);
	if (key==fn) return NULL; /* hmm... no command */
	
	while ( (*key)!='/' ) {
	    if (key==fn) return key;
	    key--;
	}
	key++;
	return key;
}

#define ALLOW	0
#define DENY	1

typedef struct tagENTRY ENTRY;

struct tagENTRY {
	/* this must match with the calling params */
	char * called_user;
	char * called_group;
	char * called_command;
	char * called_cmdline;

	/* this will be executed */
	char * run_user;
	char * run_group;
	char * run_command;

	char * params;
		
	ENTRY * next;
};

ENTRY * entries = NULL; 

int add_entry ( 
    CHR called_user, CHR called_group, CHR called_command, CHR called_cmdline, 
    CHR run_user, CHR run_group, CHR run_cmdline, CHR param
)
{
    ENTRY * ent;
    ENTRY * walk;

    ent = (ENTRY*)malloc(sizeof(ENTRY));
    ent->next            = NULL;
    ent->called_user     = strdup(called_user);
    ent->called_group    = strdup(called_group);
    ent->called_command  = strdup(called_command);
    ent->called_cmdline  = strdup(called_cmdline);
    ent->run_user        = strdup(run_user);
    ent->run_group       = strdup(run_group);
    ent->run_command     = strdup(run_cmdline);
    ent->params          = strdup(param);
    
    /* we put it on the end of the list */    
    if (entries==NULL) entries = ent;
    else {
	walk = entries;
	while (walk->next) walk=walk->next;
	walk->next = ent;
    }
}

int wildcard_match ( CHR mask, CHR value )
{
	if (strcmp(mask, "-")==0) return 1;	/* ignore -> match any */
	if (strcmp(mask, "*")==0) return 1;	/* match any */
	/* real wildcars not yet implemented */
	if (strcmp(mask, value)==0) return 1;	/* direct match */
	return 0;
}

int cmdline_match ( CHR mask, CHR value )
{
	if (wildcard_match(mask, value)) return 1;
	else {
		int mask_argc;
		char * mask_argv[32];
		int val_argc;
		char * val_argv[32];
		int x;
		
		mask_argc = SplitCommandLine ( mask, (char**)&mask_argv );
		val_argc  = SplitCommandLine ( value,(char**) &val_argv );
		
		/* currently no wildcards are supported at this level */
		if (mask_argc!=val_argc) goto __out__failed;
		for (x=0; x<mask_argc; x++) {
			if (strcmp(mask_argv[x], val_argv[x])) goto __out__failed;
		}
		
		KillCommandLine ( mask_argc, (char**)&mask_argv );
		KillCommandLine ( val_argc, (char**)&val_argv );
		return 1;
		
__out__failed:
		KillCommandLine ( mask_argc, (char**)&mask_argv );
		KillCommandLine ( val_argc, (char**)&val_argv );
		return 0;
	}
}
			
/* try to match this entry with our requirements */
int try_to_match ( ENTRY * ent, CHR called_user, CHR called_group, 
	CHR called_command, CHR called_cmdline )
{
	/* compare called user */
	if (!wildcard_match(ent->called_user, called_user)) return 0;
	/* compare the group */
	if (!wildcard_match(ent->called_group, called_group)) return 0;
	/* compare the called_command */		
	if (!wildcard_match(ent->called_command, called_command)) return 0;
	/* compare the called cmdline */
	if (!cmdline_match(ent->called_cmdline, called_cmdline)) return 0;
	return 1;
}
	
ENTRY * FindMatching ( CHR called_user, CHR called_group, CHR called_cmd, CHR called_cmdline )
{
	ENTRY * walk;
	ENTRY * found;
	
	if (called_user==NULL) called_user = "nobody";
	if (called_group==NULL) called_group = "nogroup";

	if (verbose) {
	    printf("Trying to match : user=\"%s\" group=\"%s\" cmd=\"%s\" param=\"%s\"\n", 
		called_user, called_group, called_cmd, called_cmdline );
	}
		    
	found = NULL;
	walk  = entries;
	while (walk) {
		if (try_to_match(walk, called_user, called_group, 
			called_cmd, called_cmdline )) {
			found = walk;
			if (verbose) {
			    printf("Found match: run_user=\"%s\" run_group=\"%s\" run_cmdline=\"%s\" \n",
				found->run_user, found->run_group, found->run_command );
			}
		}
		walk = walk->next;
	}
	
	return found;
}

char * get_current_user ()
{
	struct passwd * pw;
	int uid;
	
	uid = getuid();
	setpwent();
	
	while (pw = getpwent()) {
		if (pw->pw_uid == uid) {
			return pw->pw_name;
		}
	}
	endpwent();
	printf("could not determine current user name ... defaulting to nobody\n");
	return "nobody";
}

char * get_current_group ()
{
	return "users";
}

int lookup_uid ( char * name )
{
	struct passwd * pw;
	
	setpwent();
	
	while (pw = getpwent()) {
		if (strcmp(pw->pw_name, name)==0) return pw->pw_uid;
	}
	
	endpwent();
	return -1;
}

int lookup_gid ( char * name )
{
	/* FIXME!!!! */
	
	if (strcmp(name,"root")==0) return 0;
	if (strcmp(name,"users")==0) return 100;
	if (strcmp(name,"daemon")==0) return 2;
	return -1;
}

int do_su ( CHR cmd, CHR cmdline )
{
	ENTRY * entry;
	int new_uid;
	int new_gid;
	
	entry = FindMatching ( get_current_user(), get_current_group(),
		cmd, cmdline );
	if (entry==NULL) {
		printf("su_wrapper: permission denied\n");
		return 0;
	}

	if (strcmp(entry->run_command, "-")==0) {
		printf("su_wrapper: explicitly forbidden\n");
		return 0;
	}
	new_uid = lookup_uid(entry->run_user);
	if (new_uid==-1) {
		printf("su_wrapper: no such user \"%s\"\n", entry->run_user);
		return 0;
	}
	new_gid = lookup_gid(entry->run_group);

	if (verbose) {
	    printf("Calling command : cmdline=\"%s\" user=\"%s\"(%d:%d) \n", 
		entry->run_command, entry->run_user, new_uid, new_gid );
	}
	
	if (setgid(new_gid)==-1) {
//		printf("su_wrapper: setgid() permission denied ! \n");
//		return 0;
	}

	if (setreuid(new_uid, new_uid)==-1) {
		printf("su_wrapper: setreuid() permission denied ! \n");
		printf("            did you set the SUID bit on the su_wrapper executable ?\n");
		return 0;
	}
	
	return system ( entry->run_command );
}

int read_line ( int fd, char * buffer )
{
	int ret;
	char * buf;
	
	buf = buffer;
	
	while (1) {
__loop:
	    ret = read(fd, buf, 1);
	    if (ret==0) goto __eof;
	    if (ret!=1) goto __err;
	    
	    if ((*buf)==10) goto __ln;
	    if ((*buf)==13) goto __ln;
	    buf++;
	}
__err:
	*buf = 0;
	return -errno;
__eof:	
	*buf = 0;
	if (buf==buffer) return 0;
	return 1;
__ln:
	if (buf==buffer) goto __loop;
	*buf = 0;	    
	return 1;
}

int load_table ( CHR filename )
{
	int fd;
	char buffer[512];
	int ret;
	char * chr;
	int argc;
	char * argv[16];
	int x;

	fd = open ( filename, O_RDONLY );
	if (fd<0) {
		printf("WARN: su_wrapper: could not load table \"%s\"\n", filename);
		return 0;
	}
	
	while (ret=read_line(fd, (char*)&buffer)==1) {
		/* skip out comments */
		chr = (char*)&buffer;
		while (*chr) {
			if ((*chr)=='#') *chr = 0;
			else chr++;
		}
		if (strcmp((char*)&buffer,"")==0);
		else {
			char * __user      = "";
			char * __group     = "";
			char * __command   = "";
			char * __cmdline   = "";
			char * __param     = "";
			char * __r_user    = "";
			char * __r_group   = "";
			char * __r_cmdline = "";
		
			argc = SplitCommandLine ( (char*)&buffer, (char**)&argv );
			
			if (argc>0) __user      = argv[0];
			if (argc>1) __group     = argv[1];
			if (argc>2) __command   = argv[2];
			if (argc>3) __cmdline   = argv[3];
			if (argc>4) __r_user    = argv[4];
			if (argc>5) __r_group   = argv[5];
			if (argc>6) __r_cmdline = argv[6];
			
			add_entry ( __user, __group, __command, __cmdline,
			    __r_user, __r_group, __r_cmdline, __param );
		}
	}
	
	close ( fd );
	return 1;
}

int main (int argc, char * argv[])
{
	char cmd_buffer[1024];
	int x;
	
	cmd_buffer[0] = 0;
	x = 1;

	if ((argc>1)&&(strcmp(argv[1],"--verbose")==0)) {
	    verbose = 1;
	    x++;
	}
	
	while (x<argc) {
	    strcat ( (char*)&cmd_buffer, argv[x] );
	    if (x<argc) strcat ( (char*)&cmd_buffer, " " );
	    x++;
	}
	
	load_table( CONFIGFILE );
	
	do_su ( extract(argv[0]), (char*)&cmd_buffer );
}

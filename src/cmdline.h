/*

    Command line tools

    Copyright (C) 2001 Enrico Weigelt <weigelt@nibiru.thur.de>

    this code is released under the terms of the GPL.
    		  
*/

#ifndef __cmdline_tools__
#define __cmdline_tools__

#include <string.h>
#include <stdlib.h>

//#ifdef __cplusplus
//extern "C" {
//#endif

//#ifndef CMDLINE_SPACE
//#define CMDLINE_SPACE_DEF ' '
//    static char CMDLINE_SPACE = CMDLINE_SPACE_DEF;
//#endif

//#pragma hdrstop
#define SplitCommandLine(l,b)	SplitCommandLineExt(l,b," \t\n")

static inline int __is_cmdline_space ( char c, const char * spaces )
{
	while (*spaces) {
	    if ((*spaces)==c) return 1;
	    spaces++;
	}
	return 0;
}

static int SplitCommandLineExt(const char *c,char *hello[], const char * spaces )
{
    int paramcount = 0;
    char buffer[100];
    int counter;

    while (!(*c==0)) {
	/* skip the spaces */
//	while ( (!(*c==0)) && (*c==CMDLINE_SPACE) ) c++;
	while ( (!(*c==0)) && (__is_cmdline_space(*c,spaces)) ) c++;
	if (!(*c==0)) {
	    counter = 0;
	    /* copy non-spaces */
	    if (*c=='"') { /* an "" string comes ... -> copy it ! */
		c++;
		while ( (!(*c==0)) && (!(*c=='"')) ) {
		    buffer[counter] = *c;
		    counter++;
		    c++;
		}
	    }
	    else {
		while ( (!(*c==0)) && (!(__is_cmdline_space(*c, spaces))) ) {
		    buffer[counter] = *c;
		    counter++;
		    c++;
		}
	    }
	    /* copy the buffer into argv */
	    buffer[counter] = 0;
	    counter++;
	    hello[paramcount] = (char*)malloc(counter++);
	    strcpy(hello[paramcount],buffer);
	    paramcount++;
	}
    }
    /* do some rest work */
    return paramcount++;
}

static int KillCommandLine(int argc, char * argv[])
{
    int n;
    if (argc==0) return 0;
// for (n=0; n<argc; n++) free(argv[n]);
    return argc;
}

static int GetValue(const char * str)
{
    int x = 0;
    if (str == NULL) return 0;
    while ( ((*str)>('0'-1)) && ((*str)<('9'+1)) ) { x = x * 10 + (*str) - '0'; str++; }
    return x;
}

static int GetValueX(const char * str, int * ret)
{
    if (ret==NULL) return -1;
    if (str==NULL) return -1;

    *ret = 0;
    while ( ((*str)>('0'-1)) && ((*str)<('9'+1)) ) { (*ret) = (*ret) * 10 + (*str) - '0'; str++; }
    if ((*str)==0) return 1;
    return 0;
}

static unsigned char HexToChar ( unsigned char a, unsigned char b )
{
    unsigned char c;
    
    c = 0;
    
    if ( (a>('0'-1)) && (a<('9'+1)) ) c += a - '0';
    if ( (a>('A'-1)) && (a<('F'+1)) ) c += a - 'A' + 10;
    if ( (a>('a'-1)) && (a<('f'+1)) ) c += a - 'a' + 10;
    
    c *= 16;
    
    if ( (b>('0'-1)) && (b<('9'+1)) ) c += b - '0';
    if ( (b>('A'-1)) && (b<('F'+1)) ) c += b - 'A' + 10;
    if ( (b>('a'-1)) && (b<('f'+1)) ) c += b - 'a' + 10;
        
    return c;
}

static int GetHexValueBuf(const char * str, char * buffer,int bufmax)
{
    int a;
    int x;
    int y;
    
    a = strlen ( str );
    
    if (a % 2 ) { /* ungerade stellenanzahl */
	buffer[0] =  HexToChar ( '0' , str[0] );
	x = 1;	y = 1;
    }
    else {
	x = 0;	y = 0;
    }
    
    while ((str[x])&&(y<bufmax)) {
	buffer[y] = HexToChar ( str[x], str[x+1] );
	x++;
	x++;
	y++;
    }
    return y;
}


static char * CatCommandLine ( int argc, char * argv[] )
{
    int x;
    int y;
    int z;
    int size;
    char * tmp;
    char * data;
            
    size = 2;
    z = 0;
    for (x=0; x<argc; x++) if (argv[x]!=NULL) size+=strlen(argv[x])+1;
    
    data = (char*)malloc(size);
    for (x=0; x<argc; x++) {
	tmp = argv[x];
	y=0;
	while (tmp[y]!=0) {
	    data[z]=tmp[y];
	    y++;
	    z++;
	}
	data[z]=' ';
	z++;
    }
    data[z]=0;
    return data;
}

static inline char * strnew(const char * str)
{
    char * s;
    
    if (str==NULL) return NULL;
    s = (char*)malloc(strlen(str)+1);
    strcpy(s,str);
    return s;
}

#define _cmd(cmdparam,handler) else if (strcmp(_CMD_ARG,cmdparam)==0) handler

#define _CMD_START(var) if (1==0)

//#ifdef __cplusplus 
//}
//#endif

#endif

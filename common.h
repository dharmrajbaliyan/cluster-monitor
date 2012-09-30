#ifndef __COMMON__
#define __COMMON__

#define ERROR(msg) \
do {	perror(msg);	exit(EXIT_FAILURE); }while(0)

#define LISTENQUEUE 50
#define MAXLENGTH 1024

#endif //__COMMON__

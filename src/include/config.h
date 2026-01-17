#ifndef config_h
#define config_h

//#define _GNU_SOURCE
// #include <stdatomic.h>

#if __cpluspluss
# define FALSE  false
# define TRUE   true
#else
# define FALSE (0)
# define TRUE  (!(FALSE))
#endif

// Comment to enable HTTP
//#define MCP_STDIO

extern int done;

extern void processing_loop(void);
#endif
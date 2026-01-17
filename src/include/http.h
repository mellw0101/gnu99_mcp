#ifndef http_h
#define http_h

#define MCP_PORT 8100

extern void process_http(void);
extern int init_http(void);
extern void end_http(void);
extern void http_200_json(int cfd, const char* body);
extern void http_202(int cfd);


#endif
#pragma once


#include "def.h"


extern int done;

_BEGIN_C_LINKAGE

/* ---------------------------------------------------------- processing.c ---------------------------------------------------------- */


void processing_loop(void);


/* ---------------------------------------------------------- mcp.c ---------------------------------------------------------- */


void
mcp_tool_arg_add(MCPTool tool, const char *name, const char *description, MCPToolArgType type);

MCPTool
mcp_tool_add(const char *name, const char *description, MCPToolCb cb);

void
mcp_init(void);

// void add_argument(struct tool *tool, const char *name, enum type type, const char *description);
// tool *add_tool(const char *name, const char *description);
cJSON *ok(cJSON *id, cJSON *result);
cJSON *err(cJSON *id, int code, const char *msg);
cJSON *create_result_text(const char *text);
cJSON *handle_fetch(void);
cJSON *handle_tools_call(cJSON *id, cJSON *params);
void dispatch(const char *line,int fd);


/* ---------------------------------------------------------- http.c ---------------------------------------------------------- */


void process_http(void);
int init_http(void);
void end_http(void);
void http_200_json(int cfd, const char* body);
void http_202(int cfd);


/* ---------------------------------------------------------- tools.h ---------------------------------------------------------- */


void define_tools(void);


/* ---------------------------------------------------------- stdio.h ---------------------------------------------------------- */


void process_stdio(void);
int init_stdio(void);
void end_stdio(void);


_END_C_LINKAGE

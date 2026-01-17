// mcp_stdio_server.c — minimal MCP stdio server (NDJSON, no Content-Length)
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/cJSON.h"
#include "../include/config.h"
#include "../include/http.h"
#include "../include/stdio_transport.h"
#include "../include/tools.h"

int done = 0;

#define _PP_CAT(a, b)  a##b
#define PP_CAT(a, b)   _PP_CAT(a, b)

#if MCP_STDIO
# define MCP_CALL(x)  PP_CAT(x, _stdio)
#else
# define MCP_CALL(x)  PP_CAT(x, _http)
#endif

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int __attribute__((__unused__)) sig) {
  g_stop = 1;
}

int main(void) {
  signal(SIGINT, on_sigint);
  define_tools();
  if (MCP_CALL(init)() != 0) {
    fprintf(stderr, "Failed to initialize MCP client\n");
    return 1;
  }
  while (!done && !g_stop) {
    MCP_CALL(process)();
    processing_loop();
  }
  MCP_CALL(end)();
  return 0;
}
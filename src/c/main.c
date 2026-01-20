#include "../include/proto.h"


#if MCP_STDIO
# define MCP_CALL(x)  PP_CAT(x, _stdio)
#else
# define MCP_CALL(x)  PP_CAT(x, _http)
#endif


int done = 0;



static volatile sig_atomic_t g_stop = 0;


static void on_sigint(int _UNUSED sig) {
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
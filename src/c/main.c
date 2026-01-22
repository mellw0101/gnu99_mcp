#include "../include/proto.h"


#if MCP_STDIO
# define init       init_stdio
# define process    process_stdio
# define end        end_stdio
#else
# define init       init_http
# define process    process_http
# define end        end_http
#endif



int done = 0;



static volatile sig_atomic_t g_stop = 0;


static void
on_sigint(int _UNUSED sig) {
  g_stop = 1;
}

int
main(void) {
  mcp_init();
  signal(SIGINT, on_sigint);
  define_tools();
  if (init() != 0) {
    fprintf(stderr, "Failed to initialize MCP client\n");
    return 1;
  }
  while (!done && !g_stop) {
    process();
    processing_loop();
    /* Run at 30 hz. */
    hiactime_msleep(16.666);
  }
  end();
  return 0;
}

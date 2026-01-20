#include "../include/proto.h"


static char *line = NULL;


int init_stdio(void) {
  // No stdout noise — only valid MCP messages on stdout.
  setvbuf(stdout, NULL, _IONBF, 0);
  line = NULL;
  return 0;
}

void end_stdio(void) {
  free(line);
}

void process_stdio(void) {
  size_t cap = 0;
  size_t n = getline(&line, &cap, stdin);
  if (n <= 0) {
    done = 1;
    return;  // EOF
  }
  // Trim trailing \r?\n
  while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
    line[--n] = 0;
  }
  if (n == 0) {
    return;
  }
  dispatch(line, 0);
}
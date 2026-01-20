#include "../include/proto.h"


/* ---------------------------------------------------------- Define's ---------------------------------------------------------- */


#ifndef QUEUE_CAP
# define QUEUE_CAP 1024
#endif

#ifndef MSG_MAX
# define MSG_MAX 4096
#endif

#define NOT_ALLOWED_STR  "not allowed"
#define NOT_FOUND_STR    "not found"
#define ACCEPTED_STR     "Accepted"

#define CHUNK_SIZE  (4096)


/* ---------------------------------------------------------- Struct's ---------------------------------------------------------- */


typedef struct Msg_t         *Msg;
typedef struct HttpServer_t  *HttpServer;

struct Msg_t {
  int fd;
  char *data;
};

struct HttpServer_t {
  QUEUE    queue;
  mutex_t  mutex;
  cond_t   cond;
  thread_t thread;
  int listen_fd;
  int send_fd;
  /* There does not need to be any type of atomic operation. */
  int running;
};

// typedef struct {
//   char data[MSG_MAX];
//   int cfd;
// } msg_t;

// typedef struct {
//   msg_t ring[QUEUE_CAP];
//   size_t head;  // next pop
//   size_t tail;  // next push
//   mutex_t mu;
// } msg_queue_t;

__attribute__((__aligned__(4)))
// typedef struct {
//   int listen_fd;
//   int send_fd;
//   /* There does not need to be any type of atomic operation. */
//   int running;
// } http_server_t;


/* ---------------------------------------------------------- Variable's ---------------------------------------------------------- */


/* Expose non-blocking dequeue to your realtime loop */
// static msg_queue_t g_cmd_queue;
// static http_server_t g_srv;
// static pthread_t http_thr;

static HttpServer server = NULL;

// static char g_line[MSG_MAX];
static char hdr[8192 * 2];

/* ---------------------------------------------------------- Static-Function's ---------------------------------------------------------- */

static Msg
msg_create(int fd, char *data) {
  ASSERT(fd != -1);
  ASSERT(data);
  Msg m = xmalloc(sizeof *m);
  m->fd   = fd;
  m->data = data;
  return m;
}

static void
msg_free(Msg m) {
  free(m->data);
  free(m);
}

static HttpServer
http_server_create(void) {
  HttpServer hs = xmalloc(sizeof *hs);
  hs->queue = queue_create();
  mutex_init(&hs->mutex, NULL);
  cond_init(&hs->cond, NULL);
  return hs;
}

static void
http_server_push(int fd, char *data) {
  ASSERT(server);
  ASSERT(data);
  printf("%s\n", data);
  MUTEX_ACTION(&server->mutex,
    queue_push(server->queue, msg_create(fd, data));
  );
}

static Msg
http_server_pop(void) {
  ASSERT(server);
  Msg ret;
  MUTEX_ACTION(&server->mutex,
    ret = queue_front(server->queue);
    queue_pop(server->queue);
  );
  return ret;
}

  
static char *
read_socket(int fd, Ulong *outlen) {
  long bytes_read;
  Ulong len = 0;
  Ulong cap = CHUNK_SIZE;
  char *ret = xmalloc(cap);
  while ((bytes_read = read(fd, (ret + len), CHUNK_SIZE)) > 0) {
    len += bytes_read;
    cap += CHUNK_SIZE;
    ret = xrealloc(ret, CHUNK_SIZE);
  }
  if (bytes_read == -1) {
    free(ret);
    return NULL;
  }
  ret = xrealloc(ret, (len + 1));
  ret[len] = '\0';
  ASSIGN_IF_VALID(outlen, len);
  return ret; 
}

// static void queue_init(msg_queue_t *q) {
//   memset(q, 0, sizeof(*q));
//   mutex_init(&q->mu, NULL);
// }

// static bool queue_try_push(msg_queue_t *q, const char *line, int cfd) {
//   size_t next_tail;
//   printf("Enqueuing %s\n", line);
//   MUTEX_ACTION(&q->mu,
//     next_tail = (q->tail + 1) % QUEUE_CAP;
//     /* Full */
//     if (next_tail == q->head) {
//       mutex_unlock(&q->mu);
//       return FALSE;
//     }
//     q->ring[q->tail].cfd = cfd;
//     snprintf(q->ring[q->tail].data, MSG_MAX, "%s", line);
//     q->tail = next_tail;
//   );
//   return TRUE;
// }

// static bool queue_try_pop(msg_queue_t *q, char out[MSG_MAX], int *cfd) {
//   bool ok = FALSE;
//   pthread_mutex_lock(&q->mu);
//   if (q->head != q->tail) {
//     snprintf(out, MSG_MAX, "%s", q->ring[q->head].data);
//     *cfd = q->ring[q->head].cfd;
//     q->head = (q->head + 1) % QUEUE_CAP;
//     ok = TRUE;
//   }
//   pthread_mutex_unlock(&q->mu);
//   return ok;
// }

// static inline bool try_dequeue(char out[MSG_MAX], int *cfd) {
//   return queue_try_pop(&g_cmd_queue, out, cfd);
// }

static int set_sockopts(int fd) {
  int yes = 1;
  // modest recv timeout so  doesn't block the thread forever
  struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0 
  ||  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,   &tv, sizeof(tv))  < 0)
  {
    return -1;
  }
  return 0;
}

static int create_listen_socket(Ushort port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  if (set_sockopts(fd) < 0) {
    close(fd);
    return -1;
  }
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  if (listen(fd, 64) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static long recv_all(int fd, char *buf, Ulong want) {
  Ulong got = 0;
  long n;
  while (got < want) {
    n = recv(fd, (buf + got), (want - got), 0);
    /* Peer closed. */
    if (n == 0) {
      return (long)got;
    }
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    got += n;
  }
  return (long)got;
}

static void http_400(int fd, const char *msg) {
  printf("HTTP 400: %s\n", msg);
  dprintf(
    fd,
    "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Type: "
    "text/plain\r\nContent-Length: %lu\r\n\r\n%s",
    strlen(msg), msg
  );
}

static void http_404(int cfd) {
  const char *m = "not found";
  printf("HTTP 404\n");
  dprintf(cfd,
          "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: "
          "text/plain\r\nContent-Length: %zu\r\n\r\n%s",
          strlen(m), m);
}

static void http_405(int fd) {
  printf("HTTP 405\n");
  dprintf(
    fd,
    "HTTP/1.1 405 Method Not Allowed\r\nConnection: "
    "close\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s",
    SLTLEN(NOT_ALLOWED_STR), NOT_ALLOWED_STR
  );
}

void http_202(int fd) {
  printf("HTTP 202\n");
  dprintf(
    fd,
    "HTTP/1.1 202 Accepted\r\nConnection: close\r\nContent-Type: "
    "text/plain\r\nContent-Length: %zu\r\n\r\n%s",
    SLTLEN(ACCEPTED_STR), ACCEPTED_STR
  );
}

void http_200_json(int fd, const char *body) {
  size_t len = strlen(body);
  printf("HTTP 200: %s\n\n", body);
  dprintf(
    fd,
    "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: "
    "application/json\r\nContent-Length: %zu\r\n\r\n%s",
    len, body
  );
}

static void trim_trailing_newlines(char *s) {
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[--n] = 0;
  }
}

/* Handle exactly: POST /cmd HTTP/1.1, with single-line JSON body */
static void handle_http_client(int cfd) {
  char *cl;
  char *p;
  char *body;
  size_t used = 0;
  ssize_t n;
  char method[8];
  char path[64];
  char version[16];
  /* Read until we have headers (\r\n\r\n) */
  for (;;) {
    if (used >= sizeof(hdr)) {
      http_400(cfd, "headers too large");
      return;
    }
    n = recv(cfd, (hdr + used), (sizeof(hdr) - used), 0);
    if (n <= 0) {
      return;
    }
    used += n;
    /* search for end of headersk. */
    p = NULL;
    for (size_t i=3; i<used; ++i) {
      if (hdr[i - 3] == '\r' && hdr[i - 2] == '\n' && hdr[i - 1] == '\r' && hdr[i] == '\n') {
        p = &hdr[i + 1];  // first byte after CRLFCRLF
        break;
      }
    }
    if (!p) {
      continue;
    }
    /* Parse request line (must start at hdr) */
    if (sscanf(hdr, "%7s %63s %15s", method, path, version) != 3) {
      http_400(cfd, "bad request line");
      return;
    }
    printf("HTTP %s %s\n", method, path);
    if (strcmp(method, "GET") == 0) {
      if (strcmp(path, "/health") == 0) {
        http_200_json(cfd, "{\"status\":\"ok\"}\n");
        return;
      }
      else if (strcmp(path, "/mcp") == 0) {
        http_400(cfd, "GET not supported on /mcp, use POST\n");
        return;
      }
      else {
        http_404(cfd);
        return;
      }
    }
    // Only POST /cmd
    if (strcmp(method, "POST") != 0 || strcmp(path, "/mcp") != 0) {
      http_404(cfd);
      return;
    }

    // Find Content-Length
    size_t content_length = 0;
    {
      /* crude header parse (case-insensitive not required if you control client) */
      cl = strcasestr(hdr, "Content-Length:");
      if (!cl) {
        http_400(cfd, "missing content-length");
        return;
      }
      if (sscanf(cl, "Content-Length: %zu", &content_length) != 1) {
        if (sscanf(cl, "content-length: %zu", &content_length) != 1) {
          http_400(cfd, "bad content-length");
          return;
        }
      }
      printf("Content length: %lu\n", content_length);
      // if (content_length >= MSG_MAX) {
      //   http_400(cfd, "body too large");
      //   return;
      // }
    }

    // Compute already-buffered body bytes
    size_t header_bytes = (p - hdr);
    size_t have_in_buf = (used > header_bytes) ? (used - header_bytes) : 0;

    // if (!(body = read_socket(cfd, NULL))) {
    //   return;
    // }

    body = xmalloc(content_length + 1);
    if (have_in_buf > 0) {
      size_t copy = have_in_buf > content_length ? content_length : have_in_buf;
      memcpy(body, p, copy);
      // If there are extra pipelined bytes we ignore them; we close anyway.
      // Read remaining if needed:
      if (copy < content_length) {
        ssize_t m = recv_all(cfd, (body + copy), (content_length - copy));
        if (m < 0 || (size_t)m != content_length - copy) {
          return;
        }
      }
    }
    else {
      ssize_t m = recv_all(cfd, body, content_length);
      if (m < 0 || (size_t)m != content_length) {
        return;
      }
    }

    // body[content_length] = 0;
    trim_trailing_newlines(body);  // your convention: one-line JSON
    // Enqueue (non-blocking); if full, drop with 503-ish JSON
    http_server_push(cfd, body);
    // if (!queue_try_push(&g_cmd_queue, body, cfd)) {
    //   http_400(cfd, "Command queue full\n");
    // }
    return;
  }
}

static void *http_thread_main(void *_UNUSED arg) {
  server->running = TRUE;
  struct sockaddr_in cli;
  socklen_t slen = sizeof(cli);
  int cfd;
  while (server->running) {
    cfd = accept(server->listen_fd, (struct sockaddr *)&cli, &slen);
    if (cfd < 0) {
      if (errno == EINTR) {
        continue;
      }
      /* Brief nap to avoid hot loop on transient errors. */
      hiactime_nsleep(50000);
      continue;
    }
    handle_http_client(cfd);
  }
  return NULL;
}

static bool http_server_start(Ushort port) {
  ALWAYS_ASSERT((server->listen_fd = create_listen_socket(port)) >= 0);
  ALWAYS_ASSERT((thread_create(&server->thread, NULL, http_thread_main, NULL)) == 0);
  // g_srv.listen_fd = create_listen_socket(port);
  // if (g_srv.listen_fd < 0) {
  //   return FALSE;
  // }
  // g_srv.running = TRUE;
  // if (thread_create(&http_thr, NULL, (void *(*)(void *))http_thread_main, &g_srv) != 0) {
  //   close(g_srv.listen_fd);
  //   return FALSE;
  // }
  return TRUE;
}

static void http_server_stop(void) {
  server->running = FALSE;
  shutdown(server->listen_fd, SHUT_RDWR);
  close(server->listen_fd);
  thread_join(server->thread, NULL);
  queue_set_free_func(server->queue, (void (*)(void *))msg_free);
  queue_free(server->queue);
  free(server);
  // g_srv.running = FALSE;
  // // Kick accept() by closing listener
  // shutdown(g_srv.listen_fd, SHUT_RDWR);
  // close(g_srv.listen_fd);
  // pthread_join(http_thr, NULL);
}

void process_http(void) {
  Msg m;
  while (queue_size(server->queue)) {
    m = http_server_pop();
    dispatch(m->data, m->fd);
    close(m->fd);
    msg_free(m);
  }
  // int cfd;
  // while (try_dequeue(g_line, &cfd)) {
  //   dispatch(g_line, cfd);
  //   close(cfd);
  // }
}

int init_http(void) {
  // queue_init(&g_cmd_queue);
  server = http_server_create();
  if (!http_server_start(MCP_PORT)) {
    fprintf(stderr, "Failed to start HTTP server on port %u: %s\n", MCP_PORT, strerror(errno));
    return 1;
  }
  fprintf(stderr, "HTTP control listening on http://0.0.0.0:%u \n", MCP_PORT);
  return 0;
}

void end_http(void) {
  http_server_stop();
}
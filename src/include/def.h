#pragma once

#define _USE_ALL_BUILTINS
#include <fcio/proto.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"


#define MCP_PARSE_ERROR (-32700)
#define MCP_INVALID_REQUEST (-32600)
#define MCP_METHOD_NOT_FOUND (-32601)
#define MCP_INVALID_PARAMS (-32602)
#define MCP_INTERNAL_ERROR (-32603)

#define CHUNK_SIZE  (4096)

#define PROTOCOL_VERSION "2025-06-18"  // match spec

#define MCP_PORT 8100


/* ---------------------------------------------------------- Typedef's ---------------------------------------------------------- */


typedef struct MCPToolArg_t  *MCPToolArg;
typedef struct MCPTool_t     *MCPTool;

typedef cJSON *(*MCPToolCb)(cJSON *args);


/* ---------------------------------------------------------- Enum's ---------------------------------------------------------- */


typedef enum {
  MCPTOOL_ARGTYPE_STR,
  MCPTOOL_ARGTYPE_INT,
  MCPTOOL_ARGTYPE_FLOAT,
  MCPTOOL_ARGTYPE_BOOL,
#define MCPTOOL_ARGTYPE_STR    MCPTOOL_ARGTYPE_STR
#define MCPTOOL_ARGTYPE_INT    MCPTOOL_ARGTYPE_INT
#define MCPTOOL_ARGTYPE_FLOAT  MCPTOOL_ARGTYPE_FLOAT
#define MCPTOOL_ARGTYPE_BOOL   MCPTOOL_ARGTYPE_BOOL
} MCPToolArgType;


// typedef struct argument argument;
// typedef struct tool     tool;


// typedef enum type {
//   TYPE_STR,
//   TYPE_INT,
//   TYPE_FLOAT,
//   TYPE_BOOL
// } type;

// struct argument {
//   const char *name;
//   enum type type;           // "str", "int", "float", "bool"
//   const char *description;  // optional
//   struct argument *next;
// };

// struct tool {
//   const char *name;
//   const char *description;
//   struct argument *arguments;  // JSON schema as string
//   struct tool *next;
// };

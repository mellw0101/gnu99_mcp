#include <fcio/proto.h>
#include "../include/proto.h"


/* ---------------------------------------------------------- Struct's ---------------------------------------------------------- */


struct MCPToolArg_t {
  char *name;
  char *description;
  MCPToolArgType type;
  MCPToolArg prev;
  MCPToolArg next;
  /* Add a validation callback. */
};

struct MCPTool_t {
  char *name;
  char *description;
  MCPToolArg arg_head;
  MCPToolArg arg_tail;
  /* The action this tool does. */
  MCPToolCb cb;
};


/* ---------------------------------------------------------- Variable's ---------------------------------------------------------- */


/* linked list of registered tools. */
// static tool *tool_list = NULL;

static HMAP tools = NULL;


/* ---------------------------------------------------------- Static function's ---------------------------------------------------------- */

static MCPToolArg
mcp_tool_arg_create(const char *const restrict name, const char *const restrict description, MCPToolArgType type) {
  ASSERT(name);
  ASSERT(description);
  MCPToolArg a   = xmalloc(sizeof *a);
  a->name        = copy_of(name);
  a->description = copy_of(description);
  a->type        = type;
  a->prev        = NULL;
  a->next        = NULL;
  return a;
}

static MCPTool
mcp_tool_create(const char *const restrict name, const char *const restrict description, MCPToolCb cb) {
  MCPTool t = xmalloc(sizeof *t);
  t->name        = copy_of(name);
  t->description = copy_of(description);
  t->arg_head    = NULL;
  t->arg_tail    = NULL;
  t->cb          = cb;
  return t;
}

// static void
// free_argument_list(argument *head) {
//   argument *node;
//   while (head) {
//     node = head;
//     head = head->next;
//     free(node);
//   }
// }

// static void
// free_tool_list(tool *head) {
//   tool *node;
//   while (head) {
//     node = head;
//     DLIST_ADV_NEXT(head);
//     free_argument_list(node->arguments);
//     free(node);
//   }
// }

// static void
// free_tools(void) {
//   free_tool_list(tool_list);
//   tool_list = NULL;
// }

static void
mcp_tool_get_args_as_json(MCPTool tool, cJSON *props, cJSON *req) {
  cJSON *json_arg;
  const char *type;
  DLIST_FOR_NEXT(tool->arg_head, arg) {
    json_arg = cJSON_CreateObject();
    switch (arg->type) {
      case MCPTOOL_ARGTYPE_STR: {
        cJSON_AddStringToObject(json_arg, "type", "string");
        break;
      }
      case MCPTOOL_ARGTYPE_FLOAT:
      case MCPTOOL_ARGTYPE_INT: {
        cJSON_AddStringToObject(json_arg, "type", "number");
        break;
      }
      case MCPTOOL_ARGTYPE_BOOL: {
        cJSON_AddStringToObject(json_arg, "type", "boolean");
        break;
      }
      default: {
        ALWAYS_ASSERT_MSG(0, "'arg->type' Is not a valid type.");
      }
    }
    cJSON_AddStringToObject(json_arg, "description", arg->description);
    cJSON_AddItemToObject(props, arg->name, json_arg);
    cJSON_AddItemToArray(req, cJSON_CreateString(arg->name));
  }
}

// static void
// add_arguments(cJSON *props, cJSON *req, tool *tool) {
//   argument *arg = tool->arguments;
//   while (arg) {
//     cJSON *jsonArg = cJSON_CreateObject();
//     if (arg->type == TYPE_STR) {
//       cJSON_AddStringToObject(jsonArg, "type", "string");
//     }
//     else if (arg->type == TYPE_INT || arg->type == TYPE_FLOAT) {
//       cJSON_AddStringToObject(jsonArg, "type", "number");
//     }
//     else if (arg->type == TYPE_BOOL) {
//       cJSON_AddStringToObject(jsonArg, "type", "boolean");
//     }
//     /* Default to string. */
//     else {
//       cJSON_AddStringToObject(jsonArg, "type", "string");
//     }
//     if (arg->description) {
//       cJSON_AddStringToObject(jsonArg, "description", arg->description);
//     }
//     cJSON_AddItemToObject(props, arg->name, jsonArg);
//     cJSON_AddItemToArray(req, cJSON_CreateString(arg->name));
//     arg = arg->next;
//   }
// }

// static cJSON *
// get_json_for_tool(tool *tool) {
//   cJSON *t      = cJSON_CreateObject();
//   cJSON *schema = cJSON_CreateObject();
//   cJSON *props  = cJSON_CreateObject();
//   cJSON *req    = cJSON_CreateArray();
//   cJSON_AddStringToObject(t, "name", tool->name);
//   cJSON_AddStringToObject(t, "description", tool->description);
//   cJSON_AddStringToObject(schema, "type", "object");
//   add_arguments(props, req, tool);
//   cJSON_AddItemToObject(schema, "properties", props);
//   cJSON_AddItemToObject(schema, "required", req);
//   cJSON_AddItemToObject(t, "inputSchema", schema);  // camelCase per spec
//   return (t);
// }

static cJSON *
mcp_tool_get_as_json(MCPTool tool) {
  cJSON *t      = cJSON_CreateObject();
  cJSON *schema = cJSON_CreateObject();
  cJSON *props  = cJSON_CreateObject();
  cJSON *req    = cJSON_CreateArray();
  cJSON_AddStringToObject(t, "name", tool->name);
  cJSON_AddStringToObject(t, "description", tool->description);
  cJSON_AddStringToObject(schema, "type", "object");
  mcp_tool_get_args_as_json(tool, props, req);
  cJSON_AddItemToObject(schema, "properties", props);
  cJSON_AddItemToObject(schema, "required", req);
  cJSON_AddItemToObject(t, "inputSchema", schema);
  return t;
}

static void
send_json(cJSON *obj, int _UNUSED cfd) {
  char *s = cJSON_PrintUnformatted(obj);  // single line, no pretty \n
#if defined(MCP_STDIO)
  fputs(s, stdout);
  fputc('\n', stdout);  // newline = message boundary
  fflush(stdout);
#else
  log_INFO_0("Responding %s\n",s);
  http_200_json(cfd, s);
#endif
  free(s);
}

static cJSON *
handle_initialize(cJSON *id/* , cJSON *params */) {
  cJSON *result     = cJSON_CreateObject();
  cJSON *caps       = cJSON_CreateObject();
  cJSON *ts         = cJSON_CreateObject();
  cJSON *serverInfo = cJSON_CreateObject();
  cJSON_AddStringToObject(result,     "protocolVersion", PROTOCOL_VERSION);
  cJSON_AddBoolToObject(  ts,      "listChanged",     0);
  cJSON_AddItemToObject(  caps,       "tools",           ts);
  cJSON_AddItemToObject(  result,     "capabilities",    caps);
  cJSON_AddStringToObject(serverInfo, "name",            "c-mcp-stdio");
  cJSON_AddStringToObject(serverInfo, "version",         "0.2.0");
  cJSON_AddItemToObject(  result,     "serverInfo",      serverInfo);
  return ok(id, result);
}

static void
add_one_tool_to_json(const char *key, MCPTool tool, cJSON *list) {
  cJSON_AddItemToArray(list, mcp_tool_get_as_json(tool));
}

static cJSON *
handle_tools_list(cJSON *id) {
  cJSON *result = cJSON_CreateObject();
  cJSON *ts     = cJSON_CreateArray();
  hmap_forall_wdata(tools, (void (*)(const char *, void *, void *))add_one_tool_to_json, ts);
  cJSON_AddItemToObject(result, "tools", ts);
  return ok(id, result);
}

static cJSON *
handle_ping(cJSON *id) {
  return ok(id, cJSON_CreateObject());
}

/* ---------------------------------------------------------- Global function's ---------------------------------------------------------- */


void
mcp_tool_arg_add(MCPTool tool, const char *name, const char *description, MCPToolArgType type) {
  ASSERT(tool);
  ASSERT(name);
  ASSERT(description);
  MCPToolArg a = mcp_tool_arg_create(name, description, type);
  if (!tool->arg_head) {
    tool->arg_head = a;
    tool->arg_tail = a;
  }
  else {
    a->prev              = tool->arg_tail;
    tool->arg_tail->next = a;
    DLIST_ADV_NEXT(tool->arg_tail);
  }
}

MCPTool
mcp_tool_add(const char *name, const char *description, MCPToolCb cb) {
  ASSERT(tools);
  MCPTool t = mcp_tool_create(name, description, cb);
  hmap_insert(tools, name, t);
  return t;
}

void
mcp_init(void) {
  tools = hmap_create();
}

// void
// add_argument(tool *tool, const char *name, type type, const char *description) {
//   argument *arg = xmalloc(sizeof *arg);
//   arg->name = name;
//   arg->type = type;
//   arg->description = description;
//   arg->next = tool->arguments;
//   tool->arguments = arg;
// }

// tool *
// add_tool(const char *name, const char *description) {
//   tool *t = xmalloc(sizeof *t);
//   t->name        = name;
//   t->description = description;
//   t->arguments   = NULL;
//   t->next        = tool_list;
//   tool_list      = t;
//   return t;
// }

cJSON *
ok(cJSON *id, cJSON *result) {
  cJSON *m = cJSON_CreateObject();
  cJSON_AddStringToObject(m, "jsonrpc", "2.0");
  cJSON_AddItemToObject(m, "id", cJSON_Duplicate(id, 1));
  cJSON_AddItemToObject(m, "result", result);
  return m;
}

cJSON *
err(cJSON *id, int code, const char *msg) {
  cJSON *m = cJSON_CreateObject();
  cJSON_AddStringToObject(m, "jsonrpc", "2.0");
  if (id) {
    cJSON_AddItemToObject(m, "id", cJSON_Duplicate(id, 1));
  }
  cJSON *e = cJSON_CreateObject();
  cJSON_AddNumberToObject(e, "code", code);
  cJSON_AddStringToObject(e, "message", msg);
  cJSON_AddItemToObject(m, "error", e);
  return m;
}

cJSON *
handle_fetch(void) {
  cJSON *result = cJSON_CreateObject();
  cJSON *cap    = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "mcpVersion", "0.1");
  cJSON_AddStringToObject(result, "name", "hyperbolic");
  cJSON_AddStringToObject(result, "version", "0.1.0");
  cJSON_AddBoolToObject(cap, "tools", 1);
  cJSON_AddItemToObject(result, "capabilities", cap);
  return result;
}

cJSON *
create_result_text(const char *text) {
  cJSON *res     = cJSON_CreateObject();
  cJSON *content = cJSON_CreateArray();
  cJSON *item    = cJSON_CreateObject();
  cJSON_AddStringToObject(item, "type", "text");
  cJSON_AddStringToObject(item, "text", text);
  cJSON_AddItemToArray(content, item);
  cJSON_AddItemToObject(res, "content", content);
  return res;
}

cJSON *
handle_tools_call(cJSON *id, cJSON *params) {
  MCPTool tool;
  cJSON *name;
  cJSON *arguments;
  cJSON *result = NULL;
  if (!cJSON_IsObject(params)) {
    return err(id, MCP_INVALID_PARAMS, "Invalid params");
  }
  name      = cJSON_GetObjectItemCaseSensitive(params, "name");
  arguments = cJSON_GetObjectItemCaseSensitive(params, "arguments");
  if (!cJSON_IsString(name) || !name->valuestring) {
    return err(id, MCP_INVALID_PARAMS, "Missing tool name");
  }
  if (!cJSON_IsObject(arguments)) {
    return err(id, MCP_INVALID_PARAMS, "Missing arguments");
  }
  /* If the tool exists, call its function. */
  if ((tool = hmap_get(tools, name->valuestring))) {
    result = tool->cb(arguments);
  }
  else {
    return err(id, MCP_METHOD_NOT_FOUND, "Unknown tool");
  }
  return ok(id, result);
  // if (strcmp(name->valuestring, "echo") == 0) {
  //   result = tool_echo(arguments);
  // }
  // else if (strcmp(name->valuestring, "add") == 0) {
  //   result = tool_add(arguments);
  // }
  // else if (strcmp(name->valuestring, "read_file") == 0) {
  //   result = tool_read_file(arguments);
  // }
}


void
dispatch(const char *line, int fd) {
  cJSON *resp;
  cJSON *root;
  cJSON *e;
  cJSON *id;
  cJSON *method;
  cJSON *params;
  const char *m;
  /* It was a get request */
  if (!*line) {
    resp = handle_fetch();
    send_json(resp, fd);
    cJSON_Delete(resp);
    return;
  }
  root = cJSON_Parse(line);
  if (!root) {
    e = err(NULL, MCP_PARSE_ERROR, "Parse error");
    send_json(e, fd);
    cJSON_Delete(e);
    return;
  }
  /* May be NULL for notifications. */
  id     = cJSON_GetObjectItemCaseSensitive(root, "id");
  method = cJSON_GetObjectItemCaseSensitive(root, "method");
  params = cJSON_GetObjectItemCaseSensitive(root, "params");
  if (!cJSON_IsString(method) || !method->valuestring) {
    e = err(id, MCP_INVALID_REQUEST, "Invalid Request");
    send_json(e, fd);
    cJSON_Delete(e);
    cJSON_Delete(root);
    return;
  }
  resp = NULL;
  m = method->valuestring;
  if (strcmp(m, "initialize") == 0) {
    resp = handle_initialize(id/* , params */);
  }
  else if (strcmp(m, "ping") == 0) {
    resp = handle_ping(id);
  }
  else if (strcmp(m, "tools/list") == 0) {
    resp = handle_tools_list(id);
  }
  else if (strcmp(m, "tools/call") == 0) {
    resp = handle_tools_call(id, params);
  }
  /* Notification: do NOT respond */
  else if (strcmp(m, "notifications/initialized") == 0) {
    http_202(fd);
    cJSON_Delete(root);
    return;
  }
  else {
    resp = err(id, MCP_METHOD_NOT_FOUND, "Method not found");
  }
  send_json(resp, fd);
  cJSON_Delete(resp);
  cJSON_Delete(root);
}

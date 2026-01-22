#include "../include/proto.h"


static cJSON *
tool_echo(cJSON *args) {
  const cJSON *text = cJSON_GetObjectItemCaseSensitive(args, "text");
  const char *s = (cJSON_IsString(text) && text->valuestring) ? text->valuestring : "";
  return create_result_text(s);
}

static cJSON *
tool_add(cJSON *args) {
  const cJSON *a = cJSON_GetObjectItemCaseSensitive(args, "a");
  const cJSON *b = cJSON_GetObjectItemCaseSensitive(args, "b");
  double ad = cJSON_IsNumber(a) ? a->valuedouble : 0.0;
  double bd = cJSON_IsNumber(b) ? b->valuedouble : 0.0;
  double sum = ad + bd;
  char buf[64];
  snprintf(buf, sizeof(buf), "%.17g", sum);
  return create_result_text(buf);
}


static char *
read_socket(int fd, Ulong *outlen) {
  long bytes_read;
  Ulong len = 0;
  Ulong cap = CHUNK_SIZE;
  char *ret = xmalloc(cap);
  fdlock_action(fd, F_RDLCK,
    while ((bytes_read = read(fd, (ret + len), CHUNK_SIZE)) > 0) {
      len += bytes_read;
      cap += CHUNK_SIZE;
      ret = xrealloc(ret, cap);
    }
  );
  if (bytes_read == -1) {
    free(ret);
    return NULL;
  }
  ret = xrealloc(ret, (len + 1));
  ret[len] = '\0';
  ASSIGN_IF_VALID(outlen, len);
  return ret; 
}

static char *
read_file(const char *const restrict path) {
  ASSERT(path);
  ASSERT(file_exists(path));
  int fd;
  char *ret;
  ALWAYS_ASSERT(fd = open(path, O_RDONLY));
  ret = read_socket(fd, NULL);
  close(fd);
  return ret;
}

static cJSON *
tool_read_file(cJSON *args) {
  const cJSON *path = cJSON_GetObjectItemCaseSensitive(args, "path");
  const char *s = ((cJSON_IsString(path) && path->valuestring) ? path->valuestring : "");
  char *file_data;
  cJSON *ret;
  if (*s) {
    if (!file_exists(s)) {
      return create_result_text("The provided path does not exist.");
    }
    file_data = read_file(s);
    if (!file_data) {
      return create_result_text("Failed to read file.");
    }
    else {
      ret = create_result_text(file_data);
      free(file_data);
      return ret;
    }
  }
  else {
    return create_result_text(s);
  }
}

// cJSON *
// handle_tools_call(cJSON *id, cJSON *params) {
//   cJSON *name;
//   cJSON *arguments;
//   cJSON *result = NULL;
//   if (!cJSON_IsObject(params)) {
//     return err(id, MCP_INVALID_PARAMS, "Invalid params");
//   }
//   name      = cJSON_GetObjectItemCaseSensitive(params, "name");
//   arguments = cJSON_GetObjectItemCaseSensitive(params, "arguments");
//   if (!cJSON_IsString(name) || !name->valuestring) {
//     return err(id, MCP_INVALID_PARAMS, "Missing tool name");
//   }
//   if (!cJSON_IsObject(arguments)) {
//     return err(id, MCP_INVALID_PARAMS, "Missing arguments");
//   }
//   if (strcmp(name->valuestring, "echo") == 0) {
//     result = tool_echo(arguments);
//   }
//   else if (strcmp(name->valuestring, "add") == 0) {
//     result = tool_add(arguments);
//   }
//   else if (strcmp(name->valuestring, "read_file") == 0) {
//     result = tool_read_file(arguments);
//   }
//   else {
//     return err(id, MCP_METHOD_NOT_FOUND, "Unknown tool");
//   }
//   return ok(id, result);
// }

void define_tools(void) {
  MCPTool echo;
  MCPTool add;
  MCPTool read_file;
  /* Echo */
  echo = mcp_tool_add("echo", "Echo input", tool_echo);
  mcp_tool_arg_add(echo, "text", "Text to echo", MCPTOOL_ARGTYPE_STR);
  /* Add */
  add = mcp_tool_add("add", "Add two numbers", tool_add);
  mcp_tool_arg_add(add, "a", "First number",  MCPTOOL_ARGTYPE_FLOAT);
  mcp_tool_arg_add(add, "b", "Second number", MCPTOOL_ARGTYPE_FLOAT);
  /* Read-File */
  read_file = mcp_tool_add("read_file", "Read any local file", tool_read_file);
  mcp_tool_arg_add(read_file, "path", "The absolute path of the file to be read.", MCPTOOL_ARGTYPE_STR);
  // tool *echoTool  = add_tool("echo", "Echo input text");
  // tool *addTool   = add_tool("add", "Add two numbers");
  // tool *read_file = add_tool("read_file", "Read any local file");
  // add_argument(echoTool, "text", TYPE_STR, "Text to echo");
  // /* Add arguments in reverse order (linked list), RETART ALERT...  This is such a non issue.  WTF. */
  // add_argument(addTool, "b", TYPE_FLOAT, "Second number");
  // add_argument(addTool, "a", TYPE_FLOAT, "First number");
  // add_argument(read_file, "path", TYPE_STR, "The absolute path of the file to be read.");
}

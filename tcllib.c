#include <stdlib.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "tcl.h"

#if 0
#define DBG printf
#else
#define DBG(...)
#endif

#ifndef MAX_VAR_LENGTH
#define MAX_VAR_LENGTH 256
#endif

struct tcl;
int tcl_eval(struct tcl* tcl, const char* s, size_t len);

tcl_token_t tcl_next(const char* s,
                     size_t n,
                     const char** from,
                     const char** to,
                     int* q)
{
  unsigned int i = 0;
  int depth = 0;
  char open;
  char close;

  DBG("tcl_next(%.*s)+%d+%d|%d\n", n, s, *from - s, *to - s, *q);

  /* Skip leading spaces if not quoted */
  int qq = *q;

  for (; !qq && n > 0 && tcl_is_space(*s); s++, n--) {
  }
  *from = s;
  /* Terminate command if not quoted */
  if (!qq && n > 0 && tcl_is_end(*s)) {
    *to = s + 1;
    return TCMD;
  }
  if (*s == '$') { /* Variable token, must not start with a space or quote */
    if (tcl_is_space(s[1]) || s[1] == '"') {
      return TERROR;
    }

    // heh?
    *q = 0;
    int r = tcl_next(s + 1, n - 1, to, to, q);
    *q = mode;

    return ((r == TWORD && qq) ? TPART : r);
  }

  if (*s == '[' || (!qq && *s == '{')) {
    /* Interleaving pairs are not welcome, but it simplifies the code */
    open = *s;
    close = (open == '[' ? ']' : '}');
    for (i = 0, depth = 1; i < n && depth != 0; i++) {
      if (i > 0 && s[i] == open) {
        depth++;
      } else if (s[i] == close) {
        depth--;
      }
    }
  } else if (*s == '"') {
    qq = !qq;
    *q = qq;

    *from = *to = s + 1;
    if (qq) {
      return TPART;
    }
    if (n < 2 || (!tcl_is_space(s[1]) && !tcl_is_end(s[1]))) {
      return TERROR;
    }
    *from = *to = s + 1;
    return TWORD;
  } else {
    while (i < n && (qq || !tcl_is_space(s[i])) && !tcl_is_special(s[i], qq)) {
      i++;
    }
  }
  *to = s + i;
  if (i == n) {
    return TERROR;
  }
  if (qq) {
    return TPART;
  }
  return (tcl_is_space(s[i]) || tcl_is_end(s[i])) ? TWORD : TPART;
}

/* ------------------------------------------------------- */
/* ------------------------------------------------------- */
/* ------------------------------------------------------- */
/* ------------------------------------------------------- */
/* ------------------------------------------------------- */

static inline void* tcl_malloc(size_t n) { return TCL_MALLOC(n); }
static inline void tcl_free(void* v) { TCL_FREE(v); }
static inline void* tcl_realloc(void* v, int n) { return TCL_REALLOC(v, n); }

char* tcl_append_string(char* v, const char* s, size_t len)
{
  size_t n = tcl_length(v);
  v = tcl_realloc(v, n + len + 1);
  memset((char*)tcl_string(v) + n, 0, len + 1);
  strncpy((char*)tcl_string(v) + n, s, len);
  return v;
}

char* tcl_append(char* v, char* tail)
{
  v = tcl_append_string(v, tcl_string(tail), tcl_length(tail));
  tcl_free(tail);
  return v;
}

char* tcl_alloc(const char* s, size_t len)
{
  return tcl_append_string(NULL, s, len);
}

char* tcl_dup(char* v)
{
  return tcl_alloc(tcl_string(v), tcl_length(v));
}

char* tcl_list_alloc() { return tcl_alloc("", 0); }

int tcl_list_length(char* v)
{
  int count = 0;
  tcl_each(tcl_string(v), tcl_length(v) + 1, 0)
  {
    if (p.token == TWORD) {
      count++;
    }
  }
  return count;
}

void tcl_list_free(char* v) { free(v); }

char* tcl_list_at(char* v, int index)
{
  int i = 0;
  tcl_each(tcl_string(v), tcl_length(v) + 1, 0)
  {
    if (p.token == TWORD) {
      if (i == index) {
        if (p.from[0] == '{') {
          return tcl_alloc(p.from + 1, p.to - p.from - 2);
        }
        return tcl_alloc(p.from, p.to - p.from);
      }
      i++;
    }
  }
  return NULL;
}

char* tcl_list_append(char* v, char* tail)
{
  if (tcl_length(v) > 0) {
    v = tcl_append(v, tcl_alloc(" ", 2));
  }
  if (tcl_length(tail) > 0) {
    int q = 0;
    const char* p;
    for (p = tcl_string(tail); *p; p++) {
      if (tcl_is_space(*p) || tcl_is_special(*p, 0)) {
        q = 1;
        break;
      }
    }
    if (q) {
      v = tcl_append(v, tcl_alloc("{", 1));
    }
    v = tcl_append(v, tcl_dup(tail));
    if (q) {
      v = tcl_append(v, tcl_alloc("}", 1));
    }
  } else {
    v = tcl_append(v, tcl_alloc("{}", 2));
  }
  return v;
}

/* ----------------------------- */
/* ----------------------------- */
/* ----------------------------- */
/* ----------------------------- */

struct tcl_env* tcl_env_alloc(struct tcl_env* parent)
{
  struct tcl_env* env = tcl_malloc(sizeof(*env));
  env->vars = NULL;
  env->parent = parent;
  return env;
}

struct tcl_var* tcl_env_var(struct tcl_env* env, char* name)
{
  struct tcl_var* var = tcl_malloc(sizeof(struct tcl_var));
  var->name = tcl_dup(name);
  var->next = env->vars;
  var->value = tcl_alloc("", 0);
  env->vars = var;
  return var;
}

struct tcl_env* tcl_env_free(struct tcl_env* env)
{
  struct tcl_env* parent = env->parent;
  while (env->vars) {
    struct tcl_var* var = env->vars;
    env->vars = env->vars->next;
    tcl_free(var->name);
    tcl_free(var->value);
    tcl_free(var);
  }
  tcl_free(env);
  return parent;
}

char* tcl_var(struct tcl* tcl, char* name, char* v)
{
  DBG("var(%s := %.*s)\n", tcl_string(name), tcl_length(v), tcl_string(v));
  struct tcl_var* var;
  for (var = tcl->env->vars; var != NULL; var = var->next) {
    if (strcmp(var->name, tcl_string(name)) == 0) {
      break;
    }
  }
  if (var == NULL) {
    var = tcl_env_var(tcl->env, name);
  }
  if (v != NULL) {
    tcl_free(var->value);
    var->value = tcl_dup(v);
    tcl_free(v);
  }
  return var->value;
}

int tcl_result(struct tcl* tcl, int flow, char* result)
{
  DBG("tcl_result %.*s, flow=%d\n", tcl_length(result), tcl_string(result),
      flow);
  tcl_free(tcl->result);
  tcl->result = result;
  return flow;
}

int tcl_subst(struct tcl* tcl, const char* s, size_t len)
{
  DBG("subst(%.*s)\n", (int)len, s);
  if (len == 0) {
    return tcl_result(tcl, FNORMAL, tcl_alloc("", 0));
  }
  switch (s[0]) {
  case '{':
    if (len <= 1) {
      return tcl_result(tcl, FERROR, tcl_alloc("", 0));
    }
    return tcl_result(tcl, FNORMAL, tcl_alloc(s + 1, len - 2));
  case '$': {
    if (len >= MAX_VAR_LENGTH) {
      return tcl_result(tcl, FERROR, tcl_alloc("", 0));
    }
    char buf[5 + MAX_VAR_LENGTH] = "set ";
    strncat(buf, s + 1, len - 1);
    return tcl_eval(tcl, buf, strlen(buf) + 1);
  }
  case '[': {
    char* expr = tcl_alloc(s + 1, len - 2);
    int r = tcl_eval(tcl, tcl_string(expr), tcl_length(expr) + 1);
    tcl_free(expr);
    return r;
  }
  default:
    return tcl_result(tcl, FNORMAL, tcl_alloc(s, len));
  }
}

int tcl_eval(struct tcl* tcl, const char* s, size_t len)
{
  DBG("eval(%.*s)->\n", (int)len, s);
  char* list = tcl_list_alloc();
  char* cur = NULL;
  tcl_each(s, len, 1)
  {
    DBG("tcl_next %d %.*s\n", p.token, (int)(p.to - p.from), p.from);
    switch (p.token) {
    case TERROR:
      DBG("eval: FERROR, lexer error\n");
      return tcl_result(tcl, FERROR, tcl_alloc("", 0));
    case TWORD:
      DBG("token %.*s, length=%d, cur=%p (3.1.1)\n", (int)(p.to - p.from),
          p.from, (int)(p.to - p.from), cur);
      if (cur != NULL) {
        tcl_subst(tcl, p.from, p.to - p.from);
        char* part = tcl_dup(tcl->result);
        cur = tcl_append(cur, part);
      } else {
        tcl_subst(tcl, p.from, p.to - p.from);
        cur = tcl_dup(tcl->result);
      }
      list = tcl_list_append(list, cur);
      tcl_free(cur);
      cur = NULL;
      break;
    case TPART:
      tcl_subst(tcl, p.from, p.to - p.from);
      char* part = tcl_dup(tcl->result);
      cur = tcl_append(cur, part);
      break;
    case TCMD:
      if (tcl_list_length(list) == 0) {
        tcl_result(tcl, FNORMAL, tcl_alloc("", 0));
      } else {
        char* cmdname = tcl_list_at(list, 0);
        struct tcl_cmd* cmd = NULL;
        int r = FERROR;
        for (cmd = tcl->cmds; cmd != NULL; cmd = cmd->next) {
          char *c1 = tcl_string(cmdname);
          char *c2 =tcl_string(cmd->name);

          if (tcl_strcmp(c1, c2) == 0) {
            if (cmd->arity == 0 || cmd->arity == tcl_list_length(list)) {
              r = cmd->fn(tcl, list, cmd->arg);
              break;
            }
          }
        }
        tcl_free(cmdname);
        if (cmd == NULL || r != FNORMAL) {
          tcl_list_free(list);
          return r;
        }
      }
      tcl_list_free(list);
      list = tcl_list_alloc();
      break;
    }
  }
  tcl_list_free(list);
  return FNORMAL;
}

/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */
/* --------------------------------- */
void tcl_register(struct tcl* tcl, const char* name, tcl_cmd_fn_t fn, int arity,
    void* arg)
{
  struct tcl_cmd* cmd = tcl_malloc(sizeof(struct tcl_cmd));
  cmd->name = tcl_alloc(name, strlen(name));
  cmd->fn = fn;
  cmd->arg = arg;
  cmd->arity = arity;
  cmd->next = tcl->cmds;
  tcl->cmds = cmd;
}

static int tcl_cmd_set(struct tcl* tcl, char* args, void* arg)
{
  (void)arg;
  char* var = tcl_list_at(args, 1);
  char* val = tcl_list_at(args, 2);
  int r = tcl_result(tcl, FNORMAL, tcl_dup(tcl_var(tcl, var, val)));
  tcl_free(var);
  return r;
}

static int tcl_cmd_subst(struct tcl* tcl, char* args, void* arg)
{
  (void)arg;
  char* s = tcl_list_at(args, 1);
  int r = tcl_subst(tcl, tcl_string(s), tcl_length(s));
  tcl_free(s);
  return r;
}

static int tcl_user_proc(struct tcl* tcl, char* args, void* arg)
{
  char* code = (char*)arg;
  char* params = tcl_list_at(code, 2);
  char* body = tcl_list_at(code, 3);
  tcl->env = tcl_env_alloc(tcl->env);
  for (int i = 0; i < tcl_list_length(params); i++) {
    char* param = tcl_list_at(params, i);
    char* v = tcl_list_at(args, i + 1);
    tcl_var(tcl, param, v);
    tcl_free(param);
  }
  int r = tcl_eval(tcl, tcl_string(body), tcl_length(body) + 1);
  tcl->env = tcl_env_free(tcl->env);
  tcl_free(params);
  tcl_free(body);
  /* return FNORMAL; */
  return r;
}

static int tcl_cmd_proc(struct tcl* tcl, char* args, void* arg)
{
  (void)arg;
  char* name = tcl_list_at(args, 1);
  tcl_register(tcl, tcl_string(name), tcl_user_proc, 0, tcl_dup(args));
  tcl_free(name);
  return tcl_result(tcl, FNORMAL, tcl_alloc("", 0));
}

static int tcl_cmd_if(struct tcl* tcl, char* args, void* arg)
{
  (void)arg;
  int i = 1;
  int n = tcl_list_length(args);
  int r = FNORMAL;
  while (i < n) {
    char* cond = tcl_list_at(args, i);
    char* branch = NULL;
    if (i + 1 < n) {
      branch = tcl_list_at(args, i + 1);
    }
    r = tcl_eval(tcl, tcl_string(cond), tcl_length(cond) + 1);
    tcl_free(cond);
    if (r != FNORMAL) {
      tcl_free(branch);
      break;
    }
    if (tcl_int(tcl->result)) {
      r = tcl_eval(tcl, tcl_string(branch), tcl_length(branch) + 1);
      tcl_free(branch);
      break;
    }
    i = i + 2;
    tcl_free(branch);
  }
  return r;
}

static int tcl_cmd_flow(struct tcl* tcl, char* args, void* arg)
{
  (void)arg;
  int r = FERROR;
  char* flowval = tcl_list_at(args, 0);
  const char* flow = tcl_string(flowval);
  if (strcmp(flow, "break") == 0) {
    r = FBREAK;
  } else if (strcmp(flow, "continue") == 0) {
    r = FAGAIN;
  } else if (strcmp(flow, "return") == 0) {
    r = tcl_result(tcl, FRETURN, tcl_list_at(args, 1));
  }
  tcl_free(flowval);
  return r;
}

static int tcl_cmd_while(struct tcl* tcl, char* args, void* arg)
{
  (void)arg;
  char* cond = tcl_list_at(args, 1);
  char* loop = tcl_list_at(args, 2);
  int r;
  for (;;) {
    r = tcl_eval(tcl, tcl_string(cond), tcl_length(cond) + 1);
    if (r != FNORMAL) {
      tcl_free(cond);
      tcl_free(loop);
      return r;
    }
    if (!tcl_int(tcl->result)) {
      tcl_free(cond);
      tcl_free(loop);
      return FNORMAL;
    }
    int r = tcl_eval(tcl, tcl_string(loop), tcl_length(loop) + 1);
    switch (r) {
    case FBREAK:
      tcl_free(cond);
      tcl_free(loop);
      return FNORMAL;
    case FRETURN:
      tcl_free(cond);
      tcl_free(loop);
      return FRETURN;
    case FAGAIN:
      continue;
    case FERROR:
      tcl_free(cond);
      tcl_free(loop);
      return FERROR;
    }
  }
  return FERROR;
}

#ifndef TCL_DISABLE_MATH
static int tcl_cmd_math(struct tcl* tcl, char* args, void* arg)
{
  (void)arg;
  char buf[64];
  char* opval = tcl_list_at(args, 0);
  char* aval = tcl_list_at(args, 1);
  char* bval = tcl_list_at(args, 2);
  const char* op = tcl_string(opval);
  int a = tcl_int(aval);
  int b = tcl_int(bval);
  int c = 0;
  if (op[0] == '+') {
    c = a + b;
  } else if (op[0] == '-') {
    c = a - b;
  } else if (op[0] == '*') {
    c = a * b;
  } else if (op[0] == '/') {
    c = a / b;
  } else if (op[0] == '>' && op[1] == '\0') {
    c = a > b;
  } else if (op[0] == '>' && op[1] == '=') {
    c = a >= b;
  } else if (op[0] == '<' && op[1] == '\0') {
    c = a < b;
  } else if (op[0] == '<' && op[1] == '=') {
    c = a <= b;
  } else if (op[0] == '=' && op[1] == '=') {
    c = a == b;
  } else if (op[0] == '!' && op[1] == '=') {
    c = a != b;
  }

  char* p = buf + sizeof(buf) - 1;
  char neg = (c < 0);
  *p-- = 0;
  if (neg) {
    c = -c;
  }
  do {
    *p-- = '0' + (c % 10);
    c = c / 10;
  } while (c > 0);
  if (neg) {
    *p-- = '-';
  }
  p++;

  tcl_free(opval);
  tcl_free(aval);
  tcl_free(bval);
  return tcl_result(tcl, FNORMAL, tcl_alloc(p, strlen(p)));
}
#endif

void tcl_init(struct tcl* tcl)
{
  tcl->env = tcl_env_alloc(NULL);
  tcl->result = tcl_alloc("", 0);
  tcl->cmds = NULL;
  tcl_register(tcl, "set", tcl_cmd_set, 0, NULL);
  tcl_register(tcl, "subst", tcl_cmd_subst, 2, NULL);
#ifndef TCL_DISABLE_PUTS
  tcl_register(tcl, "puts", tcl_cmd_puts, 2, NULL);
#endif
  tcl_register(tcl, "proc", tcl_cmd_proc, 4, NULL);
  tcl_register(tcl, "if", tcl_cmd_if, 0, NULL);
  tcl_register(tcl, "while", tcl_cmd_while, 3, NULL);
  tcl_register(tcl, "return", tcl_cmd_flow, 0, NULL);
  tcl_register(tcl, "break", tcl_cmd_flow, 1, NULL);
  tcl_register(tcl, "continue", tcl_cmd_flow, 1, NULL);
#ifndef TCL_DISABLE_MATH
  char* math[] = { "+", "-", "*", "/", ">", ">=", "<", "<=", "==", "!=" };
  for (unsigned int i = 0; i < (sizeof(math) / sizeof(math[0])); i++) {
    tcl_register(tcl, math[i], tcl_cmd_math, 3, NULL);
  }
#endif
}

void tcl_destroy(struct tcl* tcl)
{
  while (tcl->env) {
    tcl->env = tcl_env_free(tcl->env);
  }
  while (tcl->cmds) {
    struct tcl_cmd* cmd = tcl->cmds;
    tcl->cmds = tcl->cmds->next;
    tcl_free(cmd->name);
    tcl_free(cmd->arg);
    tcl_free(cmd);
  }
  tcl_free(tcl->result);
}

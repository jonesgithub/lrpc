#ifndef __LRPC_MAIN_H__
#define __LRPC_MAIN_H__

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


int read_config(lua_State *L, const char *file_name);
int config_optint(lua_State *L, const char *key, int opt);
const char *config_optstring(lua_State *L, const char *key, const char *opt);

extern lua_State *L;

void _logging(const char *fmt, ...);
#define logging(fmt, args...) _logging("%s:%d : "fmt, __FILE__, __LINE__, ##args)
#endif

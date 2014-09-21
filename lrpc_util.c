#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <string.h>
#include <time.h>
#include "lrpc_main.h"

#define CONFIG_TBL_NAME "config_table"

int read_config(lua_State *L, const char *file_name)
{
    int r = luaL_loadfile(L, file_name);
    if (r != 0) return r;
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, CONFIG_TBL_NAME);
    lua_setupvalue(L, -2, 1);
    return lua_pcall(L, 0, 0, 0);
}

int config_optint(lua_State *L, const char *key, int opt)
{
    lua_getglobal(L, CONFIG_TBL_NAME);
    lua_getfield(L, -1, key);
    int ret;
    if (lua_isnil(L, -1))
        ret = opt;
    else
        ret = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return ret;
}

const char * config_optstring(lua_State *L, const char *key, const char *opt)
{
    lua_getglobal(L, CONFIG_TBL_NAME);
    lua_getfield(L, -1, key);
    const char *ret = opt;
    if (!lua_isnil(L, -1))
        ret = lua_tostring(L, -1);
    lua_pop(L, 2);
    return ret;
}

#define TIME_NOW_STR_LEN 22
static const char * _time_now() 
{
	time_t tm = time(NULL);
	struct tm *tm_now = localtime(&tm);

	static char _time_now_fmt[] = "[%04d-%02d-%02d %02d:%02d:%02d]";
	static char _time_now_str[TIME_NOW_STR_LEN];
	snprintf(_time_now_str, sizeof(_time_now_str), _time_now_fmt, tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
	return _time_now_str;
}


static int _open(const char *file_path)
{
    char tmp[256];
    strncpy(tmp, file_path, sizeof(tmp));
    char *p = tmp;
    while (*p) {
        p = strchr(p+1, '/');
        if (!p) break;
        *p = 0;
        struct stat buf;
        if (stat(tmp, &buf) == -1) {
            if (errno == ENOENT) {
                mkdir(tmp, 0755);
            } else {
                return -1;
            }
        }
        *p = '/';
    }

	return open(file_path, O_CREAT | O_APPEND | O_WRONLY, 0755);
}


static int fd = -1;
#define LOG_MAXLEN 65536
static char log_msg[LOG_MAXLEN];

void _logging(const char *fmt, ...)
{
    if (fd == -1) {
        const char *log_file = config_optstring(L, "log_file", NULL);
        fd = (log_file ? _open(log_file) : STDERR_FILENO);
        if (fd == -1) {
            fprintf(stderr, "open log file %s error : %s , replacing with stderr\n", log_file, strerror(errno));
            fd = STDERR_FILENO;
        }
    }

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(log_msg, LOG_MAXLEN, fmt, ap);
	va_end(ap);

    write(fd, _time_now(), TIME_NOW_STR_LEN-1);
    write(fd, log_msg, strlen(log_msg));
    write(fd, "\n", 1);
}



#include <assert.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>


#include "lrpc_main.h"

struct timer_event
{
    struct event _ev;
    int _lcbref;
};

static struct event_base *EV_Base;

#define TIMER_EVENT_NUM 512
static struct timer_event TimerEvents[TIMER_EVENT_NUM];
static int FreeIndexs[TIMER_EVENT_NUM];
static int FreeSize = TIMER_EVENT_NUM;


static void timercb(evutil_socket_t fd, short what, void *arg)
{
    int timer_index = (intptr_t)arg;
    assert(timer_index >= 0 && timer_index < TIMER_EVENT_NUM);
    int lcb_ref = TimerEvents[timer_index]._lcbref;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lcb_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, lcb_ref);
    assert(lua_isfunction(L, -1));
    if (lua_pcall(L, 0, 1, 0) != 0) {
        logging("timer call lua back error : %s", lua_tostring(L, -1));
    } else {
        if (lua_toboolean(L, -1)) {
            FreeIndexs[FreeSize++] = timer_index;
            assert(FreeSize <= TIMER_EVENT_NUM);
        }
    }
}

//seconds number
//callback luafunction
static int ltimer_register(lua_State *L)
{
    if (FreeSize < 1) {
        return luaL_error(L, "create timer error : toooo many timers");
    }
    int alloc_index = FreeIndexs[--FreeSize];
    struct event *ev = &TimerEvents[alloc_index]._ev;

    double seconds = luaL_checknumber(L, 1);
    if (lua_type(L, 2) != LUA_TFUNCTION) {
        luaL_argerror(L, 2, "function expected");
        return 0;
    }
    lua_pushvalue(L, 2);
    TimerEvents[alloc_index]._lcbref = luaL_ref(L, LUA_REGISTRYINDEX);

    struct timeval time_out;
    time_out.tv_sec  = (int)(seconds);
    time_out.tv_usec = (int)((seconds - time_out.tv_sec) * 1000000);
    evtimer_assign(ev, EV_Base, timercb, (void *)(intptr_t)alloc_index);
    evtimer_add(ev, &time_out);
    lua_pushnumber(L, alloc_index);
    return 1;
}

//timer_id int
static int ltimer_remove(lua_State *L)
{
    int timer_index = luaL_checkint(L, 1);
    assert(timer_index >= 0 && timer_index < TIMER_EVENT_NUM);
    FreeIndexs[FreeSize++] = timer_index;
    assert(FreeSize <= TIMER_EVENT_NUM);

    int lcb_ref = TimerEvents[timer_index]._lcbref;
    luaL_unref(L, LUA_REGISTRYINDEX, lcb_ref);
    return 0;
}

int init_ltimer(lua_State *L, void *arg)
{
    int i;
    for (i = 0; i < FreeSize; i++) {
        FreeIndexs[i] = i;
    }
    EV_Base = (struct event_base *)arg;
    lua_register(L, "timer_register", ltimer_register);
    lua_register(L, "timer_remove", ltimer_remove);
    return 0;
}



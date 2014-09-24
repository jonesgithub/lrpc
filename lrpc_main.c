#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event_compat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "lrpc_main.h"

#define LISTEN_BACKLOG 64

#define MAX_IP_LEN 128

typedef unsigned short packet_header_t;
#define PACKET_HEADER_SZ sizeof(packet_header_t)
#define MAX_PACKET_SZ 0xffff


#define ERR_EXIT(fmt, args...) do { \
    fprintf(stderr, fmt, ##args);   \
    exit(-1);                        \
} while(0)

lua_State *L;
static struct event_base *EV_Base;

static int UnpackHandlerRef;

struct channel
{
    char _peer_ip[MAX_IP_LEN]; 
    struct bufferevent *_bev;
};

static struct channel *AllChannels;
static int CurClientIndex;
static int CurServerIndex;
static int MaxClients;
static int MaxRemoteSvrs;

static int init_channel(lua_State *L)
{
    MaxClients = config_optint(L, "max_clients", 1024);
    MaxRemoteSvrs = config_optint(L, "max_remote_servers", 10);

    size_t sz = sizeof(struct channel) * (MaxClients + MaxRemoteSvrs);
    AllChannels = (struct channel *)malloc(sz);
    memset(AllChannels, 0, sz);
    CurClientIndex = MaxRemoteSvrs;
    CurServerIndex = 0;
    return AllChannels ? 0 : -1;
}

static int alloc_client()
{
    if (!AllChannels) return -1;
    int j, i;
    for (j = 0; j < 2; j++) {
        for (i = CurClientIndex; i < MaxClients + MaxRemoteSvrs; i++) {
            if (AllChannels[i]._bev == NULL) {
                CurClientIndex = i + 1;
                return i;
            }
        }
        if (CurClientIndex >= MaxClients + MaxRemoteSvrs)
            CurClientIndex = MaxRemoteSvrs;
    }
    return -1;
}

static int alloc_server()
{
    if (!AllChannels) return -1;
    if (CurServerIndex >= MaxRemoteSvrs) return -1;
    return CurServerIndex++;
}

static int start_tcp_listen(const char *ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return -1;

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(ip);
    srv_addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
        return -1;
    if (listen(fd, LISTEN_BACKLOG) == -1)
        return -1;
    return fd;
}

static int connect_tcp_server(const char *ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return -1;

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = inet_addr(ip);
    srv_addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) == -1)
        return -1;
    return fd;
}

static int parse_addr(const char *addr, char *ip, int *pport)
{
    if (!addr) return -1;
    char *sep = strchr(addr, ':');
    if (!sep) return -1;
    size_t sz = sep - addr;
    if (sz >= MAX_IP_LEN) return -1;
    memcpy(ip, addr, sz);
    ip[sz] = '\0';

    char *endptr;
    *pport = strtol(sep + 1, &endptr, 10);
    if (endptr == sep + 1)
        return -1;
    return 0;
}

static void readcb(struct bufferevent *bev, void *ctx)
{
    int id = (intptr_t)ctx;
    struct evbuffer *input = bufferevent_get_input(bev);
    lua_rawgeti(L, LUA_REGISTRYINDEX, UnpackHandlerRef);
    assert(lua_isfunction(L, -1));
    while (1) {
        lua_pushvalue(L, -1);
        size_t len = evbuffer_get_length(input);
        if (len < PACKET_HEADER_SZ)
            break;
        len -= PACKET_HEADER_SZ;
        size_t packet_len = *(packet_header_t *)evbuffer_pullup(input, PACKET_HEADER_SZ);
        if (len < packet_len)
            break;
        uint8_t *packet = evbuffer_pullup(input, PACKET_HEADER_SZ + packet_len);
        packet += PACKET_HEADER_SZ;
        lua_pushnumber(L, id);
        lua_pushlightuserdata(L, packet);
        lua_pushnumber(L, packet_len);
        if (lua_pcall(L, 3, 2, 0) != 0) {
            logging("call unpack handler error : %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        } else {
            if (lua_toboolean(L, -2) == 0) {
                logging("unpack handler goes wrong : %s", lua_tostring(L, -1));
            }
            lua_pop(L, 2);
        }
        evbuffer_drain(input, PACKET_HEADER_SZ + packet_len);
    }
    lua_pop(L, 1);
}

static void errorcb(struct bufferevent *bev, short error, void *arg)
{
    int id = (intptr_t)arg;
    if (error & BEV_EVENT_EOF) {
        /* connection has been closed, do any clean up here */
        logging("channel %s closed !!!", AllChannels[id]._peer_ip);
    } else if (error & BEV_EVENT_ERROR) {
        /* check errno to see what error occurred */
        logging("channel %s goes error : %s !!!", AllChannels[id]._peer_ip, strerror(errno));
    } else
        logging("channel %s goes error : %s !!!", AllChannels[id]._peer_ip, strerror(errno));
    AllChannels[id]._bev = NULL;
    bufferevent_free(bev);
}


static void new_client(evutil_socket_t lsn_fd, short event, void *arg)
{
    struct event_base *ev_base = (struct event_base *)arg;
    struct sockaddr_in clt_addr;
    socklen_t addr_len = sizeof(clt_addr);
    int fd = accept(lsn_fd, (struct sockaddr*)&clt_addr, &addr_len);
    if (fd == -1) {
        logging("accept new client error %s", strerror(errno));
        return;
    }
    int id = alloc_client();
    if (id == -1) {
        logging("clients is already full");
        return;
    }
    strcpy(AllChannels[id]._peer_ip, inet_ntoa(clt_addr.sin_addr));

    evutil_make_socket_nonblocking(fd);
    AllChannels[id]._bev = bufferevent_socket_new(ev_base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(AllChannels[id]._bev, readcb, NULL, errorcb, (void *)(intptr_t)id);
    bufferevent_enable(AllChannels[id]._bev, EV_READ|EV_WRITE);
}

static int init_network(lua_State *L, struct event_base *ev_base)
{
    const char *char_name = config_optstring(L, "char_name", NULL);
    if (!char_name)
        ERR_EXIT("config error : not found char name\n");
    if (init_channel(L) == -1)
        ERR_EXIT("init client failed\n");

    const char *srv_addr = config_optstring(L, char_name, NULL);
    if (srv_addr) {
        char ip[MAX_IP_LEN];
        int port;
        if (parse_addr(srv_addr, ip, &port) != 0)
            ERR_EXIT("server addr error\n");
        int fd = start_tcp_listen(ip, port);
        if (fd == -1)
            ERR_EXIT("start server failed : %s\n", strerror(errno));
       struct event *lsn_ev = event_new(ev_base, fd, EV_READ | EV_PERSIST, new_client, ev_base);
        event_add(lsn_ev, NULL);
    }

    const char *remote_svrs = config_optstring(L, "remote_servers", NULL);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "remote_servers");
    if (remote_svrs) {
        size_t sz = strlen(remote_svrs);
        char tmp[sz+1];
        strcpy(tmp, remote_svrs);
        char *p = strtok(tmp, ";");
        while (p) {
            const char *rsvr_addr = config_optstring(L, p, NULL);
            if (!rsvr_addr)
                ERR_EXIT("remote server %s addr not found\n", p);
            char ip[MAX_IP_LEN];
            int port;
            if (parse_addr(rsvr_addr, ip, &port) != 0)
                ERR_EXIT("remote server %s addr  error\n", p);
            int fd = connect_tcp_server(ip, port);
            if (fd == -1)
                ERR_EXIT("connect to remote server %s failed : %s\n", p, strerror(errno));
            int id = alloc_server();
            if (id == -1)
                ERR_EXIT("remote server is full\n");
            strcpy(AllChannels[id]._peer_ip, ip);
            lua_pushnumber(L, id);
            lua_setfield(L, -2, p);

            evutil_make_socket_nonblocking(fd);
            AllChannels[id]._bev = bufferevent_socket_new(ev_base, fd, BEV_OPT_CLOSE_ON_FREE);
            bufferevent_setcb(AllChannels[id]._bev, readcb, NULL, errorcb, (void *)(intptr_t)id);
            bufferevent_enable(AllChannels[id]._bev, EV_READ|EV_WRITE);

            p = strtok(NULL, ";");
        }
    }
    lua_pop(L, 1);
    return 0;
}

//channel_id number
//packet lstring
static int lpacket_sender(lua_State *L)
{
    int channel_id = luaL_checkint(L, 1);
    struct bufferevent *bev = AllChannels[channel_id]._bev;
    if (!bev) {
        logging("lpacket_sender error : channel has closed");
        lua_pushboolean(L, 0);
        return 1;
    }
    size_t sz;
    const char *packet = luaL_checklstring(L, 2, &sz);

    if (sz > MAX_PACKET_SZ) {
        logging("packet tooooo big, throw it");
        lua_pushboolean(L, 0);
        return 1;
    }
    packet_header_t packet_len = (packet_header_t)sz;
    if ((bufferevent_write(bev, &packet_len, PACKET_HEADER_SZ) != -1)
            && bufferevent_write(bev, packet, packet_len) != -1) {
        lua_pushboolean(L, 1);
        return 1;
    }
    logging("write packet error");
    lua_pushboolean(L, 0);
    return 1;
}

//log_msg string
static int llogging(lua_State *L)
{
    const char *log_msg = luaL_checkstring(L, 1);
    _logging("%s", log_msg);
    return 0;
}

static int preload(lua_State *L)
{
    if (luaL_dofile(L, "preload.lua") != 0)
        ERR_EXIT("load preload.lua error : %s\n", lua_tostring(L, -1));
    lua_getglobal(L, "packet_handler");
    assert(lua_isfunction(L, -1));
    UnpackHandlerRef = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_register(L, "packet_sender", lpacket_sender);
    init_ltimer(L, EV_Base);
    lua_register(L, "logging", llogging);
    return 0;
}

static int load_service(lua_State *L)
{
    const char *service_main = config_optstring(L, "service_main", NULL);
    if (!service_main)
        ERR_EXIT("config error : service_main not found\n");
    if (luaL_dofile(L, service_main) != 0)
        ERR_EXIT("load service_main %s error : %s\n", service_main, lua_tostring(L, -1));
    return 0;
}

int main(int argc, char *argv[])
{
    L = luaL_newstate();
    luaL_openlibs(L);

    const char *cfg_file_name = "config.cfg";
    if (argc > 1)
        cfg_file_name = argv[1];
    if (read_config(L, cfg_file_name) != 0) {
        fprintf(stderr, "config read failed %s", lua_tostring(L, -1));
        return -1;
    }

    EV_Base = event_base_new();
    init_network(L, EV_Base);
    preload(L);
    load_service(L);

    event_base_dispatch(EV_Base);
    lua_close(L);
    return 0;
}

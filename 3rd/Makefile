LUAPATH = lua-5.2.3/src
LIBEVENT_PATH = libevent-2.0.21-stable
SPROTO_PATH = sproto
all: 
	-(test -f $(LUAPATH)/liblua.a) || (cd $(LUAPATH) && make linux)
	-(test -f $(LIBEVENT_PATH)/.libs/libevent.a ) || (cd $(LIBEVENT_PATH) && ./configure --disable-shared --disable-openssl && make)
	-(test -f $(SPROTO_PATH)/sproto.so) || (cd $(SPROTO_PATH) && make)

clean:
	-(cd $(LUAPATH) && make clean)
	-(cd $(LIBEVENT_PATH) && make clean)
	-(cd $(SPROTO_PATH) && make clean)

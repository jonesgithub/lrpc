
LUAPATH  = 3rd/lua-5.2.3/src
LUALIB = $(LUAPATH)/liblua.a
LIBEVENT_PATH = 3rd/libevent-2.0.21-stable
LIBEVENT = $(LIBEVENT_PATH)/.libs/libevent.a

INCLUDE_DIR =-I$(LUAPATH) -I$(LIBEVENT_PATH)/include 
CCFLAGS = $(INCLUDE_DIR) -Wall

ifdef DEBUG
export DEBUG
CCFLAGS += -g	
endif

SRC = lrpc_main.c lrpc_util.c
BIN = lrpc


all: $(BIN)

$(LUALIB) $(LIBEVENT): thirdlib

thirdlib:
	make -C ./3rd


$(BIN):$(SRC) $(LUALIB) $(LIBEVENT)
	$(CC) -o $@ $(CCFLAGS) $+  -lm -Wl,-E -ldl

clean:
	-rm -f *.o $(BIN)

cleanall:
	(cd 3rd && make clean)
	-rm -f *.o $(BIN)




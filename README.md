# lrpc

A framework for remote procedure calling in lua . 

# build

    git clone https://github.com/longzhiri/lrpc
	cd lrpc
	make

# run

run examples:

	./lrpc ./examples/echo/server.cfg 	# launch echo server
	./lrpc ./examples/echo/client.cfg	# launch echo client

# using lrpc

1. 定义协议： lrpc 使用 [Sproto](https://github.com/cloudwu/sproto) 做序列化，协议定义需要放在一个文件中，遵循 lua 语法，具体协议定义以 lua 字符串形式出现，格式如下：

		-- gameserver 中实现的协议
		gameserver = {
    		client = [[  # client --> gameserver
        		echo 1 {
            		request {
                		what 0 : string
            		}
            		response {
                		what 0 : string
            		}
        		}
    		]],
    		master = [[ # master --> gameserver
        		echo 1 {
            		request {
                		what 0 : string
            		}
            		response {
                		what 0 : string
            		}
        		}
    		]],
		}

		-- master 中实现的协议
    	master = {
    		gameserver = [[ # gameserver --> master
        		echo 1 {
            		request {
                		what 0 : string
            		}
            		response {
                		what 0 : string
            		}
        		}
    		]],
		}
		
2. 配置文件： 遵循 lua 语法

		char_name= "gameserver"	 -- 角色名
		
		gameserver = "127.0.0.1:9999" -- 该 lrpc 服务器提供 rpc 服务的监听地址
		
		remote_servers = "master;db" -- 将会连接到 master 和 db rpc 服务器
		master = "127.0.0.1:9998" -- master rpc 服务器地址
		db = "127.0.0.1:9997" -- db rpc 服务器地址

		service_main = "./examples/game/gameserver.lua"	-- 协议实现文件

		proto_file = "./examples/game/game.pto"	-- 协议定义文件
		
3. 接口定义：

		-- 定时器接口，使用见test/testtimer.lua
		timer.sleep(seconds)	-- 睡眠 seconds 秒才返回
		timer.call_once(seconds, cb, ...) 
		timer.call_multi(first_hit, frequency, cb, ...)
		timer.remove(id)
		
4. RPC 调用

		-- calling gameserver echo from client
		local result = gameserver.echo({what = 'hello world'})
		
		-- echo function implemented by gameserver 
		client2gameserver.echo = function (args, responsor)
			print("> server recieve", args.what)
			responsor(args)
		end
		

		
		
		

		

	



local function run()
    local ok = gameserver.login({username = "spiderman"..os.time()})

    local frame = 0
    while true do
        frame = frame + 1
        print(string.format("[FRAME %d PLAYING]", frame))
        gameserver.say_in_world({what="Hello!!! I'm spiderman"})
        timer.sleep(2)
    end
end

gameserver2client.other_say = function (args)
    print(string.format("%s say : %s", args.username, args.what))
end

timer.call_once(1, run)

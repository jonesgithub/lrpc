local function test_sleep(msg)
    print(msg)
    for i = 1, 3 do
        print("sleep 3s ...")
        timer.sleep(3)
    end
end

local function test_call_multi(msg, co)
    print(msg)
    local call_times
    return timer.call_multi(1, 2, function (...)
        call_times = call_times or ...
        print(string.format("calling the %d times", call_times))
        call_times = call_times + 1
        print("sleep 2s ...")
        if call_times == 5 then
            coroutine.resume(co)
        elseif call_times == 10 then
            coroutine.resume(co)
        end
        return true
    end, 1)
end

local function test_main(msg)
    print(msg)
    local co = coroutine.running()
    local id = test_call_multi("testing call multi", co)
    coroutine.yield()
    test_sleep("testing sleep")
    coroutine.yield()
    print("remove multi timer")
    timer.remove(id)
    print("test over !!!")
end

timer.call_once(1, test_main, "testing start")



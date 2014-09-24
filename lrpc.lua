
local co_pool = {}
function co_create(f)
    if #co_pool == 0 then
        return coroutine.create(function (...)
            local f = f
            f(...)
            while true do
                local co = coroutine.running()
                table.insert(co_pool, co)
                f = coroutine.yield()
                f(coroutine.yield())
            end
        end)
    else
        local co = co_pool[#co_pool]
        table.remove(co_pool)
        coroutine.resume(co, f)
        return co
    end
end

local function err_handler(msg)
    print(debug.traceback())
    return false,msg
end


timer = {}
local cur_timerid = 0
local alltimers = {}
function timer.sleep(seconds)
    local co = coroutine.running()
    local function timer_cb()
        coroutine.resume(co)
    end

    timer_register(seconds, timer_cb)
    coroutine.yield()
end

function timer.call_once(seconds, cb, ...)
    local args = {...}
    local id, ref

    local co = co_create(function ()
        xpcall(cb, err_handler, unpack(args))
    end)
    local function timer_cb()
        alltimers[id] = nil
        coroutine.resume(co)
    end

    ref = timer_register(seconds, timer_cb)
    cur_timerid = cur_timerid + 1
    id = cur_timerid
    alltimers[id] = ref
 
    return id
end

function timer.call_multi(first_hit, frequency, cb, ...)
    local args = {...}
    local id, ref

    local co = co_create(function ()
        while true do
            if not alltimers[id] then return end
            local next_time = os.time() + frequency
            if not xpcall(cb, err_handler, unpack(args)) then break end
            local time_sleep = next_time - os.time()
            if time_sleep < 0 then -- miss one or more callback
                time_sleep = frequency
            end
            timer.sleep(time_sleep)
        end
        alltimers[id] = nil
    end)
    local function timer_cb()
        alltimers[id] = true
        coroutine.resume(co)
    end

    ref = timer_register(first_hit, timer_cb)
    cur_timerid = cur_timerid + 1
    id = cur_timerid
    alltimers[id] = ref

    return id
end

function timer.remove(id)
    if alltimers[id] then
        local ref = alltimers[id]
        alltimers[id] = nil
        if ref ~= true then
            timer_remove(ref)
        end
        return true
    end
    return false
end

function serialize_table (t, level)
    local parts_tbl = {"{"}
    level = level or 0
    local prefix = string.rep("\t", level + 1)
    for k, v in pairs(t) do
        local tbl_str
        if type(k) == "string" then
            tbl_str = prefix .. string.format("[%q] = ", k)
        elseif (type(k) == "number") then
            tbl_str = prefix .. string.format("[%d] = ", k)
        else
            tbl_str = prefix .. string.format("[%s] = ", type(k))
        end
        
        if type(v) == "string" then
            tbl_str = tbl_str .. string.format("%q,", v)
        elseif type(v) == "number" then
            tbl_str = tbl_str .. string.format("%d,", v)
        elseif type(v) == "table" then
            tbl_str = tbl_str .. serialize_table(v, level + 1) .. ","
        else
            tbl_str = tbl_str .. string.format("%s,", type(v))
        end
        table.insert(parts_tbl, tbl_str)
    end
    table.insert(parts_tbl, string.rep("\t", level) .. "}")
    return table.concat(parts_tbl, '\n')
end



package.path = "./3rd/sproto/?.lua;./?.lua;;"
package.cpath = "./3rd/sproto/?.so;;"

local core = require "sproto.core"
local parser = require "sprotoparser"
require "lrpc"

local assert = assert
local config_tbl = assert(config_table, "config table not defined")

local header_ptext = [[
.package {
    type 0 : integer
    session 1 : integer
}
]]
local header_tbl = {type = nil, session = nil}
local header_proto, header_type

local weak_mt = {__mode = "kv"}

local sproto = {}
function sproto:new(pbin)
    self.__mt = self.__mt or {__index = self}
	local cobj = assert(core.newproto(pbin))
	local obj = {
		__cobj = cobj,
		__tcache = setmetatable( {} , weak_mt ),
		__pcache = setmetatable( {} , weak_mt ),
	}
	return setmetatable(obj, self.__mt)
end

function sproto:querytype(typename)
    local v = self.__tcache[typename]
    if not v then
        v = core.querytype(self.__cobj, typename)
        self.__tcache[typename] = v
    end
    return v
end

function sproto:queryproto(protoname)
    local v = self.__pcache[protoname]
    if not v then
        local tag, req, resp = core.protocol(self.__cobj, protoname)
        assert(tag, protoname .. " not found")
        if tonumber(protoname) then
            protoname, tag = tag, protoname
        end
        v = {
            request = req,
            response = resp,
            name = protoname,
            tag = tag
        }
        self.__pcache[protoname] = v
        self.__pcache[tag] = v
    end
    return v
end

local function doparsing(ptext)
    return sproto:new(parser.parse(ptext))
end

local cur_session = 0
local function alloc_session()
    cur_session = cur_session + 1
    return cur_session
end

local allsessions = {}
local allmakers = {}
local my_char = assert(config_tbl.char_name)
function packet_handler(channel_id, packet, size)
    local bin = core.unpack(packet, size)
    header_tbl.type = nil
    header_tbl.session = nil
    local header, size = core.decode(header_type, bin, header_tbl)
    local content = bin:sub(size + 1)
    local session = header.session
    if header.type then -- is a request packet
        return allmakers[channel_id]:dispatch(channel_id, header.type, session, content)
    else -- is a response packet
        local s = assert(allsessions[session], "a session not found")
        allsessions[session] = nil
        local result = core.decode(s.response, content)
        return coroutine.resume(s.co, result)
    end
end

local function make_maker(peer_char_name, ptext)
    local maker_obj = {}
    maker_obj.__pobj = doparsing(ptext)
    local maker_name = peer_char_name .. "2" .. my_char
    _G[maker_name] = {}
    local funcs = _G[maker_name]

    maker_obj.dispatch = function (self, channel_id, prototag, session, content)
        local pobj = self.__pobj
        local proto = pobj:queryproto(prototag)
        if not proto then return end
        local result
        if proto and proto.request then
            result = core.decode(proto.request, content)
        end
        if not funcs[proto.name] then
            logging(proto.name, "function not defined!!!")
            return
        end

        local responsor 

        if session then
            responsor = function (args)
                header_tbl.type = nil
                header_tbl.session = session
                local header = core.encode(header_type, header_tbl)
                if proto.response then
                    local content = core.encode(proto.response, args)
                    packet_sender(channel_id, core.pack(header .. content))
                else
                    packet_sender(channel_id, core.pack(header))
                end
            end
        end

        local co = co_create(function ()
            local function err_handler(msg)
                logging(debug.traceback(msg))
                return false, msg
            end
            xpcall(funcs[proto.name], err_handler, result, responsor, channel_id)
        end)
        return coroutine.resume(co)
    end
    return maker_obj
end

local function make_caller(peer_char_name, ptext)
    local caller_obj = {}
    caller_obj.__pobj = doparsing(ptext)
    local def_channel = remote_servers[peer_char_name]
    _G[peer_char_name] = caller_obj
    setmetatable(caller_obj, {__index = function (self, k)
        local p = self.__pobj:queryproto(k)
        if not p then return end
        return function (args, channel)
            header_tbl.type = p.tag
            if p.response then
                header_tbl.session = alloc_session()
                allsessions[header_tbl.session] = {
                    co = coroutine.running(),
                    response = p.response
                }
            else
                header_tbl.session = nil
            end
            local header = core.encode(header_type, header_tbl)
            channel = channel or def_channel
            assert(channel, "no channel specified")
            if args then
                local content = core.encode(p.request, args)
                packet_sender(channel, core.pack(header .. content))
            else
                packet_sender(channel, core.pack(header))
            end
            if p.response then
                return coroutine.yield()
            else
                return true
            end
        end
    end})
    return caller_obj
end


function load_protocol()
    header_proto = doparsing(header_ptext)
    header_type = header_proto:querytype("package")

    local all_ptexts = {}
    assert(loadfile(config_tbl.proto_file or "protocol.pto", "t", all_ptexts))()

    local clt_proto
    for from, ptext in pairs(all_ptexts[my_char] or {}) do
        local channel_id = remote_servers[from]
        if channel_id then
            allmakers[channel_id] = make_maker(from, ptext)
        else
            clt_proto = make_maker(from, ptext)
        end
    end
    setmetatable(allmakers, {__index = function (self, k)
        if type(k) == "number" and k > 0 then
            return clt_proto
        end
    end})

    all_ptexts[my_char] = nil

    for char_name, tbl in pairs(all_ptexts) do
        for from, ptext in pairs(tbl) do
            if from == my_char then
                make_caller(char_name, ptext)
            end
        end
    end

end
load_protocol()


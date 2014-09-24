
local Uid2Channel = {}
local Channel2Uid = {}
local Uid2Name = {}
client2gameserver.login = function (args, responsor, channel)
    local result = master.user_login(args)
    if result.id then
        Uid2Channel[result.id] = channel
        Uid2Name[result.id] = args.username
        Channel2Uid[channel] = result.id
        responsor({ok = true})
    else
        responsor({ok = false})
    end
end

client2gameserver.say_in_world = function (args, responsor, channel)
    local username = Uid2Name[Channel2Uid[channel]]
    for uid, c in pairs(Uid2Channel) do
        if c ~= channel then
            client.other_say({username = username, what = args.what}, c)
        end
    end
    master.say_in_world({username = username, what = args.what})
end

master2gameserver.user_say = function (args, responsor)
    for uid, c in pairs(Uid2Channel) do
        client.other_say(args, c)
    end
end

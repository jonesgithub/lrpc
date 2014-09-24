local Uid2Channel = {}
local Uid2Name = {}
local Channel = {}
gameserver2master.user_login = function (args, responsor, channel)
    local result = db.create_user(args)
    if result.id then
        Uid2Channel[result.id] = channel
        Uid2Name[result.id] = args.username
    end
    responsor(result)
    Channel[channel] = true
end

gameserver2master.say_in_world = function (args, responsor, channel)
    for c, _ in pairs(Channel) do
        if c ~= channel then
            gameserver.user_say(args, c) 
        end
    end
end

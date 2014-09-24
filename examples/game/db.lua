local Name2Uid = {}
local CurUid = 0

master2db.create_user = function (args, responsor, channel)
    if not Name2Uid[args.username] then
        Name2Uid[args.username] = CurUid
        CurUid = CurUid + 1
    end
    responsor({id=Name2Uid[args.username]})
end

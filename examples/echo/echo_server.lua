client2server.echo = function (args, responsor)
    print("> server recieve ", args.what)
    responsor(args)
end


local co = co_create(function ()
    local result = server.echo({what = "Hello World!"})
    print("> client received ", result.what)
end)

coroutine.resume(co)

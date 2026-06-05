local nu = require("lnuperf")
local words = {"ant", "bee", "cat", "dog", "eel"}

local ks = nu.Keyset.new()
for _, word in ipairs(words) do
  ks:add_string(word)
end

local t = nu.Table.new()
t:set_method("stdmeth")
t:set_target("stddef")
t:build(ks)
print(t:emit_buffer())

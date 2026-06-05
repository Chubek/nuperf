local nu = require("lnuperf")

local ks = nu.Keyset.new()
for _, key in ipairs({"spring", "summer", "autumn", "winter"}) do
  ks:add_string(key)
end

local t = nu.Table.new()
t:set_method("stdmeth")
t:set_target("stddef")
t:set_option("array_name", "seasons_table")
t:set_option("header_guard", "true")
t:set_option("guard_prefix", "SEASONS_MPH")
t:build(ks)
print(t:emit_buffer())

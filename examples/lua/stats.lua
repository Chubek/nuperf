local nu = require("lnuperf")

local ks = nu.Keyset.new()
ks:add_string("alpha")
ks:add_string("beta")
ks:add_string("gamma")

local t = nu.Table.new()
t:set_method("stdmeth")
t:set_target("stddef")
t:build(ks)

local s = t:stats()
print(string.format("keys=%d bytes=%d us=%d bits_per_key=%.6f", s.key_count, s.table_size_bytes, s.build_time_us, s.bits_per_key))

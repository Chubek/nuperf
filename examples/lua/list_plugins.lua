local nu = require("lnuperf")

for i, name in ipairs(nu.methods()) do
  print("method", i, name)
end

for i, name in ipairs(nu.targets()) do
  print("target", i, name)
end

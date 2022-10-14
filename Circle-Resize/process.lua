local tasks = ...

local ffi = require('ffi')
ffi.cdef[[
void export_image(
  const char *in_path, const char *out_path,
  double cx, double cy, double cr);
]]

local tryload = function (paths)
  for i = 1, #paths do
    local mod
    if pcall(function () mod = ffi.load(paths[i]) end) then
      return mod
    end
  end
  return nil
end

local s = love.filesystem.getSource()
local lastslash = s:match('^.*()[/\\]')
local strimmed = s
if lastslash ~= nil then
  strimmed = s:sub(1, lastslash - 1)
end
print(s, strimmed)
local exportmod = tryload({
  s .. '/libexportmod.so',
  s .. '/libexportmod.dylib',
  s .. '\\exportmod.dll',
  strimmed .. '/libexportmod.so',
  strimmed .. '/libexportmod.dylib',
  strimmed .. '\\exportmod.dll',
})

for i = 1, #tasks do
  local name, p1, p2, cx, cy, cr = unpack(tasks[i])
  love.thread.getChannel('process_progress'):push({
    {0.6, 0.6, 0.6}, string.format('%d/%d ', i, #tasks),
    {0.1, 0.1, 0.1}, name,
  })
  exportmod.export_image(p1, p2, cx, cy, cr)
end
love.thread.getChannel('process_progress'):push({
  {0.4, 0.6, 0.3}, 'Finished'
})

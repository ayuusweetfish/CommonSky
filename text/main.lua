-- part=1|2|3 record=1

local W = 960
local H = 600

local scale = 1
love.window.setMode(
  W * scale, H * scale,
  { fullscreen = false, highdpi = false }
)

local totaltime = 8 * 240

local font_zh = love.graphics.newFont('zh.ttf', 48)
local font_en_lg = love.graphics.newFont('en.ttf', 48)
local font_en_sm = love.graphics.newFont('en.ttf', 32)
local font_en_smlw = love.graphics.newFont('en.ttf', 24)

local part = tonumber(os.getenv('part')) or 1

local texts = {
  {  'I.', '瑶镜', 'Disque de Jade'},
  { 'II.', '虹桥', 'Arco Iris'},
  {'III.', '星幕', 'Domus Astris'},
}

local utf8 = require('utf8')

local t1 = love.graphics.newText(font_en_lg, texts[part][1])
local t2 = {}
for _, c in utf8.codes(texts[part][2]) do
  t2[#t2 + 1] = love.graphics.newText(font_zh, utf8.char(c))
end
local t3 = {}
for _, c in utf8.codes(texts[part][3]) do
  local ch = utf8.char(c)
  local font = font_en_sm
  local letterspacing = 3
  local baseline = 0
  if c >= 97 and c <= 122 then
    ch = utf8.char(c - 32)
    font = font_en_smlw
    baseline = -1
  end
  if c == 32 then
    letterspacing = 12
  end
  t3[#t3 + 1] = {love.graphics.newText(font, ch), letterspacing, baseline}
end

local dslvshadersrc = love.filesystem.read('dissolve.frag')
local dslvshader = love.graphics.newShader(dslvshadersrc)

local T = 0
local update = function ()
  T = T + 1
end

local cumtime = 0
love.update = function (dt)
  cumtime = cumtime + dt
  while cumtime > 0 do
    cumtime = cumtime - 1/240
    update()
  end
  if T >= totaltime then love.event.quit() end
end

local drawtext = function (t, x, y, ax, ay)
  local w, h = t:getDimensions()
  love.graphics.draw(t, x, y, 0, 1, 1, ax * w, ay * h)
end

local draw = function ()
  love.graphics.push()
  love.graphics.scale(scale)

  love.graphics.clear(0.05, 0.05, 0.05)

  love.graphics.setShader(dslvshader)
  dslvshader:send('intime', math.max(0, T / 240 - 1))
  dslvshader:send('outtime', math.max(0, T / 240 - 5))
  love.graphics.setColor(1, 1, 1)
  drawtext(t1, W * 0.41, H * 0.51, 1, 1)
  local t2x = W * 0.45
  for i = 1, #t2 do
    local t = t2[i]
    drawtext(t, t2x, H * 0.51, 0, 1)
    t2x = t2x + t:getWidth() + 5
  end
  local t3x = W * 0.45
  for i = 1, #t3 do
    local t, letterspacing, baseline = unpack(t3[i])
    drawtext(t, t3x, H * 0.59 + baseline, 0, 1)
    t3x = t3x + t:getWidth() + letterspacing
  end
  love.graphics.setShader(nil)

  love.graphics.pop()
end

if os.getenv('record') ~= nil then
  love.update = function () end
  local frame = 0
  love.draw = function ()
    update()
    if T % 8 == 0 then
      draw()
      frame = frame + 1
      local name = string.format('text-%d-%06d.png', part, frame)
      print(name)
      love.graphics.captureScreenshot(name)
    end
    if T >= totaltime then
      love.event.quit()
    end
  end
else
  love.draw = draw
end

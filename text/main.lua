-- part=1|2|3 record=1

local W = 960
local H = 600

local scale = 1
love.window.setMode(
  W * scale, H * scale,
  { fullscreen = false, highdpi = false }
)

local totaltime

local utf8 = require('utf8')

local dslvshadersrc = love.filesystem.read('dissolve.frag')
local dslvshader = love.graphics.newShader(dslvshadersrc)

local T = 0

local part = tonumber(os.getenv('part')) or 1
local drawpart

dslvshader:send('seed', part)

local processHiero = function (s, size, spacing)
  spacing = spacing or 0.1
  local font = love.graphics.newFont('zh.ttf', size)
  local t = {}
  for _, c in utf8.codes(s) do
    local text = love.graphics.newText(font, utf8.char(c))
    t[#t + 1] = {text, text:getWidth() * spacing, 0}
  end
  return t
end
local processLatin = function (s, uppersize, spacingrate)
  lowersize = uppersize * 0.75
  spacingrate = spacingrate or 1
  local font_up = love.graphics.newFont('en.ttf', uppersize)
  local font_lw = love.graphics.newFont('en.ttf', lowersize)
  local t = {}
  for _, c in utf8.codes(s) do
    local ch = utf8.char(c)
    local font = font_up
    local letterspacing = 3 * (uppersize/32)
    local baseline = 0
    if c >= 97 and c <= 122 then
      ch = utf8.char(c - 32)
      font = font_lw
      baseline = (font_up:getBaseline() - font_lw:getBaseline()) -
        (font_up:getHeight() - font_lw:getHeight())
    end
    if c == 32 then
      letterspacing = 12 * (uppersize/32)
    end
    t[#t + 1] = {love.graphics.newText(font, ch), letterspacing * spacingrate, baseline}
  end
  return t
end
local drawchar = function (t, x, y, ax, ay)
  local w, h = t:getDimensions()
  love.graphics.draw(t, x, y, 0, 1, 1, ax * w, ay * h)
end
local drawtext = function (t, x, y, ax, ay)
  local w = 0
  for i = 1, #t do
    local text, letterspacing, baseline = unpack(t[i])
    w = w + text:getWidth() + (i == #t - 1 and 0 or letterspacing)
  end
  x = x - ax * w
  for i = 1, #t do
    local text, letterspacing, baseline = unpack(t[i])
    drawchar(text, x, y + baseline, 0, ay)
    x = x + text:getWidth() + letterspacing
  end
end

if part <= 3 then
  local texts = {
    {  'I.', '瑶镜', 'Disque de Jade'},
    { 'II.', '虹桥', 'Arco Iris'},
    {'III.', '星幕', 'Domus Astris'},
  }

  local font_en_lg = love.graphics.newFont('en.ttf', 48)
  local t1 = love.graphics.newText(font_en_lg, texts[part][1])
  local t2 = processHiero(texts[part][2], 48, 0.12)
  local t3 = processLatin(texts[part][3], 32)

  totaltime = 8 * 240
  drawpart = function ()
    love.graphics.setShader(dslvshader)
    dslvshader:send('intime', math.max(0, T / 240 - 1.5))
    dslvshader:send('outtime', math.max(0, T / 240 - 5.5))
    love.graphics.setColor(1, 1, 1)
    drawchar(t1, W * 0.41, H * 0.51, 1, 1)
    drawtext(t2, W * 0.45, H * 0.51, 0, 1)
    drawtext(t3, W * 0.45, H * 0.59, 0, 1)
  end
elseif part == 4 then
  local casttexts = {
    {'策划与制作', 'Directed & Implemented by', '', ''},
    {'致谢', 'With Special Thanks to', '', ''},
    {'', '', '', ''},
  }
  for i = 1, #casttexts do
    casttexts[i] = {
      processHiero(casttexts[i][1], 32),
      processLatin(casttexts[i][2], 28, 0.5),
      processHiero(casttexts[i][3], 32),
      processLatin(casttexts[i][4], 28, 0.5),
    }
  end

  local datetext = processLatin('2022.10', 24, 0)

  totaltime = 10 * 240
  drawpart = function ()
    love.graphics.setShader(dslvshader)
    dslvshader:send('intime', math.max(0, T / 240 - 0.5))
    dslvshader:send('outtime', math.max(0, T / 240 - 8))
    for i = 1, #casttexts do
      drawtext(casttexts[i][1], W * 0.15, H * (0.10 + i * 0.2), 0, 1)
      drawtext(casttexts[i][2], W * 0.15, H * (0.16 + i * 0.2), 0, 1)
      drawtext(casttexts[i][3], W * 0.85, H * (0.10 + i * 0.2 - math.floor((i - 1) / 2) * 0.04), 1, 1)
      drawtext(casttexts[i][4], W * 0.85, H * (0.16 + i * 0.2 - math.floor((i - 1) / 2) * 0.04), 1, 1)
    end
    drawtext(datetext, W * 0.5, H * 0.82, 0.5, 1)
  end
elseif part == 5 then
  local font = love.graphics.newFont('zh.ttf', 32)
  local s = '这一部分尚未完成，设想的形式是在互联网上收集月亮的照片进行影像拼贴。' ..
    '月亮会按照月相排列，同时背景的天空颜色也会构成渐变。'
  local _, lines = font:getWrap(s, W * 0.7)
  local desctext = {}
  for i = 1, #lines do
    desctext[#desctext + 1] = love.graphics.newText(font, lines[i])
  end

  totaltime = 15 * 240
  drawpart = function ()
    love.graphics.setShader(dslvshader)
    dslvshader:send('intime', math.max(0, T / 240 - 0.5))
    dslvshader:send('outtime', math.max(0, T / 240 - 13))
    for i = 1, #desctext do
      drawchar(desctext[i], W * 0.5,
        H * (0.49 + (i - (#desctext + 1) / 2) * 0.08),
        0.5, 0.5)
    end
  end
end

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

local draw = function ()
  love.graphics.push()
  love.graphics.scale(scale)

  love.graphics.setShader(nil)
  love.graphics.clear(0.05, 0.05, 0.05)

  drawpart()

  love.graphics.pop()
end

if os.getenv('record') ~= nil then
  love.filesystem.setIdentity('text-cutscene')
  love.update = function () end
  local frame = 0
  love.draw = function ()
    update()
    if T % 4 == 0 then
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

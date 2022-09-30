local W = 1080
local H = 720

love.window.setMode(
  W, H,
  { fullscreen = false, highdpi = false }
)
local SC = love.graphics.getDPIScale()

require './utils'

local imgpath = '../img'
local annotpath = imgpath .. '/annot.txt'

local loadimage = function (name)
  path = imgpath .. '/' .. name
  local contents = io.open(path, 'r'):read('*a')
  local img = love.graphics.newImage(love.data.newByteData(contents))
  return img
end

local arcox, arcoy = W * 0.5, H * 0.72
local arcor = W * 0.4

local annot, annotlist = loadannots(annotpath)
local imgs = {}
for i = 1, #annotlist do
  local img = loadimage(annotlist[i], { mipmap = true })
  img:setFilter('linear', 'linear')
  local iw, ih = img:getDimensions()
  local a = annot[annotlist[i]]
  local cx, cy, cr = fitcircle(a)
  if cx ~= nil then
    local isc = arcor / cr
    local ioffx = -cx * isc
    local ioffy = -cy * isc
    local min, max = math.pi, 0
    for j = 1, #a do
      local angle = math.atan2(-(a[j][2] - cy), a[j][1] - cx)
      angle = (angle + math.pi/2) % (math.pi*2) - math.pi/2
      min = math.min(min, angle)
      max = math.max(max, angle)
    end
    min = math.max(min, 0)
    max = math.min(max, math.pi)
    local ianglecen = (min + max) / 2
    local ianglespan = (max - min) / 2
    imgs[i] = {img, iw, ih, isc, ioffx, ioffy, ianglecen, ianglespan}
  end
end

-- Sort by descending size
table.sort(imgs, function (a, b)
  return a[2] * a[3] * a[4] * a[4] > b[2] * b[3] * b[4] * b[4]
end)

local T = 0

local update = function ()
  T = T + 1
end

local cumtime = 0
function love.update(dt)
  cumtime = cumtime + dt
  while cumtime > 0 do
    cumtime = cumtime - 1/240
    update()
  end
end

-- Separated since love.filesystem.read returns contents and length
local imgshadersrc = love.filesystem.read('img.frag')
local imgshader = love.graphics.newShader(imgshadersrc)
local uppershadersrc = love.filesystem.read('upper.frag')
local uppershader = love.graphics.newShader(uppershadersrc)

local canvasfrozen = love.graphics.newCanvas()
local lastfrozen = 0

local canvasupper = love.graphics.newCanvas()

local freezeafter = 12
local enterinterval = 0.4

local totaltime = 240 * (enterinterval * #imgs + freezeafter)

local draw = function ()
  love.graphics.clear(0.1, 0.1, 0.15)
  love.graphics.setColor(1, 1, 1)
  love.graphics.setBlendMode('alpha')

  love.graphics.setCanvas(canvasupper)
  love.graphics.draw(canvasfrozen, 0, 0)
  love.graphics.setShader(imgshader)
  imgshader:send('arcopos', {arcox * SC, arcoy * SC})
  imgshader:send('arcor', arcor * SC)
  local drawimg = function (i)
    local item = imgs[i]
    local img, iw, ih, isc, ioffx, ioffy, ianglecen, ianglespan = unpack(item)
    imgshader:send('tex_dims', {iw * isc, ih * isc})
    imgshader:send('seed', {i * 2000, i * 4000})
    imgshader:send('time', T / 240 - i * enterinterval)
    imgshader:send('angle_cen', -ianglecen)
    imgshader:send('angle_span', ianglespan)
    love.graphics.draw(img,
      arcox + ioffx,
      arcoy + ioffy,
      0, isc)
  end
  for i = lastfrozen + 1,
    math.min(#imgs, lastfrozen + freezeafter / enterinterval + 1)
  do
    drawimg(i)
  end
  local frozen = math.floor((T / 240 - freezeafter) / enterinterval)
  if frozen > lastfrozen and frozen <= #imgs then
    love.graphics.setCanvas(canvasfrozen)
    drawimg(frozen)
    lastfrozen = frozen
  end
  love.graphics.setBlendMode('alpha', 'premultiplied')
  love.graphics.setCanvas(nil)
  love.graphics.setShader(nil)
  love.graphics.draw(canvasupper, 0, 0)
  love.graphics.setShader(uppershader)
  uppershader:send('time', T / 240)
  uppershader:send('texdevrate', math.exp(-(T / totaltime)))
  love.graphics.draw(canvasupper, 0, 0)
end

if os.getenv('record') ~= nil then
  love.update = function () end
  local frame = 0
  love.draw = function ()
    update()
    if T % 8 == 0 then
      draw()
      frame = frame + 1
      local name = string.format('arco-iris-%06d.png', frame)
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

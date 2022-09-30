local W = 1080
local H = 720

love.window.setMode(
  W, H,
  { fullscreen = false, highdpi = true }
)

require './utils'

local imgpath = '../img'
local annotpath = imgpath .. '/annot.txt'

local annot, annotlist = loadannots(annotpath)

local loadimage = function (name)
  path = imgpath .. '/' .. name
  local contents = io.open(path, 'r'):read('*a')
  local img = love.graphics.newImage(love.data.newByteData(contents))
  return img
end

local arcox, arcoy = W * 0.5, H * 0.7
local arcor = W * 0.4

local imgs = {}
for i = 1, #annotlist do
  local img = loadimage(annotlist[i])
  local iw, ih = img:getDimensions()
  local icx, icy, icr = fitcircle(annot[annotlist[i]])
  if icx ~= nil then
    local isc = arcor / icr
    local ioffx = -icx * isc
    local ioffy = -icy * isc
    imgs[i] = {img, iw, ih, isc, ioffx, ioffy}
  end
end

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

local imgshadersrc = love.filesystem.read('img.frag')
local imgshader = love.graphics.newShader(imgshadersrc)

function love.draw()
  love.graphics.clear(0.1, 0.1, 0.15)
  love.graphics.setColor(1, 1, 1)
  local start = math.floor(T / 20)
  love.graphics.setShader(imgshader)
  for i = 1, #imgs do
    local img, iw, ih, isc, ioffx, ioffy = unpack(imgs[(start + i) % #imgs + 1])
    imgshader:send('tex_dims', {iw, ih})
    love.graphics.draw(img,
      arcox + ioffx,
      arcoy + ioffy,
      0, isc)
  end
  love.graphics.setShader(nil)
end

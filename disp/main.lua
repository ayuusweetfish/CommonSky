local W = 1080
local H = 720

love.window.setMode(
  W, H,
  { fullscreen = false, highdpi = true }
)
local SC = love.graphics.getDPIScale()

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
    imgs[i] = {img, iw, ih, isc, ioffx, ioffy, ianglecen}
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
  imgshader:send('arcopos', {arcox * SC, arcoy * SC})
  imgshader:send('arcor', arcor * SC)
  --for i = 1, #imgs do
    --local item = imgs[(start + i) % #imgs + 1]
  for i = 1, 10 do
    local item = imgs[i]
    local img, iw, ih, isc, ioffx, ioffy, ianglecen = unpack(item)
    imgshader:send('tex_dims', {iw * isc, ih * isc})
    imgshader:send('seed', {i * 2000, i * 4000})
    imgshader:send('time', T / 240 - i)
    imgshader:send('cen_angle', -ianglecen)
    love.graphics.draw(img,
      arcox + ioffx,
      arcoy + ioffy,
      0, isc)
  end
  love.graphics.setShader(nil)
end

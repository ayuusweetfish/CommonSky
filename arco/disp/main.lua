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
  local imgdata = love.image.newImageData(love.data.newByteData(contents))
  return imgdata
end

local arcox, arcoy = W * 0.5, H * 0.72
local arcor = W * 0.4

local annot, annotlist = loadannots(annotpath)
local imgs = {}
for i = 1, #annotlist do
  local imgdata = loadimage(annotlist[i], { mipmap = true })
  local iw, ih = imgdata:getDimensions()
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
    imgs[i] = {imgdata, iw, ih, isc, ioffx, ioffy, ianglecen, ianglespan}
  end
end

-- Sort by descending size
table.sort(imgs, function (a, b)
  return a[2] * a[3] * a[4] * a[4] > b[2] * b[3] * b[4] * b[4]
end)

for i = 12, #imgs do
  local img = imgs[i]
  local anglecen = img[7]
  -- Find a place with the largest centre angle difference
  local bestidx, bestval = i, math.abs(anglecen - imgs[i - 1][7]) * 1.2
  for j = i - 10, i - 1 do
    local val = math.min(
      math.abs(anglecen - imgs[j - 1][7]),
      math.abs(anglecen - imgs[j][7]))
    if val > bestval then
      bestidx, bestval = j, val
    end
  end
  print(i, bestidx, bestval * 180 / math.pi)
  if bestidx ~= i then
    for j = i - 1, bestidx, -1 do imgs[j + 1] = imgs[j] end
    imgs[bestidx] = img
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

-- Separated since love.filesystem.read returns contents and length
local skybgshadersrc = love.filesystem.read('skybg.frag')
local skybgshader = love.graphics.newShader(skybgshadersrc)
local imgshadersrc = love.filesystem.read('img.frag')
local imgshader = love.graphics.newShader(imgshadersrc)
local watershadersrc = love.filesystem.read('water.frag')
local watershader = love.graphics.newShader(watershadersrc)

local canvasheadroom = H * 0.4
local canvasfrozen = love.graphics.newCanvas(W, H + canvasheadroom)
local lastfrozen = 0

local canvasupper = love.graphics.newCanvas()

local freezeafter = 5.5
local enterinterval = 0.25

local scenestarttime = 2
local keepmovedowntime = 2.5
local expandtime = enterinterval * #imgs + freezeafter
local moveuptime = 2
local totaltime = 240 * (scenestarttime + expandtime + moveuptime)

local logstats = function ()
  local stats = love.graphics.getStats()
  print('tex mem: ', stats.texturememory)
  print('img: ', stats.images)
end

local gradient = function (horiz, points)
  local verts = {}
  for i = 1, #points - 1, 2 do
    local x = points[i]
    local a = points[i + 1]
    if horiz then
      verts[#verts + 1] = {x, 0, x, 0, 1, 1, 1, a}
      verts[#verts + 1] = {x, 1, x, 1, 1, 1, 1, a}
    else
      verts[#verts + 1] = {0, x, 0, x, 1, 1, 1, a}
      verts[#verts + 1] = {1, x, 1, x, 1, 1, 1, a}
    end
  end
  return love.graphics.newMesh(verts, 'strip', 'static')
end

local skygradient = gradient(false, {
  0.00, 0.0,
  0.66, 0.3,
  0.95, 0.7,
  1.00, 0.0,
})
local skyhgradient = gradient(true, {
  0.00, 0.0,
  0.25, 0.3,
  0.40, 0.6,
  0.60, 0.6,
  0.75, 0.3,
  1.00, 0.0,
})
local watergradient = gradient(false, {
  0.00, 0.0,
  0.25, 0.1,
  1.00, 0.2,
})

local drawarco = function (moveuprate)
  local offy = moveuprate * canvasheadroom
  local fadeprogr = 1 - math.max(0, math.min(1, T / totaltime))
  local faderemap = 0.62 -- 0.5 to 1; the peak of sunset
  fadeprogr = math.max(fadeprogr * 0.5 / faderemap,
    0.5 + 0.5 * (fadeprogr - faderemap) / (1 - faderemap))
  local fade = function (x, a, b, c)
    if x >= 0.5 then
      local x = (1 - math.cos((x - 0.5) * 2 * math.pi)) / 2
      return b + x * (c - b)
    else
      local x = (1 - math.cos(x * 2 * math.pi)) / 2
      return a + x * (b - a)
    end
  end
  local skytint = function (x)
    return
      fade(x, 0.10, 0.90, 0.85) * (1 - moveuprate),
      fade(x, 0.10, 0.70, 0.92) * (1 - moveuprate),
      fade(x, 0.15, 0.55, 1.00) * (1 - moveuprate) + moveuprate * 0.05
  end
  love.graphics.setCanvas(nil)
  love.graphics.setShader(skybgshader)
  skybgshader:send('c', {skytint(math.max(0, fadeprogr))})
  love.graphics.setBlendMode('alpha')
  love.graphics.rectangle('fill', 0, 0, W, H)

  love.graphics.setShader(nil)
  love.graphics.setBlendMode('alpha')
  local skyalpha = math.max(0, math.min(1, (fadeprogr - 0.2) / 0.3))
  local skyorange = math.max(0, math.min(1, (0.8 - fadeprogr) / 0.3))
  local skyr, skyg, skyb = 0.94, 0.96 - skyorange * 0.1, 1.0 - skyorange * 0.5
  love.graphics.setColor(skyr, skyg, skyb, skyalpha * (1 - moveuprate))
  love.graphics.draw(skygradient, 0, offy, 0, W, arcoy * 1.05)
  love.graphics.setColor(skyr, skyg, skyb, skyorange * skyalpha * skyalpha * (1 - moveuprate))
  love.graphics.draw(skyhgradient, 0, offy, 0, W, arcoy)
  love.graphics.setColor(0.03, 0.15, 0.3, math.sqrt((math.cos(fadeprogr * math.pi * 2) + 1) / 2))
  love.graphics.draw(watergradient, 0, offy + arcoy, 0, W, H - arcoy)

  love.graphics.setColor(1, 1, 1)
  love.graphics.setBlendMode('alpha', 'premultiplied')
  love.graphics.setCanvas(canvasupper)
  love.graphics.clear(0, 0, 0, 0)
  love.graphics.setShader(nil)
  love.graphics.draw(canvasfrozen, 0, offy - canvasheadroom)
  love.graphics.setBlendMode('alpha')
  love.graphics.setShader(imgshader)
  imgshader:send('arcopos', {arcox * SC, arcoy * SC})
  imgshader:send('arcor', arcor * SC)
  local drawimg = function (i, freeze)
    local item = imgs[i]
    local img, iw, ih, isc, ioffx, ioffy, ianglecen, ianglespan = unpack(item)
    if not img:typeOf('Image') then
      -- Create Image from ImageData
      img = love.graphics.newImage(img, { mipmaps = true })
      img:setFilter('linear', 'linear')
      item[1] = img
      -- print('create') logstats()
    end
    imgshader:send('screenoffs', {0, freeze and canvasheadroom or offy})
    imgshader:send('tex_dims', {iw * isc, ih * isc})
    -- Unexpectedly generates subtle textures!!
    imgshader:send('seed', {i * 2000, i * 4000})
    imgshader:send('time', T / 240 - scenestarttime - i * enterinterval)
    imgshader:send('angle_cen', -ianglecen)
    imgshader:send('angle_span', ianglespan)
    love.graphics.draw(img,
      arcox + ioffx,
      arcoy + ioffy + (freeze and canvasheadroom or offy),
      0, isc)
    if freeze then
      -- Release Image object, freeing GPU texture memory
      img:release()
      -- print('freeze') logstats()
      item[1] = nil
    end
  end
  for i = lastfrozen + 1,
    math.min(#imgs, lastfrozen + freezeafter / enterinterval + 1)
  do
    drawimg(i, false)
  end
  local frozen = math.floor((T / 240 - scenestarttime - freezeafter) / enterinterval)
  if frozen > lastfrozen and frozen <= #imgs then
    love.graphics.setBlendMode('alpha')
    love.graphics.setCanvas(canvasfrozen)
    drawimg(frozen, true)
    lastfrozen = frozen
  end
  love.graphics.setBlendMode('alpha', 'premultiplied')
  love.graphics.setCanvas(nil)
  love.graphics.setShader(nil)
  love.graphics.setColor(1 - moveuprate, 1 - moveuprate, 1 - moveuprate, 1 - moveuprate)
  love.graphics.draw(canvasupper, 0, 0)
  love.graphics.setShader(watershader)
  watershader:send('screenoffs', offy)
  watershader:send('time', T / 240)
  watershader:send('texdevrate', math.exp(-((T / 240 - scenestarttime) / expandtime)))
  love.graphics.setColor(1, 1, 1)
  love.graphics.draw(canvasupper, 0, 0)
end

local draw = function ()
  local moveuprate = 0
  local t = T / 240
  if t >= scenestarttime + expandtime then
    local x = math.min(1, (t - (scenestarttime + expandtime)) / moveuptime)
    if x < 0.5 then
      x = x * x * x * 4
    else
      x = 1 - (1 - x) * (1 - x) * (1 - x) * 4
    end
    moveuprate = x
  elseif t < scenestarttime + keepmovedowntime then
    if t <= 1 then moveuprate = 1
    else
      local x = (t - 1) / (scenestarttime + keepmovedowntime - 1)
      moveuprate = math.exp(-5 * x) * (1 - x)
    end
  end
  drawarco(moveuprate)
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

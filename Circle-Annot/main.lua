local W = 1080
local H = 720

love.window.setMode(
  W, H,
  { fullscreen = false, highdpi = true }
)

local init = function (rawpath, mountedpath)
  require './utils'

  local imgpath = mountedpath
  local annotpath = rawpath .. '/annot.txt'

  local path, name
  local img
  local iw, ih, isc, ioffx, ioffy

  local anchors
  local anchorsel, anchorseloffx, anchorseloffy

  local cx, cy, cr  -- Fit circle

  -- Saved annotations
  local annot, annotlist
  local selectedindex

  local updateimage = function ()
    name = annotlist[selectedindex]
    path = mountedpath .. '/' .. name
    local contents = love.filesystem.read('data', path)
    img = love.graphics.newImage(contents)
    iw, ih = img:getDimensions()
    isc = math.min(W / iw, H / ih)
    ioffx = (W - iw * isc) / 2
    ioffy = (H - ih * isc) / 2
    anchors = annot[name]
    anchorsel = nil
    cx, cy, cr = fitcircle(anchors)
  end

  -- Screen coordinates to image coordinates
  local icoord = function (sx, sy)
    return
      (sx - ioffx) / isc,
      (sy - ioffy) / isc
  end
  -- Image coordinates to screen coordinates
  local scoord = function (ix, iy)
    return
      ix * isc + ioffx,
      iy * isc + ioffy
  end

  function love.mousepressed(x, y, button, istouch, presses)
    for i = 1, #anchors do
      local ax, ay = unpack(anchors[i])
      local sax, say = scoord(ax, ay)
      if (sax - x) * (sax - x) + (say - y) * (say - y) <= 25 then
        if button == 1 then
          -- Select
          anchorsel = i
          anchorseloffx = sax - x
          anchorseloffy = say - y
        else
          -- Remove
          table.remove(anchors, i)
          cx, cy, cr = fitcircle(anchors)
          return
        end
        break
      end
    end
    if anchorsel == nil then
      local ix, iy = icoord(x, y)
      anchors[#anchors + 1] = {ix, iy}
      anchorsel = #anchors
      anchorseloffx, anchorseloffy = 0, 0
      cx, cy, cr = fitcircle(anchors)
    end
  end

  function love.mousemoved(x, y, button, istouch, presses)
    if anchorsel ~= nil then
      local ix, iy = icoord(x + anchorseloffx, y + anchorseloffy)
      anchors[anchorsel][1] = ix
      anchors[anchorsel][2] = iy
      cx, cy, cr = fitcircle(anchors)
    end
  end

  function love.mousereleased(x, y, button, istouch, presses)
    anchorsel = nil
  end

  love.keyboard.setKeyRepeat(true)
  function love.keypressed(key, scancode, isrepeat)
    if key == 'left' or key == 'up' then
      selectedindex = (selectedindex + #annotlist - 2) % #annotlist + 1
      saveannots(annotpath, annot, annotlist)
      updateimage()
    elseif key == 'right' or key == 'down' then
      selectedindex = selectedindex % #annotlist + 1
      saveannots(annotpath, annot, annotlist)
      updateimage()
    end
  end

  annot, annotlist = loadannots(annotpath, imgpath)

  if os.getenv('stats') ~= nil then
    local hist = {}
    for i = 0, 180 do hist[i] = 0 end
    for i = 1, #annotlist do
      local a = annot[annotlist[i]]
      local cx, cy, cr = fitcircle(a)
      if cx ~= nil then
        local min, max = 180, 0
        for j = 1, #a do
          local angle = math.atan2(-(a[j][2] - cy), a[j][1] - cx) * (180 / math.pi)
          angle = (angle + 90) % 360 - 90   -- Range: [-90, 270)
          min = math.min(min, angle)
          max = math.max(max, angle)
        end
        min = math.max(min, 0)
        max = math.min(max, 180)
        for k = math.floor(min + 0.5), math.floor(max + 0.5) do
          hist[k] = hist[k] + 1
        end
      end
    end
    local scale = math.max(1, math.max(unpack(hist)) / 80)
    for i = 0, 180 do
      print(i, string.rep('#', math.floor(hist[i] / scale + 0.5)))
    end
  end

  selectedindex = 1
  for i = 1, #annotlist do
    if #annot[annotlist[i]] == 0 then
      selectedindex = i
      break
    end
  end
  updateimage()

  function love.draw()
    love.graphics.clear(0.99, 0.99, 0.98)
    love.graphics.setColor(1, 1, 1)
    love.graphics.draw(img, ioffx, ioffy, 0, isc)
    if cr ~= nil then
      love.graphics.setColor(0, 0, 0)
      local scx, scy = scoord(cx, cy)
      love.graphics.circle('line', scx, scy, cr * isc)
    end
    for i = 1, #anchors do
      local sx, sy = scoord(unpack(anchors[i]))
      love.graphics.setColor(1, 1, 1, 0.7)
      love.graphics.circle('fill', sx, sy, 5)
      love.graphics.setColor(0, 0, 0, 1)
      love.graphics.circle('line', sx, sy, 5)
    end
    love.graphics.setColor(0, 0, 0)
    love.graphics.print(tostring(selectedindex) .. '/' .. tostring(#annotlist), 20, 18)
    love.graphics.print(name, 20, 36)
  end

  function love.quit()
    saveannots(annotpath, annot, annotlist)
  end
end

function love.draw()
  love.graphics.clear(0.7, 0.7, 0.68)
  love.graphics.setColor(0, 0, 0)
  love.graphics.print('0/0', 20, 18)
end

function love.directorydropped(path)
  love.filesystem.mount(path, 'img')
  init(path, 'img')
end

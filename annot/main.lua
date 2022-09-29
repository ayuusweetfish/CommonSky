local W = 1080
local H = 720

love.window.setMode(
  W, H,
  { fullscreen = false, highdpi = true }
)

local imgpath = '../img'

local path, name
local img
local iw, ih, isc, ioffx, ioffy

local anchors
local anchorsel, anchorseloffx, anchorseloffy

local cx, cy, cr  -- Fit circle

-- Saved annotations
local annot = {}      -- name -> list of anchors
local annotlist = {}  -- index -> name
local saveannots      -- Defined later

local selectedindex

local fitcircle = function ()
  if #anchors <= 2 then
    cx, cy, cr = nil, nil, nil
    return
  end
  -- Coope's method
  local N = #anchors
  local X = {}  -- N * 3
  local Y = {}  -- N
  for i = 1, N do
    local ax, ay = unpack(anchors[i])
    -- (ax - X)^2 + (ay - Y)^2 = R^2
    local coeffx = -2 * ax
    local coeffy = -2 * ay
    local coeffc = -(ax * ax + ay * ay)
    -- Cx * X + Cy * Y + 1 * D = Cc
    -- where D = X^2 + Y^2 - R^2
    X[#X + 1] = coeffx
    X[#X + 1] = coeffy
    X[#X + 1] = 1
    Y[#Y + 1] = coeffc
  end
  -- Linear least squares
  -- B = inv(X^T X) * X^T * Y
  local XTX = {}  -- 3 * 3
  for i = 1, 3 do
    for j = 1, 3 do
      local s = 0
      for k = 1, N do
        s = s + X[(k - 1) * 3 + i] * X[(k - 1) * 3 + j]
      end
      XTX[(i - 1) * 3 + j] = s
    end
  end
  local XTY = {}  -- 3
  for i = 1, 3 do
    local s = 0
    for k = 1, N do
      s = s + X[(k - 1) * 3 + i] * Y[k]
    end
    XTY[i] = s
  end
  -- Invert (X^T X)
  local a11, a12, a13, a21, a22, a23, a31, a32, a33 = unpack(XTX)
  local d =   -- Determinant
      a11 * (a22 * a33 - a32 * a23)
    - a21 * (a12 * a33 - a13 * a32)
    + a31 * (a12 * a23 - a13 * a22)
  local invXTX = {
     (a22*a33 - a32*a23)/d, -(a12*a33 - a13*a32)/d,  (a12*a23 - a13*a22)/d,
    -(a21*a33 - a23*a31)/d,  (a11*a33 - a13*a31)/d, -(a11*a23 - a13*a21)/d,
     (a21*a32 - a22*a31)/d, -(a11*a32 - a12*a31)/d,  (a11*a22 - a12*a21)/d,
  }
  -- Solution to LLS
  local B = {}
  for i = 1, 3 do
    local s = 0
    for k = 1, 3 do
      s = s + invXTX[(i - 1) * 3 + k] * XTY[k]
    end
    B[i] = s
  end
  -- for i = 1, 3 do print(B[i]) end
  cx = B[1]
  cy = B[2]
  cr = math.sqrt(cx * cx + cy * cy - B[3])
  -- print(cx, cy, cr)
end

local updateimage = function ()
  name = annotlist[selectedindex]
  path = imgpath .. '/' .. name
  local contents = io.open(path, 'r'):read('*a')
  img = love.graphics.newImage(love.data.newByteData(contents))
  iw, ih = img:getDimensions()
  isc = math.min(W / iw, H / ih)
  ioffx = (W - iw * isc) / 2
  ioffy = (H - ih * isc) / 2
  anchors = annot[name]
  anchorsel = nil
  fitcircle()
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
        fitcircle()
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
    fitcircle()
  end
end

function love.mousemoved(x, y, button, istouch, presses)
  if anchorsel ~= nil then
    local ix, iy = icoord(x + anchorseloffx, y + anchorseloffy)
    anchors[anchorsel][1] = ix
    anchors[anchorsel][2] = iy
    fitcircle()
  end
end

function love.mousereleased(x, y, button, istouch, presses)
  anchorsel = nil
  saveannots()
end

love.keyboard.setKeyRepeat(true)
function love.keypressed(key, scancode, isrepeat)
  if key == 'left' or key == 'up' then
    selectedindex = (selectedindex + #annotlist - 2) % #annotlist + 1
    updateimage()
  elseif key == 'right' or key == 'down' then
    selectedindex = selectedindex % #annotlist + 1
    updateimage()
  end
end

-- Load saved annotations
local f = io.open(imgpath .. '/annot.txt', 'r')
while true do
  local s = f:read('*l')
  if s == nil then break end
  local index = s:find('\t')
  if index ~= nil then
    local name = s:sub(1, index - 1)
    local anchors = {}
    while index ~= nil do
      local nextindex1 = s:find('\t', index + 1)
      local nextindex2 = s:find('\t', nextindex1 + 1)
      local x = tonumber(s:sub(index + 1, nextindex1 - 1))
      local y = tonumber(s:sub(nextindex1 + 1, (nextindex2 or 0) - 1))
      anchors[#anchors + 1] = {x, y}
      index = nextindex2
    end
    annot[name] = anchors
    annotlist[#annotlist + 1] = name
  elseif annot[s] == nil then
    annot[s] = {}
    annotlist[#annotlist + 1] = s
  end
end
f:close()

saveannots = function ()
  local f = io.open(imgpath .. '/annot.txt', 'w')
  for i = 1, #annotlist do
    local name = annotlist[i]
    f:write(name)
    local a = annot[name]
    for j = 1, #a do
      f:write('\t', a[j][1], '\t', a[j][2])
    end
    f:write('\n')
  end
  f:close()
end
saveannots()

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

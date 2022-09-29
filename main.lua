local W = 1080
local H = 720

love.window.setMode(
  W, H,
  { fullscreen = false, highdpi = true }
)

local path, name
local img
local iw, ih, isc, ioffx, ioffy

local anchors
local anchorsel, anchorseloffx, anchorseloffy

local cx, cy, cr  -- Fit circle

local setImage = function (p)
  path = p
  local index = p:find("/[^/]*$")
  if index ~= nil then
    name = p:sub(index + 1)
  else
    name = p
  end
  local contents = io.open(p, 'r'):read('*a')
  img = love.graphics.newImage(love.data.newByteData(contents))
  iw, ih = img:getDimensions()
  isc = math.min(W / iw, H / ih)
  ioffx = (W - iw * isc) / 2
  ioffy = (H - ih * isc) / 2
  --anchors = {{40, 150}, {100, 250}, {200, 350}, {300, 400}}
  anchors = {}
  anchorsel = nil
end

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

setImage('1.png')
--fitcircle()

local ptx, pty = 0, 0

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
    if (ax - x) * (ax - x) + (ay - y) * (ay - y) <= 25 then
      anchorsel = i
      anchorseloffx = ax - x
      anchorseloffy = ay - y
      break
    end
  end
  if anchorsel == nil then
    anchors[#anchors + 1] = {x, y}
    anchorsel = #anchors
    anchorseloffx, anchorseloffy = 0, 0
    fitcircle()
  end
end

function love.mousemoved(x, y, button, istouch, presses)
  ptx, pty = x, y
  if anchorsel ~= nil then
    anchors[anchorsel][1] = x + anchorseloffx
    anchors[anchorsel][2] = y + anchorseloffy
    fitcircle()
  end
end

function love.mousereleased(x, y, button, istouch, presses)
  anchorsel = nil
end

function love.draw()
  love.graphics.clear(0.99, 0.99, 0.98)
  love.graphics.setColor(1, 1, 1)
  love.graphics.draw(img, ioffx, ioffy, 0, isc)
  if cr ~= nil then
    love.graphics.setColor(0, 0, 0)
    love.graphics.circle('line', cx, cy, cr)
  end
  for i = 1, #anchors do
    local sx, sy = unpack(anchors[i])
    love.graphics.setColor(1, 1, 1, 0.7)
    love.graphics.circle('fill', sx, sy, 5)
    love.graphics.setColor(0, 0, 0, 1)
    love.graphics.circle('line', sx, sy, 5)
  end
end

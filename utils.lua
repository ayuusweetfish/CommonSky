-- Returns cx, cy, cr
local fitcircle = function (anchors)
  if #anchors <= 2 then
    return nil, nil, nil
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
  local cx = B[1]
  local cy = B[2]
  local cr = math.sqrt(cx * cx + cy * cy - B[3])
  return cx, cy, cr
end

-- Annotations

-- Save annotations
-- annot: name -> list of anchors
-- annotlist: index -> name
local saveannots = function (annotpath, annot, annotlist)
  local f = io.open(annotpath, 'w')
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

-- Load saved annotations
-- Returns annot, annotlist
-- If imgdir is nil, new images will not be added automatically.
-- Run the following after adding new images:
-- find . \( -name "*.jpg" -o -name "*.jpeg" -o -name "*.JPG" -o -name "*.png" \) -exec basename {} \; >> annot.txt
local loadannots = function (annotpath, imgdir)
  local annot = {}
  local annotlist = {}
  local f = io.open(annotpath, 'r')
  if f ~= nil then
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
  end
  -- List images automatically if asked to
  if imgdir ~= nil then
    for _, name in ipairs(love.filesystem.getDirectoryItems(imgdir)) do
      if (name:sub(-4):lower() == '.png' or
          name:sub(-4):lower() == '.jpg' or
          name:sub(-5):lower() == '.jpeg')
        and annot[name] == nil
      then
        annot[name] = {}
        annotlist[#annotlist + 1] = name
      end
    end
  end
  saveannots(annotpath, annot, annotlist)
  return annot, annotlist
end

_G['fitcircle'] = fitcircle
_G['saveannots'] = saveannots
_G['loadannots'] = loadannots

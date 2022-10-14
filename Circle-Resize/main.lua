local IMG_W = 1920
local IMG_H = 1080

love.window.setMode(
  960, 540,
  { fullscreen = false, highdpi = true }
)

require './utils'

local font = love.graphics.newFont(30)
local text = love.graphics.newText(font,
  {{0.1, 0.1, 0.1}, 'Please drop folder here'}
)

local init = function (rawpath, mountedpath)
  local imgpath = mountedpath
  local annotpath = rawpath .. '/annot.txt'
  local annot, annotlist = loadannots(annotpath, imgpath)
  local tasks = {}
  for i = 1, #annotlist do
    local name = annotlist[i]
    local a = annot[name]
    local cx, cy, cr = fitcircle(a)
    if cx ~= nil then
      tasks[#tasks + 1] = {
        name,
        rawpath .. '/' .. name,
        rawpath .. '/' .. name .. '.resized.png',
        cx, cy, cr
      }
    end
  end
  local thread = love.thread.newThread('process.lua')
  thread:start(tasks)
end

function love.draw()
  local t = love.thread.getChannel('process_progress'):pop()
  if t ~= nil then
    text = love.graphics.newText(font, t)
  end

  love.graphics.clear(0.99, 0.96, 0.94)
  love.graphics.setColor(1, 1, 1)
  local W, H = love.graphics.getDimensions()
  local w, h = text:getDimensions()
  love.graphics.draw(text, W / 2, H / 2, 0, 1, 1, w / 2, h / 2)
end

function love.directorydropped(path)
  love.filesystem.mount(path, 'img')
  init(path, 'img')
end

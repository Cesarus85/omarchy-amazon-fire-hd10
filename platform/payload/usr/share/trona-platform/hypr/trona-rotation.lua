-- Keep the built-in touchscreen mapped to the panel, including rotations
-- made through the sensor controller or Omarchy's own display panel.
local orientation = 0
local file = io.open(os.getenv("HOME") .. "/.local/state/trona/orientation", "r")
if file then
  local saved = tonumber(file:read("*l"))
  file:close()
  if saved and saved >= 0 and saved <= 3 and saved == math.floor(saved) then orientation = saved end
end
local display_scale = 1.25
local scale_file = io.open(os.getenv("HOME") .. "/.local/state/trona/scale", "r")
if scale_file then
  local saved = tonumber(scale_file:read("*l")); scale_file:close()
  if saved == 1 or saved == 1.25 or saved == 1.5 then display_scale = saved end
end
hl.monitor({ output = "DSI-1", mode = "preferred", position = "auto", scale = display_scale, transform = orientation })
local last_transform = nil
local function sync_touch()
  local monitor = hl.get_monitor("DSI-1")
  if not monitor then return end
  local transform = monitor.transform or 0
  if transform == last_transform then return end
  last_transform = transform
  local path = os.getenv("HOME") .. "/.local/state/trona/orientation"
  local state_file = io.open(path .. ".lua.tmp", "w")
  if state_file then
    state_file:write(tostring(transform) .. "\n"); state_file:close()
    os.rename(path .. ".lua.tmp", path)
  end
  hl.config({ input = { touchdevice = { output = "DSI-1", transform = transform } } })
  hl.exec_cmd("systemctl --user restart trona-keyboard.service")
end
sync_touch()
hl.on("monitor.layout_changed", sync_touch)
hl.on("monitor.added", sync_touch)
hl.on("hyprland.start", sync_touch)

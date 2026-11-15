-- Start with an empty compositor and the authenticated touch greeter.
-- Desktop configuration and application bindings are loaded only after PAM success.
local runtime = os.getenv("XDG_RUNTIME_DIR")
local stat = io.open("/proc/self/stat", "r")
local pid = stat:read("*l"):match("^(%d+)"); stat:close()
local marker = runtime .. "/trona-session-" .. pid
local f = io.open(marker, "r")
local phase = f and f:read("*l") or "locked"
if f then f:close() end
if phase == "ready" or phase == "activating" then
  local callbacks = {}
  local real_on = hl.on
  if phase == "activating" then
    hl.on = function(event, callback, ...)
      if event == "hyprland.start" then table.insert(callbacks, callback) end
      return real_on(event, callback, ...)
    end
  end
  local ok, err = pcall(dofile, os.getenv("HOME") .. "/.config/hypr/hyprland.lua")
  hl.on = real_on
  if not ok then error(err) end
  if phase == "activating" then
    for _, callback in ipairs(callbacks) do callback() end
    local out = assert(io.open(marker, "w")); out:write("ready\n"); out:close()
  end
else
  hl.config({input = { kb_layout = "de", follow_mouse = 0 }, misc = { disable_hyprland_logo = true }})
  dofile("/usr/share/trona-platform/hypr/trona-rotation.lua")
  hl.on("hyprland.start", function()
    hl.exec_cmd("/usr/local/libexec/trona-login")
  end)
end

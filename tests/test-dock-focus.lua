-- Run from the repository root. Capture the compositor operations in order.
local calls = {}
local ws = { id = 11 }
local monitor = { name = "DSI-1", active_workspace = ws }
local target = { id = "files", fullscreen = 1, floating = false, workspace = ws }
local function floating(id, overrides)
  local w = { id = id, floating = true, mapped = true, allowed_over_fullscreen = true,
              tags = { "omarchy-floating-workspace", "omarchy-mode-floated" } }
  for k,v in pairs(overrides or {}) do w[k] = v end
  return w
end
local windows = {
  target, floating("terminal"), floating("second-app"),
  floating("pinned", { pinned = true }), floating("dialog", { tags = {} }),
  floating("already-below", { allowed_over_fullscreen = false }),
  floating("hidden", { hidden = true }), floating("unmapped", { mapped = false })
}
o = { bind = function() end }
hl = {
  window_rule = function() end,
  get_window = function(selector) if selector == "address:selected" then return target end end,
  get_active_monitor = function() return monitor end,
  get_workspace_windows = function(id) assert(id == "11"); return windows end,
  dsp = {
    window = {
      close = function() end,
      alter_zorder = function(args) return args.mode .. " " .. args.window.id end
    },
    focus = function(args) return "focus " .. args.window.id end
  },
  dispatch = function(op) table.insert(calls,op) end
}
dofile("platform/payload/usr/share/trona-platform/hypr/trona-windows.lua")
local function expect(expected, selector)
  calls = {}
  trona_dock_focus(selector or "address:selected")()
  assert(table.concat(calls,",") == expected, table.concat(calls,","))
end
expect("bottom terminal,bottom second-app,focus files")
expect("", "address:closed")
target.floating = true
expect("top files,focus files")
target.floating = false
monitor.active_workspace = { id = 12 }
expect("top files,focus files")
monitor.active_workspace = ws
monitor.name = "HDMI-A-1"
expect("top files,focus files")
print("Dock dispatch order and overlay exclusions passed")

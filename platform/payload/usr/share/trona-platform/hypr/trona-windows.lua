-- Regular tiled applications start maximized, retaining the bar and keyboard.
-- App-specific floating windows (dialogs, PiP, etc.) keep their own rules.
-- Floating-workspace apps are sized by trona-fit-windows instead, so Omarchy's
-- automatic unmaximize-on-open does not shrink the previous app again.
hl.window_rule({
  name = "trona-tablet-apps",
  match = { float = false, modal = false, workspace = "r[1-2147483647]" },
  maximize = true,
})

-- Accessible as a latched Super, Alt, W sequence on the tablet keyboard.
o.bind("SUPER + ALT + W", "Close window (tablet)", hl.dsp.window.close())

-- A maximized tiled app can hold keyboard focus while a workspace-managed
-- floating app still covers it. Apply stacking and focus in one dispatch.
function trona_dock_focus(selector)
  return function()
    local target = hl.get_window(selector)
    if not target then return end
    local monitor = hl.get_active_monitor()
    local workspace = target.workspace
    local tablet = monitor and monitor.name == "DSI-1" and workspace
      and monitor.active_workspace and monitor.active_workspace.id == workspace.id

    if tablet and target.fullscreen == 1 and not target.floating then
      for _, other in ipairs(hl.get_workspace_windows(tostring(workspace.id))) do
        if other.floating and other.mapped and not other.hidden and not other.pinned
            and other.allowed_over_fullscreen then
          for _, tag in ipairs(other.tags or {}) do
            if tag == "omarchy-mode-floated" then
              hl.dispatch(hl.dsp.window.alter_zorder({ mode = "bottom", window = other }))
              break
            end
          end
        end
      end
    else
      hl.dispatch(hl.dsp.window.alter_zorder({ mode = "top", window = target }))
    end
    hl.dispatch(hl.dsp.focus({ window = target }))
  end
end

-- Regular tiled applications start maximized, retaining the bar and keyboard.
-- App-specific floating windows (dialogs, PiP, etc.) keep their own rules.
-- Floating-workspace apps are sized by trona-fit-windows instead, so Omarchy's
-- automatic unmaximize-on-open does not shrink the previous app again.
hl.window_rule({
  name = "trona-tablet-apps",
  match = { float = false, modal = false, workspace = "r[1-2147483647]" },
  maximize = true,
})

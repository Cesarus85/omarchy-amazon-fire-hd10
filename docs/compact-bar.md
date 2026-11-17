# Compact tablet status bar

The desktop default puts growing widget groups on both sides of an independently
centered clock. On the Trona's portrait display at 150% scale, the long rotation
labels, workspaces and desktop extras can run into the center group.

The optional `tools/trona-compact-bar.py` applies a simpler layout to the current
user's `~/.config/omarchy/shell.json`:

- Left: menu, rotation, automatic-rotation lock/state, on-screen keyboard,
  scale and the original workspace switcher.
- Right: Bluetooth, network, audio, brightness, clock and battery percentage.
- Empty center groups, so the clock no longer sits in the path of the left group.
- Short rotation label `↻`; automatic-rotation state remains readable.
- Both clock display formats stay `HH:mm` to prevent an alternate long date
  from expanding the bar. The calendar remains available by tapping the clock.
- World clocks, weather, tray, agents, VPN and other extras are omitted from the
  visible bar. Their widget configurations remain in `bar.tronaCompactHidden`
  for later customization; they are not placed in a hover-only overflow drawer.
  Updates remain available through the ordinary Omarchy menu.

Run once as **trona**, inside the normal desktop session:

```sh
python3 tools/trona-compact-bar.py
```

Omarchy watches and reloads shell.json. No compositor restart, system QML patch
or platform-package rebuild is required. Re-running the tool is idempotent and
preserves settings outside the bar, including the dock and idle policy. The
rotation helper continues updating its own state label. The existing platform
repair checks retain the layout because all four tablet control IDs remain
present.

On 2026-11-16, screenshots of the reference device showed no overlap in portrait
and landscape at 150% scale with its five displayed workspace slots. The rotation
helper verified display, touch and keyboard geometry in both formats, and the
original portrait orientation was restored. `trona-update-check check` passed.
This is a compact layout, not an arbitrary-width overflow engine: an unusually
large font or many additional custom widgets still requires layout adjustment.

To customize later, edit `bar.layout` or use Omarchy's normal bar tools. An omitted
widget can be moved from `bar.tronaCompactHidden` into a visible section. Keep
`bar.locked=true` for touch use.

## Bottom touch controls

The Trona platform package adds three buttons along the bottom of the built-in
display. Each button has a 48-logical-pixel-high touch target. The row reserves
52 logical pixels. Opening the on-screen keyboard places it immediately above
the row. The existing status widgets keep their space at the top.

- **Apps** opens or dismisses Omarchy's app grid.
- **Zurück / Esc** sends Escape to the focused app or menu. Its behavior in
  an app is the same as pressing the physical Esc key.
- **Schließen** asks the active window to close. Applications can still prompt
  to save. The button is disabled while the app grid covers the window.

On the desktop keyboard, tap **Super**, **Alt**, then **W** to close the
focused window. On a hardware keyboard, hold both modifiers while pressing W.

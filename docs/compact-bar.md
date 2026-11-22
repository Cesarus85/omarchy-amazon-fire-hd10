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
  from expanding the bar. On the tablet the status group opens the control center.
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

The control center keeps the configured rotation-lock, audio and brightness
widgets available to the shell and hides their individual bar slots on the built-in
display. The remaining status icons share the control-center touch target.

## Touch controls in the app dock

The built-in display uses Omarchy's bottom dock for navigation. Tap the nine
dots to open the app grid, or tap an app icon to open or switch to that app.
The two icons at the right use the same touch target size as the app icons.
Hold an app icon in the dock for about half a second to open its menu. Choose
**Remove from dock** to unpin the app, or **Keep in dock** to pin a running
app. A running app remains visible until its last window closes.

- **←** sends Escape to the focused app. In the app grid the arrow is beside
  the search field. It dismisses an open app menu, clears a search, or returns
  to the desktop.
- **×** asks the active window to close. Applications can prompt to save.
  The app grid has its own Back button so a window underneath stays safe.

With the screen keyboard open, the dock releases its row to the focused app.
Swipe up from the small grip at the bottom centre of the screen to show the
dock below the keyboard. The keyboard moves up while the dock is visible.
Swipe down from the icons or tap the grip between the keyboard and the dock
to dismiss it. It also hides after six seconds of inactivity. An open dock
menu stays available while you choose an action. Closing the keyboard
restores the usual dock.
This works with both automatic and manual keyboard visibility.

With the keyboard closed, the dock reserves its icon row on tiled and floating workspaces. A true
fullscreen app can use the entire screen. When the row fills up, swipe the
app icons sideways. On the screen keyboard, **Super**, **Alt**, then **W**
also closes the active window. On a hardware keyboard, hold both modifiers
while pressing W.

## Tablet control center

Tap the status group around the clock and battery, or swipe down from the top
right of the built-in screen. In fullscreen the swipe starts in the top seven
logical pixels of the rightmost 100 pixels. Scrolling started inside an app
continues to go to the app.

The brightness and volume sliders use the same controls as the Omarchy panels.
The Wi-Fi and Bluetooth tiles show the current connection and open the native
device lists. The rotation lock tile switches automatic rotation. The manual
rotate button and the on-screen keyboard button remain in the bar.

Tap outside the card, tap its close button, press Escape, or swipe up on the
card to dismiss it. In very short work areas the card scrolls to keep every
control reachable. The panel uses available space above the keyboard and dock.
Its width follows the screen in portrait and is capped at 410 logical pixels
in landscape. The platform update hook restores the bar integration.

When a text field has opened the keyboard automatically, temporary focus in
the control center keeps that keyboard in place. After dismissal, focus in
the app determines whether it stays visible. The keyboard focus listener
waits for Hyprland's layer-close event while a hide is deferred.

On the reference Fire HD 10, virtual touchscreen taps and drags exercised
the installed bar in portrait and landscape, each with the keyboard shown
and hidden. Tests also used an editable GTK field to open the keyboard
automatically, checked backlight and audio changes against their device
values, and verified that the Wi-Fi and Bluetooth tiles opened their
existing panels. Screenshots of the two layouts were checked on this device.

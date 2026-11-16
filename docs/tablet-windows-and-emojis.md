# Tablet app sizes and Unicode input

Platform package **2026.11.16-3** includes these fixes for the pinned Omarchy
4.5.0 / Hyprland 0.57.2 installation.

## Application windows

Omarchy's saved floating workspace mode opens regular apps at approximately
64% width and 68% height. `trona-fit-windows` now fills the DSI-1 work area for
windows tagged `omarchy-mode-floated`, respecting display scale, rotation and
space reserved by the bar and on-screen keyboard. A maximized app that Omarchy
restores when another app opens also returns to a full work-area window.

The managed `trona-windows.lua` rule opens ordinary non-floating, non-modal
applications maximized. It is installed through the existing update repair
path. Special workspaces are excluded. Dialogs, app-specific floating windows,
manually floated windows and pinned windows are not automatically enlarged;
the existing clamp still keeps oversized floating dialogs within the work area
when the app permits resizing. Application minimum sizes can still constrain it.

## Emoji input

The wvkbd Unicode keys temporarily remap the Compose/Menu key to the selected
character. Hyprland 0.57.2's
[input-method keyboard grab](https://github.com/hyprwm/Hyprland/blob/v0.57.2/src/protocols/InputMethodV2.cpp)
returns early when the keyboard identity is unchanged, even if its keymap has
changed. Consequently Fcitx can receive an outdated mapping: a context-menu key
or the first selected emoji repeated for subsequent selections.

`userspace/wvkbd/wvkbd-ime-keymap.patch` creates a fresh virtual-keyboard protocol
object whenever a new keymap is uploaded. This makes the IME receive the current
map while keeping the keyboard surface and focus in place. Keys are released
before replacing their keyboard; local modifier latches are retained. The patch
also includes the required trailing NUL in the keymap size and releases its
temporary mapping and file descriptor after upload.

Fcitx remains enabled. There is no clipboard injection or application-specific
emoji replacement. Rebuild the keyboard with `userspace/wvkbd/build.sh` and stage
that binary with the updated platform sources; changing only the launcher script
does not fix this issue.

## Validation

- Both patches apply cleanly to the original wvkbd v0.16 archive; native ARM64
  keyboard compilation succeeds.
- On the reference device the original keyboard repeated one emoji in a GTK
  entry with Fcitx active. Temporarily stopping Fcitx allowed different emojis.
  With the patched keyboard and Fcitx running again, the owner confirmed multiple
  different emojis and ordinary `abc` input.
- Geometry tests cover all four rotations, reserved bar/keyboard space, dialog
  clamping and exclusion of fullscreen, pinned and external-monitor windows.

For physical acceptance, open multiple apps, switch between them, rotate the
tablet and toggle its keyboard. Check that app content remains usable and a file
chooser is still a dialog. Test emoji entry in both a native app and a browser,
then letters and an on-screen Super shortcut. A restart should retain both fixes.

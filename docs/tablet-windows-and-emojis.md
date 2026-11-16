# Tablet app sizes and Unicode input

Platform package **2026.11.16-5** includes these fixes for the pinned Omarchy
4.5.0 / Hyprland 0.57.2 installation.

## Application windows

Omarchy's saved floating workspace mode opens regular apps at approximately
64% width and 68% height. `trona-fit-windows` now fills the DSI-1 work area for
windows tagged `omarchy-mode-floated`, respecting display scale, rotation and
space reserved by the bar and on-screen keyboard. A maximized app that Omarchy
restores when another app opens also returns to a full work-area window.
After resizing, the service explicitly restores the top-left position: Hyprland
otherwise resizes floating windows about their centre, placing their tops above
the bar when the on-screen keyboard disappears.

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

Chromium additionally handles the physical Menu key as a context-menu shortcut,
even when its keysym is Unicode. Unicode input therefore temporarily uses the
printable A-key position instead. Its regular mapping is restored before the
next ordinary key. This second fix is needed for browsers as well as the IME
cache fix; a successful GTK entry test alone did not establish browser support.

Chromium additionally produced replacement characters for supplementary-plane
Unicode key events. For these characters, `trona-insert-unicode` now uses UTF-8
clipboard paste **only in Chromium-family windows**. It remembers the previous
clipboard in memory, restores it after insertion, and leaves a newer user copy
alone. It preserves one preferred MIME representation (file URIs, PNG/JPEG or
text); alternate representations of a multi-format clipboard are not retained.
Reads are bounded to 8 MiB and fail before changing an unpreservable clipboard.
Clipboard contents are never logged or written to disk. Focus changes abort
insertion. Ordinary native apps continue to receive direct Unicode keys.

Fcitx remains enabled. Rebuild the keyboard with `userspace/wvkbd/build.sh` and stage
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
  clamping, re-anchoring after centre-based resize, and exclusion of fullscreen,
  pinned and external-monitor windows.

The owner then confirmed correct window positioning after keyboard hiding and
correct different emojis followed by `abc` in Chrome with the final paste path.
The on-screen Super shortcut was separately confirmed working. Source tests also
cover clipboard restoration, focus changes and a concurrent user copy. No full
reboot was performed during this UI-only fix.

For regression checks, open multiple apps, switch between them, rotate the
tablet and toggle its keyboard. Check that app content remains usable and a file
chooser is still a dialog. Test emoji entry in both a native app and a browser,
then letters and an on-screen Super shortcut. A restart should retain both fixes.

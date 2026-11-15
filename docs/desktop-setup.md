# User configuration prerequisites

The platform package deliberately does not bundle a user's home directory.
Start with the default configuration from the **matching Omarchy package**,
owned by `trona:trona`. The runtime uses Omarchy's Lua-generation Hyprland
configuration, not the older hyprland.conf syntax.

In `~/.config/hypr/looknfeel.lua`, the reference uses:

```lua
hl.config({
  decoration = { blur = { enabled = false }, shadow = { enabled = false } },
  animations = { enabled = false },
  cursor = { no_hardware_cursors = true },
  xwayland = { enabled = false },
})
```

Set `input.lua` to your keyboard layout (reference: `de`) and
`follow_mouse = 0`. In the user's `autostart.lua`, register:

```lua
hl.on("hyprland.start", function()
  hl.exec_cmd("uwsm finalize")
  hl.exec_cmd("/usr/local/bin/trona-desktop-apps")
end)
```

Do not add a terminal launch. The usual Omarchy bootstrap still starts its shell.
The platform session wrapper starts the secure greeter first and activates the
normal configuration only after PAM authentication.

Create `~/.local/state/trona/`. The initial state files are plain text:
`orientation` = 0, `scale` = 1.25, `auto-rotate` = 1.
The keyboard helper manages its visibility setting.

The repair hook links the managed rotation Lua file into the user's Hyprland
config, adds the bar controls, locks the bar to the top, and applies checked
Omarchy app-grid/network-panel/lock patches. It preserves other user settings
and drops privileges before editing home-directory files.

Keep Omarchy's independent screensaver/idle-lock timers disabled in the matching
shell configuration; the native controller owns the five-minute standby policy.
Inspect the actual schema for the pinned version. Changing only an old
hypridle.conf is insufficient for this version.

Enable user services as the desktop user with a valid user bus:

```sh
systemctl --user enable trona-auto-rotate.service trona-keyboard.service \
  trona-fit-windows.service trona-sd-mount.service
```

Do not run the shell or camera app as root. An SD card is optional; the private
profile should use its own filesystem UUID or `NO-SD-CARD`.

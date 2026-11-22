#!/usr/bin/python3
"""Route a dock tap to one foreground action. Never read field values or window titles."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

CTL = ['/usr/bin/hyprctl', '-i', '0', '-j']


def query(name):
    return json.loads(subprocess.check_output(CTL + [name], timeout=1))


def shell_panel_open():
    levels = query('layers').get('DSI-1', {}).get('levels', {})
    panels = {'omarchy-keyboard-panel', 'omarchy-dock-apps', 'trona-control-center'}
    return any(layer['namespace'] in panels for group in levels.values() for layer in group)


def browser_page(window, api):
    role, state = api.Role, api.StateType
    if (window.get_attributes() or {}).get('class') != 'BrowserRootView':
        return False

    def matches(states, roles=(), attrs=None):
        all_match = api.CollectionMatchType.ALL
        rule = api.MatchRule.new(api.StateSet.new(states), all_match, attrs or {},
                                 all_match, roles, api.CollectionMatchType.ANY if roles else all_match,
                                 [], all_match, False)
        return window.get_matches(rule, api.CollectionSortOrder.CANONICAL, 1, True)

    if matches([state.SHOWING], attrs={'class': 'FindBarView'}):
        return False
    if matches([state.MODAL]) or matches([state.SHOWING], [role.MENU, role.POPUP_MENU, role.DIALOG, role.FILE_CHOOSER]):
        return False
    focused = matches([state.FOCUSED])
    node = focused[0] if focused else None
    for _ in range(48):
        if node is None or node == window:
            break
        if node.get_state_set().contains(state.EDITABLE):
            return False
        if node.get_role() == role.DOCUMENT_WEB:
            return True
        node = node.get_parent()
    return False


def files_view(window, api):
    if window.get_accessible_id() != 'NautilusWindow':
        return False
    role, state = api.Role, api.StateType
    chrome = [window]
    deadline = time.monotonic() + 0.65
    saw_view = False
    while chrome:
        if time.monotonic() > deadline:
            return False
        node = chrome.pop()
        states = node.get_state_set()
        if not states.contains(state.SHOWING):
            continue
        kind = node.get_role()
        if states.contains(state.MODAL) or states.contains(state.EDITABLE) or kind in (
                role.MENU, role.POPUP_MENU, role.DIALOG, role.FILE_CHOOSER):
            return False
        # GTK exposes the file view as a table. Skip its rows so a large
        # directory costs the same as an empty one.
        if kind in (role.TABLE, role.TREE_TABLE):
            saw_view = True
        elif kind not in (role.LIST_BOX, role.TREE, role.TABLE_CELL, role.LIST_ITEM):
            chrome.extend(node)
    return saw_view


def accessible_action(client, browser, portal=False):
    import gi
    gi.require_version('Atspi', '2.0')
    from gi.repository import Atspi, GLib
    # A shell restart can leave an unrelated defunct application in the
    # listener's AT-SPI cache. Refresh top-level windows before choosing one.
    desktop = Atspi.get_desktop(0)
    desktop.clear_cache_single()
    for app in desktop:
        try:
            if not app or app.get_process_id() != client['pid']:
                continue
        except GLib.Error:
            continue
        try:
            app.clear_cache_single()
            windows = []
            for window in app:
                window.clear_cache_single()
                if window.get_state_set().contains(Atspi.StateType.ACTIVE):
                    windows.append(window)
            if len(windows) != 1:
                break
            window = windows[0]
            if portal:
                if (window.get_role() == Atspi.Role.FILE_CHOOSER
                        and window.get_state_set().contains(Atspi.StateType.MODAL)):
                    return 'cancel-chooser'
            elif window.get_role() == Atspi.Role.FRAME:
                normal_view = browser_page(window, Atspi) if browser else files_view(window, Atspi)
                if normal_view:
                    return 'history'
        except GLib.Error:
            pass
        break
    return 'escape'


def main(expected):
    client = query('activewindow')
    if client.get('address', '') != expected:
        return
    app = client.get('class', '').lower()
    browser = app in ('google-chrome', 'chromium', 'chromium-browser') or app.startswith('chrome-')
    portal = app == 'xdg-desktop-portal-gtk'
    action = 'escape'
    if expected and (browser or portal or app == 'org.gnome.nautilus') and not shell_panel_open():
        action = accessible_action(client, browser, portal)
    if query('activewindow').get('address', '') != expected:
        return
    if action != 'escape' and shell_panel_open():
        action = 'escape'
    if action == 'cancel-chooser':
        # The GTK portal has a separate toplevel. Request closing that exact
        # chooser; a delayed key chord could reach the browser underneath.
        subprocess.run(['/usr/bin/hyprctl', '-i', '0', 'dispatch',
                        'hl.dsp.window.close({ window = "address:' + expected + '" })'],
                       check=True, timeout=2, stdout=subprocess.DEVNULL)
        return
    keys = ['-M', 'alt', '-k', 'Left', '-m', 'alt'] if action == 'history' else ['-k', 'Escape']
    subprocess.run(['/usr/bin/wtype', *keys], check=True, timeout=2)


# Reuse the desktop focus listener's warm accessibility cache. The command
# also works during a listener restart, using a fresh accessibility connection.
def endpoint():
    return str(Path(os.environ['XDG_RUNTIME_DIR']) / 'trona-dock-back.sock')


def listen(loop):
    receiver = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    path = Path(endpoint())
    path.unlink(missing_ok=True)
    receiver.bind(str(path))
    receiver.setblocking(False)

    def handle(fd, condition):
        try:
            raw = receiver.recv(64).decode('ascii')
            if raw.startswith('back '):
                main(raw[5:])
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
            print('Dock back action failed:', type(error).__name__, file=sys.stderr, flush=True)
        return True

    loop.io_add_watch(receiver.fileno(), loop.PRIORITY_DEFAULT, loop.IO_IN, handle)


def request(address):
    if address and (not address.startswith('0x') or not all(c in '0123456789abcdefABCDEF' for c in address[2:])):
        return
    with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as sender:
        sender.setblocking(False)
        try:
            sender.sendto(('back ' + address).encode('ascii'), endpoint())
        except (FileNotFoundError, ConnectionRefusedError):
            main(address)
        except BlockingIOError:
            pass


if __name__ == '__main__' and len(sys.argv) == 2:
    request(sys.argv[1])

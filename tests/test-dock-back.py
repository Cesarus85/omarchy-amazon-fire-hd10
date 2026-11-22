#!/usr/bin/env python3
"""A pending dock tap must never navigate a different surface."""
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import unittest
from unittest.mock import patch

path = Path(__file__).resolve().parents[1] / 'platform/payload/usr/local/libexec/trona_dock_back.py'
spec = spec_from_file_location('dock_back', path)
back = module_from_spec(spec)
spec.loader.exec_module(back)


class DockBack(unittest.TestCase):
    def route(self, app, action='history', panels=(False, False), addresses=('0x123', '0x123')):
        with patch.object(back, 'query', side_effect=[{'address': addr, 'class': app} for addr in addresses]), \
                patch.object(back, 'shell_panel_open', side_effect=panels), \
                patch.object(back, 'accessible_action', return_value=action), \
                patch.object(back.subprocess, 'run') as send:
            back.main('0x123')
            return send.call_args.args[0][1:] if send.called else None

    def test_history_only_in_supported_foreground_apps(self):
        for app in ('google-chrome', 'org.gnome.Nautilus'):
            self.assertEqual(self.route(app), ['-M', 'alt', '-k', 'Left', '-m', 'alt'])
        self.assertEqual(self.route('foot'), ['-k', 'Escape'])

    def test_unknown_context_and_shell_panels_dismiss(self):
        self.assertEqual(self.route('google-chrome', action='escape'), ['-k', 'Escape'])
        self.assertEqual(self.route('google-chrome', panels=(False, True)), ['-k', 'Escape'])
        self.assertEqual(self.route('org.gnome.Nautilus', panels=(True,)), ['-k', 'Escape'])

    def test_cancel_addresses_the_portal_window(self):
        sent = self.route('xdg-desktop-portal-gtk', action='cancel-chooser')
        self.assertEqual(sent[-1], 'hl.dsp.window.close({ window = "address:0x123" })')
        self.assertIsNone(self.route('xdg-desktop-portal-gtk', action='cancel-chooser',
                                     addresses=('0x123', '0x345')))

    def test_focus_change_cancels_delivery(self):
        self.assertIsNone(self.route('google-chrome', addresses=('0x234',)))
        self.assertIsNone(self.route('org.gnome.Nautilus', addresses=('0x123', '0x234')))


if __name__ == '__main__':
    unittest.main()

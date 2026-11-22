#!/usr/bin/python3
"""Run in a fresh GTK3 process with GTK3_MODULES set to trona-filechooser.so."""
import os
import unittest

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import GLib, Gtk


def descendants(widget):
    yield widget
    if isinstance(widget, Gtk.Container):
        children = []
        widget.forall(children.append)
        for child in children:
            yield from descendants(child)


class FileChooser(unittest.TestCase):
    def test_open_and_save_fit_at_tablet_font_size(self):
        self.assertTrue(os.path.isfile(os.environ["GTK3_MODULES"]))
        Gtk.Settings.get_default().set_property("gtk-xft-dpi", 134 * 1024)
        for action in (Gtk.FileChooserAction.OPEN, Gtk.FileChooserAction.SAVE):
            with self.subTest(action=action):
                dialog = Gtk.FileChooserDialog(title="Datei", action=action, use_header_bar=True)
                try:
                    dialog.add_buttons("Abbrechen", Gtk.ResponseType.CANCEL,
                                       "Bestätigen", Gtk.ResponseType.ACCEPT)
                    if action == Gtk.FileChooserAction.SAVE:
                        dialog.set_current_name("Ein langer Dateiname.txt")
                    dialog.show()
                    children = list(descendants(dialog))
                    fields = [w for w in children if isinstance(w, Gtk.Entry)]
                    self.assertTrue(any(isinstance(w, Gtk.SearchEntry) for w in fields))
                    if action == Gtk.FileChooserAction.SAVE:
                        self.assertTrue(any(w.get_text() == "Ein langer Dateiname.txt" for w in fields))
                    self.assertTrue(all(w.get_width_chars() <= 12 for w in fields))
                    files, = [w for w in children if isinstance(w, Gtk.ScrolledWindow)
                              and Gtk.Buildable.get_name(w) == "browse_files_swin"]
                    self.assertEqual(files.get_policy()[0], Gtk.PolicyType.AUTOMATIC)
                    self.assertLessEqual(dialog.get_preferred_width().minimum_width, 796)
                    if action == Gtk.FileChooserAction.OPEN:
                        chooser = next(w for w in children if isinstance(w, Gtk.FileChooserWidget))
                        chooser.emit("location-popup", "/")
                        loop = GLib.MainLoop()
                        GLib.timeout_add(350, loop.quit)
                        loop.run()
                        location = next(w for w in descendants(chooser)
                                        if isinstance(w, Gtk.Entry) and w.get_text() == "/")
                        self.assertLessEqual(location.get_width_chars(), 12)
                        self.assertLessEqual(dialog.get_preferred_width().minimum_width, 796)
                finally:
                    dialog.destroy()


if __name__ == "__main__":
    unittest.main()

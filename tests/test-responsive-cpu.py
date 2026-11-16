#!/usr/bin/env python3
"""Policy safety tests on temporary files only; never access CPU sysfs."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('responsive', Path(__file__).resolve().parents[1] / 'tools/trona-responsive-cpu.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class PolicyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.cpu = self.root / 'cpu'
        self.board = self.root / 'compatible'
        self.board.write_bytes(b'amazon,trona\0mediatek,mt8183\0')
        self.state = self.root / 'run/governors.json'
        for number in (0, 4):
            p = self.cpu / f'policy{number}'
            p.mkdir(parents=True)
            for name, value in {
                'scaling_driver': 'mtk-cpufreq', 'cpuinfo_min_freq': '793000',
                'cpuinfo_max_freq': '1248000', 'scaling_governor': 'schedutil',
                'scaling_available_governors': 'schedutil ondemand performance',
                'scaling_available_frequencies': '793000 910000 1014000 1131000 1248000',
            }.items():
                (p / name).write_text(value)

    def governors(self):
        return [(self.cpu / f'policy{n}/scaling_governor').read_text() for n in (0, 4)]

    def test_restore_and_idempotent_stop(self):
        m.apply(self.cpu, self.board, self.state)
        self.assertEqual(self.governors(), ['ondemand'] * 2)
        with self.assertRaises(RuntimeError):
            m.apply(self.cpu, self.board, self.state)
        m.restore(self.cpu, self.board, self.state)
        m.restore(self.cpu, self.board, self.state)
        self.assertEqual(self.governors(), ['schedutil'] * 2)

    def test_reject_unknown_board_or_second_policy_before_writes(self):
        self.board.write_bytes(b'amazon,other\0')
        with self.assertRaises(RuntimeError):
            m.apply(self.cpu, self.board, self.state)
        self.board.write_bytes(b'amazon,trona\0')
        (self.cpu / 'policy4/cpuinfo_max_freq').write_text('2000000')
        with self.assertRaises(RuntimeError):
            m.apply(self.cpu, self.board, self.state)
        self.assertEqual(self.governors(), ['schedutil'] * 2)
        self.assertFalse(self.state.exists())

    def test_second_write_failure_restores_first(self):
        original = Path.write_text
        def write(path, value, *args, **kwargs):
            if path == self.cpu / 'policy4/scaling_governor' and value == 'ondemand':
                raise OSError('simulated driver rejection')
            return original(path, value, *args, **kwargs)
        with patch.object(Path, 'write_text', write):
            with self.assertRaises(OSError):
                m.apply(self.cpu, self.board, self.state)
        self.assertEqual(self.governors(), ['schedutil'] * 2)
        self.assertFalse(self.state.exists())

    def test_preserves_later_external_policy_change(self):
        m.apply(self.cpu, self.board, self.state)
        (self.cpu / 'policy4/scaling_governor').write_text('performance')
        m.restore(self.cpu, self.board, self.state)
        self.assertEqual(self.governors(), ['schedutil', 'performance'])


if __name__ == '__main__':
    unittest.main()

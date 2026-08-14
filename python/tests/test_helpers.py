#!/usr/bin/env python3
"""Unit tests for arbuz plugin helpers (no running spreadsheet required)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

HOST = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HOST))

import arbuz  # noqa: E402


class Helpers(unittest.TestCase):
    def test_col_letter(self):
        self.assertEqual(arbuz.col_letter(0), "A")
        self.assertEqual(arbuz.col_letter(25), "Z")
        self.assertEqual(arbuz.col_letter(26), "AA")

    def test_a1(self):
        self.assertEqual(arbuz.a1(1, 1), "A1")
        self.assertEqual(arbuz.a1(5, 2), "B5")
        with self.assertRaises(ValueError):
            arbuz.a1(0, 1)

    def test_numbers(self):
        self.assertEqual(arbuz.numbers([1, "2", ["3", "x"], None, ""]), [1.0, 2.0, 3.0])

    def test_error(self):
        self.assertEqual(arbuz.error("#DIV/0!"), {"error": "#DIV/0!"})

    def test_unwrap(self):
        self.assertEqual(arbuz._unwrap_arg({"values": [1, 2]}), [1, 2])
        self.assertEqual(arbuz._unwrap_arg({"values": [7]}), 7)
        self.assertEqual(arbuz._unwrap_arg({"rows": [[1]]}), 1)
        self.assertEqual(arbuz._unwrap_arg({"rows": [[1, 2], [3, 4]]}), [[1, 2], [3, 4]])
        self.assertEqual(arbuz._unwrap_arg(7), 7)
        with self.assertRaises(ValueError):
            arbuz._unwrap_arg({"error": "#N/A"})


if __name__ == "__main__":
    unittest.main(verbosity=2)

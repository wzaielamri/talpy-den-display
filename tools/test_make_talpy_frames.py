"""Tests for make_talpy_frames's frame encoder (the format talpy_decode reads on the board).

    python3 -m unittest tools/test_make_talpy_frames.py
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_talpy_frames import SETS, encode_frame, placement, rgb565a8  # noqa: E402


class EncodeFrameTest(unittest.TestCase):
    def test_all_transparent_300_pixels(self):
        self.assertEqual(encode_frame([(0, 0)] * 300), bytes([0x7F, 0x7F, 0x2B]))  # 128 + 128 + 44

    def test_alpha_zero_drops_rgb(self):
        self.assertEqual(encode_frame([(0x1234, 0)]), bytes([0x00]))

    def test_repeat_split_at_64(self):
        self.assertEqual(encode_frame([(0x0001, 255)] * 70), bytes([0xBF, 1, 0, 255, 0x85, 1, 0, 255]))

    def test_literals_then_transparent(self):
        self.assertEqual(encode_frame([(1, 255), (2, 255), (0, 0)]), bytes([0xC1, 1, 0, 255, 2, 0, 255, 0x00]))


class Rgb565a8Test(unittest.TestCase):
    def test_rgb565a8_layout(self):
        self.assertEqual(rgb565a8([(0x1234, 0x80), (0, 0)]), bytes([0x34, 0x12, 0, 0, 0x80, 0]))


class PlacementTest(unittest.TestCase):
    def test_banner_at_origin(self):
        self.assertEqual(placement(SETS["banner"], (10, 20, 106, 124)), (0, 0))

    def test_weather_from_scene_box_corner(self):
        # the scene box's corner (-14, -22) is (26, 18) units into the SVG: 28.6, 19.8 px at 1.1 px per unit
        self.assertEqual(placement(SETS["weather"], (49, 48, 113, 113)), (20, 28))
        self.assertEqual(placement(SETS["weather"], (29, 20, 60, 60)), (0, 0))


if __name__ == "__main__":
    unittest.main()

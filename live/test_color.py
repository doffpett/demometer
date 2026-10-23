import unittest
import numpy as np
from PIL import Image
from live_color import sample_color, WIDTH, HEIGHT


class ColorTests(unittest.TestCase):
    def frame(self, color):
        return np.tile(np.array(color, dtype=np.uint8), (HEIGHT, WIDTH, 1))

    def test_primary_colors(self):
        for source, expected in [((255, 0, 0), (200, 0, 0)),
                                 ((0, 230, 0), (0, 200, 0)),
                                 ((0, 0, 230), (0, 0, 200))]:
            # Avoid the detector's intentionally rejected clipped highlights.
            source = tuple(min(x, 240) for x in source)
            self.assertEqual(sample_color(self.frame(source)), expected)

    def test_blue_can(self):
        red, green, blue = sample_color(self.frame((25, 122, 157)))
        self.assertEqual(red, 0)
        self.assertEqual(blue, 200)
        self.assertLess(green, 110)

    def test_neutral_dark_and_white_are_rejected(self):
        for color in [(0, 0, 0), (20, 20, 20), (128, 128, 128), (255, 255, 255)]:
            self.assertIsNone(sample_color(self.frame(color)))

    def test_color_outside_center_does_not_win(self):
        frame = self.frame((230, 0, 0))
        frame[45:200, 150:330] = (0, 0, 230)
        self.assertEqual(sample_color(frame), (0, 0, 200))

    def test_brightness_changes_keep_same_output(self):
        self.assertEqual(sample_color(self.frame((20, 100, 160))),
                         sample_color(self.frame((15, 75, 120))))

    def test_multicolored_ambiguous_scene_rejected(self):
        frame = self.frame((100, 100, 100))
        for a, b, color in [(158, 199, (230, 0, 0)), (199, 240, (0, 230, 0)),
                             (240, 281, (0, 0, 230)), (281, 322, (230, 230, 0))]:
            frame[59:189, a:b] = color
        self.assertIsNone(sample_color(frame))

    def test_can_above_blue_leds_and_table(self):
        frame = self.frame((150, 120, 80))
        frame[:120, 140:320] = (45, 120, 35)
        frame[130:, 200:280] = (0, 50, 230)
        red, green, blue = sample_color(frame)
        self.assertLess(red, 10)
        self.assertEqual(green, 200)
        self.assertEqual(blue, 0)

    def test_shaded_green_can_with_lettering_and_reflections(self):
        # Green paint, warm lettering and cyan reflections on a metal can.
        # No single hue has an absolute majority of the colored pixels.
        colors = [(65, 120, 35)] * 4 + [(110, 85, 60)] * 3 + [(35, 95, 100)] * 3
        frame = np.array(colors, dtype=np.uint8)[
            np.arange(HEIGHT * WIDTH).reshape(HEIGHT, WIDTH) % len(colors)]
        red, green, blue = sample_color(frame)
        self.assertGreater(green, red)
        self.assertGreater(green, blue)

    def test_red_wraps_around_hue_boundary(self):
        hsv = self.frame((2, 220, 180))
        hsv[:, ::2, 0] = 254
        frame = np.asarray(Image.fromarray(hsv, mode="HSV").convert("RGB"))
        red, green, blue = sample_color(frame)
        self.assertEqual(red, 200)
        self.assertLess(green, 40)
        self.assertLess(blue, 40)


if __name__ == "__main__":
    unittest.main()

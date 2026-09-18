import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionVisualPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )

    def test_v3_layout_is_installed(self):
        self.assertIn("PROGRESSION LAYOUT V3", self.src)

    def test_global_headers_use_absolute_ui_at(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertIn('ui_at(-4.75f, -11.55f, .0180f', draw)
        self.assertIn('ui_at(-4.90f, -9.72f, .0190f', draw)
        self.assertNotIn("text_group(", draw)

    def test_native_stage_title_is_fully_covered(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertIn("ui_panel(-20.0f, -10.45f, 40.0f, 2.75f", draw)

    def test_fight_panels_are_compact(self):
        fight = self.src.split("static void draw_fight_plate", 1)[1]
        fight = fight.split("static void draw_bottom_bar", 1)[0]
        self.assertIn("ui_panel(x, 5.02f, 14.12f, 2.48f", fight)

    def test_build_strip_has_four_absolute_ability_slots(self):
        bar = self.src.split("static void draw_bottom_bar", 1)[1]
        bar = bar.split("static void draw_build", 1)[0]
        self.assertIn('ui_at(-14.25f, 10.18f', bar)
        self.assertIn('ui_at(.55f, 10.18f', bar)
        self.assertIn('ui_at(-14.25f, 11.05f', bar)
        self.assertIn('ui_at(.55f, 11.05f', bar)
        self.assertIn("ROGUE_ABILITY_DOWN", bar)

    def test_no_custom_vs_box(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertNotIn("draw_center_vs", draw)


if __name__ == "__main__":
    unittest.main()

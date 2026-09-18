import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionVisualPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )

    def test_clean_target_render_pass_is_installed(self):
        self.assertIn("CLEAN / READABLE TARGET-RENDER PASS", self.src)
        self.assertIn("Peak: ~22 Rogue SIS objects", self.src)

    def test_full_top_backdrop_hides_native_route_clutter(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertIn("ui_panel(-20.0f, -15.0f, 40.0f, 4.65f", draw)

    def test_cards_use_panel_plus_readable_text_group(self):
        card = self.src.split("static void draw_reward_card", 1)[1]
        card = card.split("static void draw_fight_plate", 1)[0]
        self.assertEqual(card.count("ui_panel("), 1)
        self.assertIn("text_group(", card)
        self.assertIn("selected ? ui_dark : ui_glass", card)

    def test_fight_plates_are_thin_lower_thirds(self):
        fight = self.src.split("static void draw_fight_plate", 1)[1]
        fight = fight.split("static void draw_bottom_bar", 1)[0]
        self.assertIn("ui_panel(x, 4.92f, 14.15f, 2.72f", fight)
        self.assertIn("text_group(", fight)

    def test_build_strip_always_has_all_four_specials(self):
        bar = self.src.split("static void draw_bottom_bar", 1)[1]
        bar = bar.split("static void draw_build", 1)[0]
        self.assertIn('"N  %.14s', bar)
        self.assertIn('"U  %.14s', bar)
        self.assertIn("ROGUE_ABILITY_DOWN", bar)

    def test_native_vs_art_is_not_covered_by_custom_vs_box(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertNotIn("draw_center_vs", draw)


if __name__ == "__main__":
    unittest.main()

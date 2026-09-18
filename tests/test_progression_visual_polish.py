import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionVisualPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )

    def test_safe_polish_pass_is_installed(self):
        self.assertIn("SAFE TARGET-RENDER POLISH", self.src)
        self.assertIn("Normal maximum: ~10 Rogue HSD_Text objects", self.src)

    def test_cards_are_single_sis_objects(self):
        card = self.src.split("static void draw_reward_card", 1)[1]
        card = card.split("static void draw_fight_plate", 1)[0]
        self.assertEqual(card.count("ui_panel("), 1)
        self.assertIn("panel_entry(", card)
        self.assertNotIn("draw_panel_frame", card)

    def test_fight_plates_are_single_sis_objects(self):
        fight = self.src.split("static void draw_fight_plate", 1)[1]
        fight = fight.split("static void draw_center_vs", 1)[0]
        self.assertEqual(fight.count("ui_panel("), 1)
        self.assertIn("ui_blue", fight)
        self.assertIn("ui_purple", fight)

    def test_bottom_bar_is_single_sis_object(self):
        bar = self.src.split("static void draw_bottom_bar", 1)[1]
        bar = bar.split("static void draw_build", 1)[0]
        self.assertEqual(bar.count("ui_panel("), 1)
        self.assertIn('"CURRENT CHARACTER BUILD / UPGRADES"', bar)
        self.assertIn("panel_entry(", bar)

    def test_phase_and_route_are_compact(self):
        self.assertIn("static void draw_route_header", self.src)
        self.assertIn("static void draw_phase_bar", self.src)
        self.assertIn('"CHOOSE UPGRADE"', self.src)
        self.assertIn('"CHOOSE NEXT FIGHT"', self.src)

    def test_upgrade_cards_still_disappear_after_selection(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertIn("has_reward && !upgrade_chosen", draw)


if __name__ == "__main__":
    unittest.main()

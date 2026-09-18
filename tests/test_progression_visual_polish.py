import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionVisualPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )

    def test_balanced_polish_pass_is_installed(self):
        self.assertIn("BALANCED TARGET-RENDER PASS", self.src)
        self.assertIn("Peak reward-screen budget is about 19 Rogue objects", self.src)

    def test_route_tabs_are_screen_positioned_objects(self):
        self.assertIn("static void draw_node", self.src)
        node = self.src.split("static void draw_node", 1)[1]
        node = node.split("static const char* reward_icon_letter", 1)[0]
        self.assertIn("ui_panel(x, -13.55f", node)

    def test_cards_remain_single_objects(self):
        card = self.src.split("static void draw_reward_card", 1)[1]
        card = card.split("static void draw_fight_plate", 1)[0]
        self.assertEqual(card.count("ui_panel("), 1)
        self.assertIn("panel_entry(", card)

    def test_bottom_text_uses_screen_positions(self):
        bar = self.src.split("static void draw_bottom_bar", 1)[1]
        bar = bar.split("static void draw_build", 1)[0]
        self.assertIn('ui_at(-9.55f, 8.78f, .0200f', bar)
        self.assertIn('"CURRENT CHARACTER BUILD / UPGRADES"', bar)

    def test_no_custom_center_vs_box(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertNotIn("draw_center_vs", draw)

    def test_cards_still_disappear_after_upgrade(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertIn("has_reward && !upgrade_chosen", draw)
        self.assertIn('"UPGRADE LOCKED: %s"', draw)


if __name__ == "__main__":
    unittest.main()

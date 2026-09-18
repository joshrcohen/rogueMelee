import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionVisualPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )

    def test_polish_pass_is_installed(self):
        self.assertIn("TARGET-RENDER POLISH PASS", self.src)

    def test_selected_cards_use_frame_not_solid_yellow_fill(self):
        card = self.src.split("static void draw_reward_card", 1)[1]
        card = card.split("static void draw_fight_plate", 1)[0]
        self.assertIn("draw_panel_frame", card)
        self.assertIn("selected ? ui_gold : ui_border", card)
        self.assertNotIn("GXColor bg = selected ? ui_gold", card)

    def test_fight_plates_have_side_accents(self):
        fight = self.src.split("static void draw_fight_plate", 1)[1]
        fight = fight.split("static void draw_center_vs", 1)[0]
        self.assertIn("ui_blue", fight)
        self.assertIn("ui_purple", fight)
        self.assertIn("RogueRoute_TypeName", fight)

    def test_build_bar_uses_large_standalone_text(self):
        bar = self.src.split("static void draw_bottom_bar", 1)[1]
        bar = bar.split("static void draw_controls", 1)[0]
        self.assertIn('"CURRENT CHARACTER BUILD / UPGRADES"', bar)
        self.assertIn(".0208f", bar)
        self.assertIn("ui_at(", bar)

    def test_phase_bar_covers_native_stage_label(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertIn('ui_panel(-20.0f, -9.45f, 40.0f, 1.25f', draw)
        self.assertIn('"CHOOSE UPGRADE"', draw)
        self.assertIn('"CHOOSE NEXT FIGHT"', draw)

    def test_upgrade_cards_still_disappear_after_selection(self):
        draw = self.src.split("static void draw_progression", 1)[1]
        draw = draw.split("static int copy_encounter", 1)[0]
        self.assertIn("has_reward && !upgrade_chosen", draw)


if __name__ == "__main__":
    unittest.main()

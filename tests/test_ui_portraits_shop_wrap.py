import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class UiPortraitShopWrapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.rewards = (ROOT / "mod/rogue_rewards.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_native_character_images(self):
        self.assertIn("ifStock_802F96D0", self.ui)
        self.assertIn("ui_fighter_icon(g_rogue_run.player_kind", self.ui)
        self.assertIn("ui_fighter_icon(encounter->enemy_kind", self.ui)

    def test_reward_wrapping(self):
        self.assertIn("static void ui_wrapped_at", self.ui)
        # Native GmRegClr has a smaller SPECIAL BONUS text region than the
        # old full-screen Rogue reward panel.
        self.assertIn("detail, 44, 4", self.ui)
        self.assertIn('"CHOOSE UPGRADE"', self.ui)

    def test_shop_visible(self):
        self.assertIn('"SHOP"', self.ui)
        self.assertIn('"AFTER MATCH 4: SHOP / REST AREA"', self.ui)

    def test_shop_before_boss(self):
        self.assertIn("act_floor != ROGUE_FLOORS_PER_ACT", self.rewards)

    def test_intro_cleanup(self):
        self.assertIn("exitStageIntro", self.rogue)
        self.assertIn("enterStageIntro, exitStageIntro", self.rogue)

if __name__ == "__main__":
    unittest.main()

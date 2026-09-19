import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class UiPortraitShopWrapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(encoding="utf-8")
        cls.rewards = (ROOT / "mod/rogue_rewards.c").read_text(encoding="utf-8")
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")

    def test_progression_uses_demo_fighters_not_stock_icons(self):
        self.assertNotIn("ifStock_802F96D0", self.progress)
        self.assertIn("g_rogue_progression_intro.allies", self.progress)
        self.assertIn("g_rogue_progression_intro.enemies", self.progress)

    def test_reward_cards_keep_description_text(self):
        self.assertIn("Rogue_DescribeReward", self.progress)
        self.assertIn("split_description", self.progress)

    def test_build_strip_has_all_abilities_and_stats(self):
        self.assertIn("ability_name(ROGUE_ABILITY_NEUTRAL)", self.progress)
        self.assertIn("ability_name(ROGUE_ABILITY_SIDE)", self.progress)
        self.assertIn("ability_name(ROGUE_ABILITY_UP)", self.progress)
        self.assertIn("ability_name(ROGUE_ABILITY_DOWN)", self.progress)
        self.assertIn('"CURRENT CHARACTER BUILD / UPGRADES"', self.progress)
        self.assertIn('"DAMAGE %.0f%%       DEFENSE %.0f%%"', self.progress)

    def test_shop_before_boss(self):
        self.assertIn("act_floor != ROGUE_FLOORS_PER_ACT", self.rewards)

    def test_existing_shop_ui_is_untouched(self):
        self.assertIn('"SHOP"', self.ui)
        self.assertIn('"AFTER MATCH 4: SHOP / REST AREA"', self.ui)


if __name__ == "__main__":
    unittest.main()

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeStageClearRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.build = (ROOT / "tools/build.py").read_text(encoding="utf-8")

    def test_reward_text_reuses_native_regclear_canvas(self):
        section = self.ui.split("void RogueUI_OpenResults(void)", 1)[1]
        section = section.split("int RogueUI_Frame(void)", 1)[0]
        self.assertIn("RogueUI_Clear();", section)
        self.assertIn("overlay_sis = 0;", section)
        self.assertIn("overlay_canvas = 0;", section)
        self.assertIn("ready = true;", section)
        self.assertNotIn("openCanvas();", section)

    def test_left_side_is_upgrade_picker(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertIn('"CHOOSE UPGRADE"', section)
        self.assertIn('"UPGRADE DETAILS"', section)
        self.assertIn("g_rogue_run.current_rewards[i]", section)
        self.assertIn("Rogue_DescribeReward", section)

    def test_left_side_shows_currency(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertIn('"GOLD"', section)
        self.assertIn('"TOTAL"', section)
        self.assertIn('"+%d", earned', section)

    def test_right_side_is_left_to_vanilla_melee(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertNotIn('"REWARD DETAILS"', section)
        self.assertNotIn('"SPECIAL BONUS"', section)
        self.assertNotIn('"RUN FLOOR"', section)
        self.assertNotIn(
            "postpatch_native_stage_clear_rewards.py",
            self.build,
        )


if __name__ == "__main__":
    unittest.main()

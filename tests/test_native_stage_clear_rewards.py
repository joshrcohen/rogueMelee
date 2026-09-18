import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeStageClearRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.build = (ROOT / "tools/build.py").read_text(encoding="utf-8")
        cls.fixer = (
            ROOT / "tools/postpatch_native_stage_clear_rewards.py"
        ).read_text(encoding="utf-8")

    def test_reward_text_reuses_native_regclear_canvas(self):
        section = self.ui.split("void RogueUI_OpenResults(void)", 1)[1]
        section = section.split("int RogueUI_Frame(void)", 1)[0]
        self.assertIn("RogueUI_Clear();", section)
        self.assertIn("overlay_sis = 0;", section)
        self.assertIn("overlay_canvas = 0;", section)
        self.assertIn("ready = true;", section)
        self.assertNotIn("openCanvas();", section)

    def test_native_result_boxes_show_currency(self):
        self.assertIn('"GOLD"', self.ui)
        self.assertIn('"TOTAL"', self.ui)
        self.assertIn('"+%d", earned', self.ui)

    def test_special_bonus_panel_is_upgrade_picker(self):
        self.assertIn('"CHOOSE UPGRADE"', self.ui)
        self.assertIn("g_rogue_run.current_rewards[i]", self.ui)
        self.assertIn("Rogue_DescribeReward", self.ui)
        self.assertIn(
            '"A / START: TAKE    B: BUILD"',
            self.ui,
        )

    def test_vanilla_bonus_text_is_suppressed_only_for_rogue(self):
        self.assertIn(
            "gm_GetCurrentGameMode() != GM_ROGUE",
            self.fixer,
        )

    def test_stage_clear_postpatch_is_in_build_digest(self):
        self.assertIn(
            "postpatch_native_stage_clear_rewards.py",
            self.build,
        )


if __name__ == "__main__":
    unittest.main()

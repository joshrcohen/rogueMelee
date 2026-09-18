import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeStageClearRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(encoding="utf-8")

    def test_stage_clear_is_not_progression_host(self):
        post = self.rogue.split("bool Rogue_PostFight(void)", 1)[1]
        post = post.split("static void enterGameOver", 1)[0]
        reward = post.split("if (g_rogue_run.phase == ROGUE_PHASE_REWARD)", 1)[1]
        self.assertIn("destination = ROGUE_STATE_PROGRESSION;", reward)
        self.assertIn("return false;", reward)

    def test_stage_clear_score_is_still_vanilla(self):
        self.assertIn("start->rules.x4_4 = true;", self.rogue)
        self.assertIn("start->rules.x18", self.rogue)

    def test_reward_is_selected_in_progression_scene(self):
        self.assertIn("Rogue_SelectReward(chosen)", self.progress)
        self.assertIn("upgrade_chosen = true;", self.progress)

    def test_boss_floor_preserves_shop_before_boss(self):
        self.assertIn("if (Rogue_BeginCamp())", self.progress)
        self.assertIn("gm_SetNextGameModeStateId(3);", self.progress)


if __name__ == "__main__":
    unittest.main()

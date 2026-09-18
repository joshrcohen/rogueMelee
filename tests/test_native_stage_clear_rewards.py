import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeStageClearRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_stage_clear_stays_vanilla(self):
        self.assertIn("start->rules.x4_4 = true;", self.rogue)
        self.assertIn("start->rules.x18", self.rogue)
        self.assertNotIn("static void drawStageClear(void)", self.ui)

    def test_postfight_reward_goes_to_existing_route_state(self):
        post = self.rogue.split("bool Rogue_PostFight(void)", 1)[1]
        post = post.split("static void enterGameOver", 1)[0]
        self.assertIn("g_rogue_run.phase == ROGUE_PHASE_REWARD", post)
        self.assertIn("destination = 4;", post)

    def test_route_contains_three_reward_boxes(self):
        route = self.ui.split("/* Rogue Bracket:", 1)[1].split(
            "/* Camp signs are projected", 1
        )[0]
        self.assertIn("for (i = 0; i < 3; ++i)", route)
        self.assertIn("routeRewardBox", route)
        self.assertIn('"CHOOSE UPGRADE"', route)
        self.assertIn('"CHOOSE AN UPGRADE FIRST"', route)

    def test_reward_selection_exits_before_mutating_route(self):
        route = self.ui.split("int RogueUI_RouteFrame(void)", 1)[1].split(
            "/* Camp signs are projected", 1
        )[0]
        self.assertNotIn("Rogue_SelectReward(route_reward_cursor)", route)
        self.assertIn(
            "return ROGUE_UI_ROUTE_REWARD_BASE + route_reward_cursor;",
            route,
        )

        frame = self.rogue.split("static void routeFrame(void)", 1)[1].split(
            "static void enterRoute", 1
        )[0]
        self.assertIn("route_reward_choice =", frame)
        self.assertIn("gm_8016B328();", frame)

        exit_route = self.rogue.split("static void exitRoute", 1)[1].split(
            "static void encounterFrame", 1
        )[0]
        self.assertIn("Rogue_SelectReward(reward_choice)", exit_route)
        self.assertIn("gm_SetNextGameModeStateId(4);", exit_route)


if __name__ == "__main__":
    unittest.main()

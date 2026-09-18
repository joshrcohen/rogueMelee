import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeStageClearRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_progression_reuses_native_regclear_canvas(self):
        section = self.ui.split("void RogueUI_OpenResults(void)", 1)[1]
        section = section.split("int RogueUI_Frame(void)", 1)[0]
        self.assertIn("RogueUI_Clear();", section)
        self.assertIn("overlay_sis = 0;", section)
        self.assertIn("overlay_canvas = 0;", section)
        self.assertIn("stage_progress_active", section)
        self.assertNotIn("openCanvas();", section)

    def test_popup_contains_requested_sections(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertIn('"ROGUE PROGRESSION"', section)
        self.assertIn('"CHOOSE UPGRADE"', section)
        self.assertIn('"CHOOSE NEXT FIGHT"', section)
        self.assertIn('"RUN STATS"', section)

    def test_popup_uses_text_only_on_native_result_canvas(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertNotIn("ui_panel_box(", section)
        self.assertNotIn("ui_fighter_icon(", section)
        self.assertNotIn("ui_backdrop(", section)
        self.assertIn("ui_at(", section)

    def test_upgrade_then_fight_is_two_step_input(self):
        frame = self.ui.split("int RogueUI_Frame(void)", 1)[1].split(
            "/* Rogue Bracket:", 1
        )[0]
        reward_pos = frame.index("Rogue_SelectReward(chosen)")
        fight_pos = frame.index("RogueRoute_Select(&g_rogue_run.route,")
        self.assertLess(reward_pos, fight_pos)
        self.assertIn("stage_upgrade_chosen = true;", frame)
        self.assertIn("stage_route_cursor ^= 1;", frame)

    def test_next_route_is_ready_before_popup(self):
        post = self.rogue.split("bool Rogue_PostFight(void)", 1)[1].split(
            "static void enterGameOver", 1
        )[0]
        prepare = post.index("RogueRoute_Prepare(&g_rogue_run.route,")
        open_results = post.index("RogueUI_OpenResults();")
        self.assertLess(prepare, open_results)

    def test_route_phase_does_not_end_stage_clear_hook(self):
        post = self.rogue.split("bool Rogue_PostFight(void)", 1)[1].split(
            "static void enterGameOver", 1
        )[0]
        self.assertNotIn(
            "g_rogue_run.phase == ROGUE_PHASE_ROUTE ||",
            post,
        )

    def test_boss_floor_continues_to_existing_camp(self):
        frame = self.ui.split("int RogueUI_Frame(void)", 1)[1].split(
            "/* Rogue Bracket:", 1
        )[0]
        self.assertIn(
            "g_rogue_run.phase == ROGUE_PHASE_ENCOUNTER",
            frame,
        )
        self.assertIn("return ROGUE_UI_CONTINUE;", frame)


if __name__ == "__main__":
    unittest.main()

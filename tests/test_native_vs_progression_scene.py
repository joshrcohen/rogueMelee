import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeVsProgressionSceneTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(encoding="utf-8")
        cls.header = (ROOT / "mod/rogue_progression.h").read_text(encoding="utf-8")
        cls.fix = (
            ROOT / "tools/postpatch_ability_native_rogue_flow.py"
        ).read_text(encoding="utf-8")

    def test_state_id_is_dedicated(self):
        self.assertIn("#define ROGUE_STATE_PROGRESSION 7", self.header)

    def test_native_intro_is_held_only_by_rogue_progression(self):
        self.assertIn("if (Rogue_ProgressionIntroFrame())", self.fix)
        self.assertIn("return;", self.fix)

    def test_frame_guards_mode_and_state(self):
        frame = self.progress.split("bool Rogue_ProgressionIntroFrame(void)", 1)[1]
        self.assertIn("gm_GetCurrentGameMode() != GM_ROGUE", frame)
        self.assertIn("gm_GetCurrentSceneIndex() != ROGUE_STATE_PROGRESSION", frame)

    def test_preload_covers_both_route_teams(self):
        self.assertIn("intro->ally_count", self.progress)
        self.assertIn("intro->enemy_count", self.progress)
        self.assertIn("lbDvd_80018C2C(0xC7);", self.progress)

    def test_progression_gets_results_sized_sis_arena(self):
        self.assertIn('root / "src" / "melee" / "gm" / "gm_1A3F.c"', self.fix)
        self.assertIn("gm_GetCurrentGameMode() == GM_ROGUE && state->id == 7", self.fix)
        self.assertIn("HSD_SisLib_803A6048(0xC000);", self.fix)

    def test_progression_reuses_native_classic_road_map(self):
        self.assertIn("style the retail IrRdMap", self.fix)
        self.assertIn("rogue_map_scale.x *= 0.78f;", self.fix)
        self.assertIn("rogue_map_pos.y += 6.5f;", self.fix)
        self.assertIn("ROGUE_STATE_PROGRESSION", self.fix)

    def test_master_hand_is_never_passed_to_intro_easy(self):
        self.assertIn("CKind_MasterH", self.progress)
        self.assertIn("CKind_CrezyH", self.progress)
        self.assertIn("Retail GS_INTRO_EASY explicitly aborts", self.progress)

    def test_scene_exits_to_real_matchup_intro(self):
        self.assertIn("gm_SetNextGameModeStateId(next_intro_state());", self.progress)


if __name__ == "__main__":
    unittest.main()

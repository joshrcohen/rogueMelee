import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeMatchupIntroTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_real_classic_scene_is_used(self):
        self.assertIn("{ GS_INTRO_EASY, &stage_intro, NULL }", self.rogue)
        self.assertIn(
            "1, lbDvdPreload_3, 0, enterStageIntro, exitStageIntro",
            self.rogue,
        )

    def test_payload_matches_classic_intro_layout(self):
        for token in (
            "typedef struct RogueIntroData",
            "s32 model_scale_kind;",
            "s32 game_type;",
            "u8 port;",
            "u8 nametag;",
            "u8 stage_number;",
            "u8 ally_count;",
            "u8 enemy_count;",
            "u8 allies[3];",
            "u8 enemies[3];",
            "u8 ally_costumes[3];",
            "u8 enemy_costumes[3];",
            "u8 ally_flags[3];",
            "u8 enemy_flags[3];",
        ):
            self.assertIn(token, self.rogue)

    def test_current_rogue_fighters_feed_native_intro(self):
        self.assertIn(
            "stage_intro.allies[0] = g_rogue_run.player_kind",
            self.rogue,
        )
        self.assertIn(
            "stage_intro.ally_costumes[0] = g_rogue_run.player_costume",
            self.rogue,
        )
        self.assertIn(
            "stage_intro.enemies[i] = encounter->enemies[i].kind",
            self.rogue,
        )
        self.assertIn(
            "stage_intro.enemy_costumes[i] = encounter->enemies[i].costume",
            self.rogue,
        )

    def test_native_special_presentations_are_selected(self):
        self.assertIn("stage_intro.model_scale_kind = 4;", self.rogue)
        self.assertIn("stage_intro.model_scale_kind = 1;", self.rogue)
        self.assertIn("stage_intro.model_scale_kind = 2;", self.rogue)
        self.assertIn(
            "stage_intro.enemy_flags[i] = encounter->enemies[i].metal ? 1 : 0;",
            self.rogue,
        )

    def test_rogue_stage_and_audio_are_preloaded(self):
        self.assertIn(
            "stage_intro.stage_number = (u8) g_rogue_run.floor",
            self.rogue,
        )
        self.assertIn("gc->stkind = encounter->stage;", self.rogue)
        self.assertIn("lbAudioAx_80026EBC(encounter->stage)", self.rogue)

    def test_hand_bosses_use_safe_adventure_fallback(self):
        self.assertIn("kind == CKind_MasterH || kind == CKind_CrezyH", self.rogue)
        self.assertIn(
            "{ GS_INTRO_NORMAL, &boss_stage_intro, NULL }",
            self.rogue,
        )
        self.assertIn("gm_SetNextGameModeStateId(2);", self.rogue)

    def test_route_preview_replaces_duplicate_normal_intro(self):
        self.assertIn(
            "gm_SetNextGameModeStateId(Rogue_BeginCamp() ? 3 : 2);",
            self.rogue,
        )
        self.assertIn(
            "gm_SetNextGameModeStateId(Rogue_IntroState());",
            self.rogue,
        )


if __name__ == "__main__":
    unittest.main()

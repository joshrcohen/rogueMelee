import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionFlowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.fixer = (
            ROOT / "tools/postpatch_ability_native_rogue_flow.py"
        ).read_text(encoding="utf-8")

    def test_css_enters_progression_before_first_fight(self):
        css_exit = self.rogue.split(
            "static void exitCharacterSelect", 1
        )[1].split("static void enterStageIntro", 1)[0]
        self.assertIn("gm_SetNextGameModeStateId(4);", css_exit)

    def test_progression_is_non_gameplay_scene(self):
        self.assertIn("{ GS_TOU_BRACKET, NULL, NULL }", self.rogue)
        self.assertNotIn("GS_INTRO_EASY, &route_intro", self.rogue)
        self.assertNotIn("gm_GetCurrentSceneIndex() == 4", self.fixer)

    def test_progression_contains_all_four_sections(self):
        for token in (
            '"ROGUE ROUTE"',
            '"UPGRADES"',
            '"CHOOSE UPGRADE"',
            '"CHOOSE NEXT MATCH     LEFT / RIGHT"',
            '"GOLD GAINED"',
            '"TOTAL GOLD"',
            '"TOTAL SCORE"',
        ):
            self.assertIn(token, self.ui)

    def test_progression_does_not_boot_battle_hud_for_portraits(self):
        route_matchup = self.ui.split(
            "static void routeDrawMatchup", 1
        )[1].split("static void routeDrawFightChoices", 1)[0]
        self.assertNotRegex(
            route_matchup,
            r"(?m)^\s*ui_fighter_icon\s*\(",
        )
        self.assertNotIn("ifAll_802F390C", route_matchup)
        self.assertIn("SIS-ONLY MATCHUP PREVIEW", route_matchup)

    def test_route_choice_redraws_without_scene_reload(self):
        frame = self.ui.split("int RogueUI_RouteFrame(void)", 1)[1]
        frame = frame.split("/* Camp signs", 1)[0]
        movement = frame.split(
            "if (input & (MenuInput_Left | MenuInput_Right |", 1
        )[1]
        self.assertIn("route_cursor ^= 1;", movement)
        self.assertIn("routeDraw();", movement)
        self.assertNotIn("route_refresh_requested = true;", movement)

    def test_shop_returns_to_progression_then_boss(self):
        camp_exit = self.rogue.split(
            "static void exitCamp", 1
        )[1].split("static void enterRouteMenu", 1)[0]
        self.assertIn("gm_SetNextGameModeStateId(4);", camp_exit)
        self.assertIn(
            "g_rogue_run.current_encounter.type == ROGUE_ENCOUNTER_BOSS",
            self.ui,
        )
        self.assertIn("ROUTE_UI_BOSS", self.ui)

    def test_match4_reward_enters_shop_before_boss_confirmation(self):
        frame = self.ui.split("int RogueUI_RouteFrame(void)", 1)[1]
        frame = frame.split("/* Camp signs", 1)[0]
        self.assertIn("return ROGUE_ROUTE_CHOICES;", frame)
        menu = self.rogue.split(
            "void Rogue_RouteMenuSceneFrame(void)", 1
        )[1].split("static void encounterFrame", 1)[0]
        self.assertIn("Rogue_BeginCamp() ? 3 : 2", menu)


if __name__ == "__main__":
    unittest.main()

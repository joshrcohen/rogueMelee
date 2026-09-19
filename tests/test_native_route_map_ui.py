import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeRouteMapUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )
        cls.fix = (
            ROOT / "tools/postpatch_ability_native_rogue_flow.py"
        ).read_text(encoding="utf-8")

    def test_v8_draws_rogue_specific_six_node_route(self):
        self.assertIn("PROGRESSION V8: ROGUE ROUTE + CLEAN LAYOUT", self.progress)
        self.assertIn("static void draw_route_map(HSD_Text* text)", self.progress)
        self.assertIn('"CLEAR", "NEXT", "ELITE", "MATCH 4", "SHOP", "BOSS"', self.progress)
        self.assertIn('i == 3 ? "[]" : "O"', self.progress)

    def test_route_has_no_background_overlay(self):
        self.assertNotIn("ui_route_glass", self.progress)
        base = self.progress.split("static void draw_base_panels(void)", 1)[1]
        base = base.split("static void draw_reward_card_panels", 1)[0]
        self.assertIn("draw NOTHING behind the route", base)

    def test_stock_classic_map_transform_code_is_removed(self):
        self.assertNotIn("rogue_map_scale", self.fix)
        self.assertNotIn("rogue_map_pos", self.fix)
        self.assertIn("HSD_JObjSetFlagsAll(jobj, JOBJ_HIDDEN);", self.fix)

    def test_reward_descriptions_are_compact(self):
        self.assertIn("wrap_description3", self.progress)
        self.assertIn('" Other slots stay equipped."', self.progress)
        self.assertIn("211.0f", self.progress)

    def test_v8_avoids_unavailable_msl_strstr(self):
        self.assertIn("static void trim_suffix(", self.progress)
        self.assertIn("strcmp(text + text_len - suffix_len, suffix)", self.progress)
        self.assertNotIn("strstr(", self.progress)

    def test_fight_lower_thirds_are_shorter_and_higher(self):
        self.assertIn("22.0f, 286.0f, 282.0f, 50.0f", self.progress)
        self.assertIn("336.0f, 286.0f, 282.0f, 50.0f", self.progress)
        fight = self.progress.split("static void draw_fight_text", 1)[1]
        fight = fight.split("static void draw_build_strip_text", 1)[0]
        self.assertNotIn("PICK UPGRADE FIRST", fight)

    def test_main_hud_keeps_detailed_stats_in_build_screen(self):
        strip = self.progress.split("static void draw_build_strip_text", 1)[1]
        strip = strip.split("static void draw_full_build_text", 1)[0]
        self.assertNotIn("DMG %.0f", strip)
        self.assertNotIn("DEF %.0f", strip)
        self.assertIn("SCORE %d", strip)


if __name__ == "__main__":
    unittest.main()

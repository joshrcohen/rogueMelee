import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeRouteMapUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(encoding="utf-8")
        cls.fix = (ROOT / "tools/postpatch_ability_native_rogue_flow.py").read_text(encoding="utf-8")

    def test_v7_uses_native_route_map_visual_language(self):
        self.assertIn("PROGRESSION V7: NATIVE ROAD MAP POLISH", self.progress)
        self.assertIn("IrRdMap supplies the actual Melee line", self.progress)
        self.assertIn("style the retail IrRdMap", self.fix)

    def test_route_text_does_not_draw_connecting_arrows(self):
        block = self.progress.split("static void draw_route_text(HSD_Text* text)", 1)[1]
        block = block.split("static void draw_phase_text", 1)[0]
        self.assertNotIn('&ui_muted, ">"', block)

    def test_native_map_resets_per_act(self):
        self.assertIn("stage_number = (u8) target_act_floor;", self.progress)

    def test_reward_descriptions_wrap_to_three_lines(self):
        self.assertIn("wrap_description3", self.progress)
        self.assertIn("char line3[64];", self.progress)

    def test_long_text_gets_adaptive_scale(self):
        self.assertIn("fit_text_scale", self.progress)
        self.assertIn("name_scale = fit_text_scale", self.progress)
        self.assertIn("title_scale = fit_text_scale", self.progress)

    def test_fight_lower_thirds_are_raised(self):
        self.assertIn("22.0f, 302.0f, 282.0f, 58.0f", self.progress)
        self.assertIn("336.0f, 302.0f, 282.0f, 58.0f", self.progress)


if __name__ == "__main__":
    unittest.main()

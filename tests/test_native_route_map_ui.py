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

    def test_v10_route_is_native_assets_only(self):
        self.assertIn("PROGRESSION V10: CLEAN NATIVE ROUTE", self.progress)
        block = self.progress.split(
            "static void draw_route_map(HSD_Text* text)", 1
        )[1].split("static void draw_phase_text", 1)[0]

        self.assertIn('"ACT %d  -  FLOOR %d"', block)
        self.assertNotIn('"------------"', block)
        self.assertNotIn('"[]"', block)
        self.assertNotIn('"O"', block)
        self.assertNotIn('"V"', block)
        self.assertNotIn('"CLEAR"', block)
        self.assertNotIn('"NEXT"', block)
        self.assertNotIn('"ELITE"', block)
        self.assertNotIn('"MATCH 4"', block)
        self.assertNotIn('"SHOP"', block)
        self.assertNotIn('"BOSS"', block)

    def test_native_route_model_is_not_hidden_or_transformed(self):
        self.assertNotIn(
            "HSD_JObjSetFlagsAll(jobj, JOBJ_HIDDEN);", self.fix
        )
        self.assertNotIn("rogue_map_scale", self.fix)
        self.assertNotIn("rogue_map_pos", self.fix)
        self.assertIn(
            "native retail route map left untouched", self.fix
        )

    def test_native_map_uses_act_local_stage_number(self):
        self.assertIn(
            "stage_number = (u8) target_act_floor;", self.progress
        )

    def test_reward_readability_helpers_remain(self):
        self.assertIn("wrap_description3", self.progress)
        self.assertIn("fit_text_scale", self.progress)
        self.assertIn("trim_suffix", self.progress)


if __name__ == "__main__":
    unittest.main()

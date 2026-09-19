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

    def test_v11_route_uses_native_assets_only(self):
        self.assertIn(
            "PROGRESSION V12: COMPACT HUD + MINIMAL ENCOUNTER TEXT",
            self.progress,
        )
        block = self.progress.split(
            "static void draw_route_map(HSD_Text* text)", 1
        )[1].split("static void draw_phase_text", 1)[0]

        self.assertIn('"ACT %d/%d     FLOOR %d/%d"', block)
        self.assertIn("ROGUE_ACTS", block)
        self.assertIn("ROGUE_RUN_ENCOUNTERS", block)
        self.assertNotIn('"------------"', block)
        self.assertNotIn('"[]"', block)
        self.assertNotIn('"O"', block)
        self.assertNotIn('"V"', block)

    def test_header_starts_below_native_route(self):
        base = self.progress.split(
            "static void draw_base_panels(void)", 1
        )[1].split("static void draw_reward_card_panels", 1)[0]
        self.assertIn("0.0f, 96.0f, 640.0f, 48.0f", base)

    def test_native_map_tracks_five_floors_per_act(self):
        self.assertIn(
            "stage_number = (u8) target_act_floor;", self.progress
        )
        self.assertIn(
            "Three acts x five floors = the full 15-floor Rogue run.",
            self.progress,
        )

    def test_native_route_model_remains_unmodified(self):
        self.assertNotIn(
            "HSD_JObjSetFlagsAll(jobj, JOBJ_HIDDEN);", self.fix
        )
        self.assertNotIn("rogue_map_scale", self.fix)
        self.assertNotIn("rogue_map_pos", self.fix)

    def test_reward_copy_is_kept_inside_cards(self):
        self.assertIn("wrap_description3", self.progress)
        self.assertIn("19);", self.progress)
        self.assertIn("231.0f", self.progress)


if __name__ == "__main__":
    unittest.main()

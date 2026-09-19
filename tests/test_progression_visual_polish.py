import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionVisualPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )

    def test_v5_safe_sis_panels_are_installed(self):
        self.assertIn("PROGRESSION V10: CLEAN NATIVE ROUTE", self.src)
        self.assertIn("static HSD_Text* ui_rect(", self.src)

    def test_raw_gx_panel_rendering_is_gone(self):
        # V5 comments explain why the V4 raw-GX path was removed, so the
        # identifiers may legitimately appear in prose. Verify executable
        # implementation markers instead.
        self.assertNotIn("#include <sysdolphin/baselib/hsd_3915.h>", self.src)
        self.assertNotIn("static void progression_panel_render", self.src)
        self.assertNotIn("panel_driver->render_callback =", self.src)
        self.assertNotIn("panel_driver_open(", self.src)
        self.assertNotIn("panel_driver_close(", self.src)
        self.assertNotIn("static HSD_Text* panel_driver", self.src)

    def test_rectangles_use_native_sis_backgrounds(self):
        rect = self.src.split("static HSD_Text* ui_rect", 1)[1]
        rect = rect.split("static void ui_encode", 1)[0]
        self.assertIn("text->bg_color = color;", rect)
        self.assertIn("text->box_size_x = w;", rect)
        self.assertIn("text->font_size.x = 1.0f;", rect)

    def test_reward_screen_splits_dynamic_sis_buffers(self):
        self.assertIn("draw_reward_card_panels", self.src)
        self.assertIn("draw_reward_card_text", self.src)
        self.assertIn("for (i = 0; i < 3; ++i)", self.src)
        self.assertIn("Keep every section in a modest independent buffer", self.src)

    def test_pixel_space_text_remains(self):
        self.assertIn("text->pos_x = 0.0f;", self.src)
        self.assertIn("text->font_size.x = 1.0f;", self.src)
        self.assertIn("ui_entry_raw(", self.src)

    def test_route_has_no_rogue_background_panel(self):
        base = self.src.split("static void draw_base_panels(void)", 1)[1]
        base = base.split("static void draw_reward_card_panels", 1)[0]
        self.assertIn("Intentionally draw NOTHING behind the route", base)
        self.assertNotIn("ui_route_glass", self.src)

    def test_build_has_four_distinct_slots(self):
        self.assertIn('static const char* keys[4] = {"N", "S", "U", "D"}', self.src)
        self.assertIn("ability_name(i)", self.src)


if __name__ == "__main__":
    unittest.main()

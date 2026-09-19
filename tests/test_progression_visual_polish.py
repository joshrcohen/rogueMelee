import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ProgressionVisualPolishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "mod/rogue_progression.c").read_text(
            encoding="utf-8"
        )

    def test_v4_native_panel_layer_is_installed(self):
        self.assertIn("PROGRESSION V4: NATIVE PANEL LAYER", self.src)
        self.assertIn("progression_panel_render", self.src)
        self.assertIn("DrawRectangle(", self.src)

    def test_panel_driver_uses_sis_render_callback(self):
        self.assertIn("panel_driver->render_callback = progression_panel_render", self.src)
        self.assertIn("panel_driver_open();", self.src)
        self.assertIn("panel_driver_close();", self.src)

    def test_panels_are_not_fake_text_backgrounds(self):
        renderer = self.src.split("PROGRESSION V4: NATIVE PANEL LAYER", 1)[1]
        renderer = renderer.split("static int copy_encounter", 1)[0]
        self.assertNotIn("static HSD_Text* ui_panel", renderer)
        self.assertIn("panel_box(", renderer)
        self.assertIn("panel_outline(", renderer)

    def test_native_pixel_text_group(self):
        self.assertIn("text->font_size.x = 1.0f;", self.src)
        self.assertIn("text->pos_x = 0.0f;", self.src)
        self.assertIn("ui_entry_raw(", self.src)

    def test_reference_layout_sections_exist(self):
        self.assertIn("draw_route_panels", self.src)
        self.assertIn("draw_reward_panels", self.src)
        self.assertIn("draw_fight_panels", self.src)
        self.assertIn("draw_build_strip_panels", self.src)
        self.assertIn("draw_full_build_panels", self.src)

    def test_build_hud_uses_four_ability_chips(self):
        self.assertIn("static const float chip_x[4]", self.src)
        self.assertIn('static const char* keys[4] = { "N", "S", "U", "D" }', self.src)
        self.assertIn("ability_name(i)", self.src)

    def test_major_titles_get_shadow_entries(self):
        self.assertIn("static void ui_title", self.src)
        self.assertIn("x + 1.0f, y + 1.0f", self.src)


if __name__ == "__main__":
    unittest.main()

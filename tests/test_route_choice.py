import re
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class RouteChoiceTests(unittest.TestCase):
    def test_route_source_is_compiled(self):
        p=(ROOT/'patches/engine.patch').read_text(encoding='utf-8')
        self.assertIn('Object(Equivalent, "melee/rogue/rogue_route.c")',p)
    def test_shape(self):
        h=(ROOT/'mod/rogue_route.h').read_text(encoding='utf-8')
        self.assertIn('#define ROGUE_ROUTE_ROUNDS 4',h)
        self.assertIn('#define ROGUE_ROUTE_CHOICES 2',h)
    def test_independent_rng(self):
        h=(ROOT/'mod/rogue_state.h').read_text(encoding='utf-8')
        c=(ROOT/'mod/rogue_state.c').read_text(encoding='utf-8')
        self.assertIn('RogueRng route_rng;',h)
        self.assertIn('seed ^ 0x524F5554U',c)
    def test_reward_flow(self):
        c=(ROOT/'mod/rogue_rewards.c').read_text(encoding='utf-8')
        self.assertIn('RogueRoute_Prepare',c)
        self.assertIn('RogueRoute_UseBoss',c)
        self.assertIn('ROGUE_PHASE_ROUTE',c)
    def test_native_rest_area_scene(self):
        c=(ROOT/'mod/rogue.c').read_text(encoding='utf-8')
        self.assertIn('enterRoute',c)
        self.assertIn('St_Kind_Heal',c)
        self.assertRegex(c,r'4,\s*lbDvdPreload_2,\s*0,\s*enterRoute,\s*exitRoute')
    def test_bracket_ui(self):
        c=(ROOT/'mod/rogue_ui.c').read_text(encoding='utf-8')
        self.assertIn('"ROGUE ROUTE"',c)
        self.assertIn("MATCH SET  -  STARTING ENCOUNTER", c)
        self.assertIn('"CHOOSE UPGRADE"', c)
        self.assertIn('"TOTAL SCORE"', c)
        self.assertIn("Rogue_SelectReward(route_reward_cursor)", c)
if __name__=='__main__': unittest.main()

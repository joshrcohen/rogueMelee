import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class AerialShopTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.state=(ROOT/"mod/rogue_state.h").read_text(encoding="utf-8")
        cls.ability=(ROOT/"mod/rogue_ability.c").read_text(encoding="utf-8")
        cls.header=(ROOT/"mod/rogue_ability.h").read_text(encoding="utf-8")
        cls.ui=(ROOT/"mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.camp=(ROOT/"mod/rogue_camp.c").read_text(encoding="utf-8")
        cls.build=(ROOT/"tools/build.py").read_text(encoding="utf-8")
        cls.post=(ROOT/"tools/postpatch_aerial_shop.py").read_text(encoding="utf-8")

    def test_five_slots_flat_100_gold(self):
        for name in ("NAIR","FAIR","BAIR","UAIR","DAIR"):
            self.assertIn("ROGUE_AERIAL_"+name,self.header)
        self.assertIn("#define ROGUE_AERIAL_PRICE 100",self.header)

    def test_run_stores_sources(self):
        self.assertIn("CharacterKind aerial_source[ROGUE_AERIAL_SLOTS]",self.state)

    def test_full_cast_catalog(self):
        self.assertIn("CKind_Playable_Count",self.ui)
        self.assertIn("Rogue_BuyAerial",self.ui)
        self.assertIn("AERIAL SHOP",self.ui)

    def test_runtime_source_data(self):
        self.assertIn("fp->x24 = gFtDataList[source]->xC",self.ability)
        self.assertIn("fighter_state.aerial_active = true",self.ability)
        self.assertIn("ftPartsRemap(fp->kind, source, bone)",self.ability)
        self.assertIn("Rogue_AerialLandingLag",self.post)

    def test_camp_zone(self):
        self.assertIn("i==1?It_Kind_Sword",self.camp)
        self.assertIn('"UPGRADE","AERIALS","REST","TRAIN","NEXT FIGHT"',self.ui)

    def test_build_pipeline_runs_postpatch(self):
        self.assertIn("postpatch_aerial_shop.py",self.build)

if __name__=="__main__": unittest.main()

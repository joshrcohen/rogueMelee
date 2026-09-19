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
        cls.rogue=(ROOT/"mod/rogue.c").read_text(encoding="utf-8")
        cls.camp=(ROOT/"mod/rogue_camp.c").read_text(encoding="utf-8")
        cls.build=(ROOT/"tools/build.py").read_text(encoding="utf-8")
        cls.post=(ROOT/"tools/postpatch_aerial_shop.py").read_text(encoding="utf-8")

    def test_five_slots_are_free_for_now(self):
        for name in ("NAIR","FAIR","BAIR","UAIR","DAIR"):
            self.assertIn("ROGUE_AERIAL_"+name,self.header)
        self.assertIn("#define ROGUE_AERIAL_PRICE 0",self.header)
        self.assertIn('"AERIAL SHOP  -  FREE"',self.ui)
        self.assertIn('"%s     PRICE FREE"',self.ui)

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

    def test_aerial_runtime_never_loads_files_on_attack(self):
        attempt=self.ability.split("bool Rogue_AerialTryEnter",1)[1]
        attempt=attempt.split("float Rogue_AerialLandingLag",1)[0]
        self.assertNotIn("ftLib_80087508",attempt)
        self.assertNotIn("prepareAerialSource",attempt)
        self.assertIn("aerial_loaded_sources[source]",attempt)

    def test_shop_purchase_never_loads_donor_assets(self):
        purchase=self.ability.split("bool Rogue_BuyAerial",1)[1]
        purchase=purchase.split("bool Rogue_AerialTryEnter",1)[0]
        self.assertNotIn("prepareAerialSource",purchase)
        self.assertNotIn("ftLib_80087508",purchase)
        self.assertNotIn("ftData_8008572C",purchase)
        self.assertIn("selection only",purchase)
        self.assertIn("APPLIES NEXT FIGHT",self.ui)

    def test_aerials_prepare_during_ready_countdown(self):
        self.assertIn("void Rogue_AerialPrepareFrame(void)",self.ability)
        self.assertIn("prepareAerialSource(fp, source)",self.ability)
        self.assertIn("gm_GetFrameCount() < 30",self.rogue)
        self.assertIn("Rogue_AerialPrepareFrame();",self.rogue)

    def test_gamewatch_custom_item_aerials_are_guarded(self):
        self.assertIn("source == Ft_Kind_GameWatch",self.ability)
        self.assertIn("ROGUE_AERIAL_NAIR",self.ability)
        self.assertIn("ROGUE_AERIAL_BAIR",self.ability)
        self.assertIn("ROGUE_AERIAL_UAIR",self.ability)
        self.assertIn("UNSUPPORTED",self.ui)

    def test_camp_zone(self):
        self.assertIn("i==1?It_Kind_Sword",self.camp)
        self.assertIn('"UPGRADE","AERIALS","REST","TRAIN","NEXT FIGHT"',self.ui)

    def test_build_pipeline_runs_postpatch(self):
        self.assertIn("postpatch_aerial_shop.py",self.build)

if __name__=="__main__": unittest.main()

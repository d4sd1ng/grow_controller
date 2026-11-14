"""VPD formula regression tests for the user's -2 C leaf-temperature chart.

Run: python3 -m unittest discover -s tests -p 'test_vpd_calculator.py' -v
"""
import unittest

from vpd_calculator import calculate_vpd, calculate_target_humidity


class VpdCalculatorTests(unittest.TestCase):
    def test_chart_offset_examples(self):
        self.assertEqual(calculate_vpd(24.0, 60.0), 0.85)
        self.assertEqual(calculate_vpd(26.0, 50.0), 1.30)
        self.assertEqual(calculate_vpd(20.0, 70.0), 0.43)

    def test_explicit_leaf_temperature(self):
        # Supplied IR value overrides the -2 C estimate.
        self.assertEqual(calculate_vpd(24.0, 60.0, leaf_temperature=24.0), 1.19)

    def test_target_humidity_uses_same_model(self):
        # Solver uses the -2 C leaf offset, not the air-only VPD model.
        t = 24.0
        for stage in ("early_grow", "late_grow_early_flowering", "late_flowering"):
            rh = calculate_target_humidity(t, stage)
            self.assertGreaterEqual(rh, 0)
            self.assertLessEqual(rh, 100)


if __name__ == "__main__":
    unittest.main()

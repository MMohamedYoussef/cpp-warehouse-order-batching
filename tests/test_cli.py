import csv
import io
import pathlib
import subprocess
import sys
import tempfile
import unittest

BIN = pathlib.Path(sys.argv.pop(1)).resolve()


class BatchingTests(unittest.TestCase):
    def run_cli(self, data, capacity):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "orders.csv"
            target = pathlib.Path(directory) / "batches.csv"
            source.write_text(data, encoding="utf-8")
            result = subprocess.run([str(BIN), str(source), str(capacity), str(target)], capture_output=True, text=True)
            output = target.read_text(encoding="utf-8") if target.exists() else None
            return result, output

    def test_batches_reduce_walk_and_respect_capacity(self):
        data = "order_id,location,units\nO1,2,2\nO2,4,1\nO3,8,2\n"
        result, output = self.run_cli(data, 3)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output, "batch_id,orders,units,walking_distance\n1,O1|O2,3,8\n2,O3,2,16\n")
        self.assertEqual(result.stdout.strip(), "batches=2 baseline_distance=28 batched_distance=24")

    def test_rejects_pick_larger_than_cart(self):
        result, output = self.run_cli("order_id,location,units\nO1,2,4\n", 3)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("capacity", result.stderr.lower())
        self.assertIsNone(output)

    def test_rejects_duplicate_order(self):
        result, _ = self.run_cli("order_id,location,units\nO1,2,1\nO1,3,1\n", 3)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate", result.stderr.lower())

    def test_rejects_formula_like_order_ids(self):
        for order in ("=1+1", "+2", "-2", "@SUM", "  =1+1"):
            with self.subTest(order=order):
                result, output = self.run_cli(f"order_id,location,units\n{order},2,1\n", 3)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("formula", result.stderr.lower())
                self.assertIsNone(output)

    def test_rejects_existing_output_without_temporary_residue(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "orders.csv"
            source.write_text("order_id,location,units\nO1,2,1\n", encoding="utf-8")
            target = root / "batches.csv"
            target.write_text("keep", encoding="utf-8")
            result = subprocess.run([str(BIN), str(source), "3", str(target)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(target.read_text(encoding="utf-8"), "keep")
            self.assertEqual(list(root.glob("batches.csv.tmp-*")), [])

    def test_refuses_dangling_symlink_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "orders.csv"
            source.write_text("order_id,location,units\nO1,2,1\n", encoding="utf-8")
            target = root / "batches.csv"
            missing = root / "missing.csv"
            try:
                target.symlink_to(missing)
            except (OSError, NotImplementedError):
                self.skipTest("creating symlinks is unavailable on this host")
            result = subprocess.run([str(BIN), str(source), "3", str(target)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(target.is_symlink())
            self.assertFalse(missing.exists())
            self.assertEqual(list(root.glob("batches.csv.tmp-*")), [])


if __name__ == "__main__":
    unittest.main()

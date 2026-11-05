"""Exercise release validation against missing, stale and corrupted artifacts."""
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from package_appfs import SOUNDS, sha256, verify_package
from prepare_bsp_compat import prepare


class ReleaseIntegrity(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="chronos-package-test-")
        self.addCleanup(self.temporary.cleanup)
        self.package = Path(self.temporary.name)
        metadata = json.loads((ROOT / "assets/icons/metadata.json").read_text())
        (self.package / "metadata.json").write_text(json.dumps(metadata))
        for name in ("chronos.bin", "LICENSE.txt", "THIRD_PARTY_LICENSES.txt", "README.md",
                     "external-display.md", "ACCEPTANCE.md", *metadata["icon"].values()):
            (self.package / name).write_bytes(b"fixture")
        for name, source in SOUNDS.items():
            shutil.copy2(ROOT / "assets/sounds" / source, self.package / name)
        (self.package / "BUILD_INFO.json").write_text(json.dumps({
            "firmware_sha256": sha256(self.package / "chronos.bin"),
            "application_version": metadata["version"],
            "application_revision": metadata["application"][0]["revision"],
            "bsp_version": metadata["application"][0]["bsp_version"]}))
        self.checksums()

    def checksums(self):
        (self.package / "SHA256SUMS").write_text("".join(
            f"{sha256(path)}  {path.name}\n" for path in sorted(self.package.iterdir())
            if path.name != "SHA256SUMS"))

    def test_complete_package(self):
        verify_package(self.package)

    def test_missing_declared_sound(self):
        (self.package / "alarm3.wav").unlink()
        with self.assertRaisesRegex(ValueError, "Missing or invalid package file"):
            verify_package(self.package)

    def test_corrupted_binary(self):
        (self.package / "chronos.bin").write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            verify_package(self.package)

    def test_unlisted_file(self):
        (self.package / "stale.bin").write_bytes(b"old firmware")
        with self.assertRaisesRegex(ValueError, "cover every package file"):
            verify_package(self.package)

    def test_broken_documentation_link(self):
        (self.package / "README.md").write_text("[Missing guide](missing.md)\n")
        self.checksums()
        with self.assertRaisesRegex(ValueError, "Broken package documentation link"):
            verify_package(self.package)

    def test_stale_version_even_with_updated_checksums(self):
        info = json.loads((self.package / "BUILD_INFO.json").read_text())
        info["application_version"] = "0.1.0"
        (self.package / "BUILD_INFO.json").write_text(json.dumps(info))
        self.checksums()
        with self.assertRaisesRegex(ValueError, "does not match package metadata"):
            verify_package(self.package)

    def test_unreviewed_dependency_rejected_without_output(self):
        bsp = self.package / "bsp"
        target = bsp / "targets/tanmatsu/badge_bsp_display.c"
        target.parent.mkdir(parents=True)
        target.write_text("unreviewed upstream source\n")
        output = self.package / "generated"
        with self.assertRaisesRegex(ValueError, "Unreviewed BSP/MIPI source"):
            prepare(bsp, self.package / "mipi", output)
        self.assertFalse(output.exists())
        self.assertEqual(target.read_text(), "unreviewed upstream source\n")


if __name__ == "__main__":
    unittest.main()

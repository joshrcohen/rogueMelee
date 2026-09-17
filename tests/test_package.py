"""Asset-free regression tests for the portable delta format."""
import hashlib
import io
from pathlib import Path
import random
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/package'))
from delta import delta


def decode(source, encoded):
    stream = io.BytesIO(encoded)
    assert stream.read(8) == b'ROGUED01'
    expected = stream.read(32)
    length, = struct.unpack('<I', stream.read(4))
    result = bytearray()
    while True:
        op = stream.read(1)
        if op == b'\xff':
            break
        if op == b'\x00':
            count, = struct.unpack('<I', stream.read(4))
            result += stream.read(count)
        elif op == b'\x01':
            offset, count = struct.unpack('<II', stream.read(8))
            result += source[offset:offset + count]
        else:
            raise AssertionError('Unknown operation')
    assert stream.read() == b''
    assert len(result) == length
    assert hashlib.sha256(result).digest() == expected
    return bytes(result)


class DeltaTests(unittest.TestCase):
    def test_shifted_and_reordered_sections(self):
        source = random.Random(41).randbytes(8192)
        target = b'new header' + source[4096:] + source[:2048] + b'new code'
        encoded = delta(source, target)
        self.assertEqual(decode(source, encoded), target)
        self.assertLess(len(encoded), len(target) // 4)

    def test_short_and_repetitive_inputs(self):
        for source, target in [(b'', b''), (b'', b'new'), (b'old', b''),
                               (b'x' * 4096, b'x' * 3000 + b'y' * 40)]:
            with self.subTest(source_length=len(source), target_length=len(target)):
                self.assertEqual(decode(source, delta(source, target)), target)

    def test_unrelated_sections(self):
        rng = random.Random(923)
        source, target = rng.randbytes(4096), rng.randbytes(4096)
        self.assertEqual(decode(source, delta(source, target)), target)

    def test_corrupt_digest_rejected(self):
        encoded = bytearray(delta(b'original', b'modified'))
        encoded[8] ^= 1
        with self.assertRaises(AssertionError):
            decode(b'original', encoded)


if __name__ == '__main__':
    unittest.main()

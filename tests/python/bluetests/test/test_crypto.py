from . import blueunittest
import blue

class TestCrypto(blueunittest.TestCase):
    def setUp(self):
        self.cipher = blue.SymmetricCipher()

    def test_canSetKey(self):
        key = blue.crypto.GenerateRandomBytes(32)
        iv = blue.crypto.GenerateRandomBytes(16)
        self.assertIsNone(self.cipher.LoadKey(key, iv))

    def test_keyRequires32bytes(self):
        iv = blue.crypto.GenerateRandomBytes(16)
        for len in (1, 4, 38192, 43):
            key = blue.crypto.GenerateRandomBytes(len)
            with self.assertRaises(ValueError):
                self.cipher.LoadKey(key, iv)

    def test_ivRequires16bytes(self):
        key = blue.crypto.GenerateRandomBytes(32)
        for len in (1, 4, 38192, 43):
            iv = blue.crypto.GenerateRandomBytes(len)
            with self.assertRaises(ValueError):
                self.cipher.LoadKey(key, iv)

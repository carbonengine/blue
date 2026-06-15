# Copyright © 2023 CCP ehf.

from . import blueunittest

import blue

class TestAsymmetricCipher(blueunittest.TestCase):
    def setUp(self):
        self.cipher = blue.crypto.GetSharedAsymmetricCipher()
        self.cipher.GenerateKey(2048)
        self.payload = b"Lorem ipsum dolor sit amet, consectetur adipiscing elit integer."

    def test_signing(self):
        signature = self.cipher.Sign(self.payload)
        self.assertTrue(self.cipher.VerifySignature(self.payload, signature))
        self.assertFalse(self.cipher.VerifySignature(self.payload, b"this is not a real signature"))

    def test_encryption(self):
        encrypted_payload = self.cipher.Encrypt(self.payload)
        self.assertEqual(self.payload, self.cipher.Decrypt(encrypted_payload))


class TestSymmetricCipher(blueunittest.TestCase):
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

    def test_encryption(self):
        key = blue.crypto.GenerateRandomBytes(32)
        iv = blue.crypto.GenerateRandomBytes(16)
        payload = b'foobarbaz'
        self.cipher.LoadKey(key, iv)
        enc = self.cipher.Encrypt(payload)
        self.assertEqual(payload, self.cipher.Decrypt(enc))

# Copyright © 2023 CCP ehf.

import blue
import unittest

class testMemStream(unittest.TestCase):
    def testCreate(self):
        m = blue.MemStream()


    def testWrite(self):
        m = blue.MemStream()
        s = b"This is a test string"
        m.Write(s)


    def testWriteAndReadBack(self):
        m = blue.MemStream()
        s0 = b"This is a test string"
        m.Write(s0)
        m.Seek(0)

        s1 = m.Read()
        self.assertEqual(s0, s1)


    def testMultipleWriteAndReadBack(self):
        m = blue.MemStream()
        m.Write(b"This ")
        m.Write(b"is a ")
        m.Write(b"test string")

        m.Seek(0)
        s = m.Read()
        self.assertEqual(s, b"This is a test string")

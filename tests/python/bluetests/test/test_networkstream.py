# Copyright © 2023 CCP ehf.

from . import blueunittest
import blue


class TestNetworkStream(blueunittest.TestCase):
    def testStreamWithInvalidUrlDoesNotRaise(self):
        blue.BlueNetworkStream('blahblah')

    def testStreamWithInvalidUrlRaisesAtReading(self):
        s = blue.BlueNetworkStream('blahblah')
        with self.assertRaises(IOError):
            s.read()

    def testCanOpenHttpStream(self):
        self.assertTrue(blue.BlueNetworkStream('http://www.google.com'))

    def testCanQueryStreamSize(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        self.assertGreater(s.size, 0)

    def testReadWithRequestedSizeReturnsStringOfThatSize(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        self.assertEqual(len(s.read(10)), 10)

    def testCanReadEntireFile(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        self.assertEqual(len(s.read()), s.size)

    def testReadReturnsUpToFileSizeData(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        self.assertEqual(len(s.read(10000000)), s.size)

    def testCurrentPositionIsZeroAtTheBeginningOfTransfer(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        self.assertEqual(s.tell(), 0)

    def testCurrentPositionIsIncrementedWithRead(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        s.read(10)
        self.assertEqual(s.tell(), 10)

    def testReadingClosedFileRaises(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        s.close()
        with self.assertRaises(ValueError):
            s.read()

    def testCurrentPositionIsUnavailableAfterClosingFile(self):
        s = blue.BlueNetworkStream('http://www.google.com')
        s.close()
        with self.assertRaises(ValueError):
            s.tell()

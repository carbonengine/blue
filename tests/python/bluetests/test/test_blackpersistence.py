# Copyright © 2023 CCP ehf.

import unittest
import blue
import tempfile
import shutil
import os

MAX_INT32   = 2147483647
MAX_UINT32  = 4294967295
MAX_INT64   = 9223372036854775807
MAX_UINT64  = 18446744073709551615

class TestBlackWriterAndReader(unittest.TestCase):
    def setUp(self):
        self.cachePath = blue.paths.GetSearchPath("cache")
        self.tmpDir = tempfile.mkdtemp()
        blue.paths.SetSearchPath("cache", self.tmpDir)

    def tearDown(self):
        shutil.rmtree(self.tmpDir)
        blue.paths.SetSearchPath("cache", self.cachePath)


    def _testWriteToMemStreamAndReadBack(self, testValue, testAttributeName):
        x = blue.BlueTestHelperAttributes()
        setattr(x,testAttributeName,testValue)

        writer = blue.BlackWriter()
        reader = blue.BlackReader()

        stream = blue.MemStream()
        writer.WriteObjectToStream(x, stream)

        stream.Seek(0)

        y = reader.CreateObjectFromStream(stream)

        self.assertEqual(type(x), type(y))
        self.assertEqual(x.myString, y.myString)
        self.assertEqual(x.sharedString, y.sharedString)

        #TODO: Test references
        #TODO: Test circular references
        #TODO: Test all base types and container types

    def testWriteStringToMemStreamAndReadBack(self):
        self._testWriteToMemStreamAndReadBack("Test String","myString")

    def testWriteStringToMemStreamAndReadBack(self):
        self._testWriteToMemStreamAndReadBack("Test String","sharedString")

    def testWriteInt32ToMemStreamAndReadBack(self):
        self._testWriteToMemStreamAndReadBack(MAX_INT32,"myInt")
        self._testWriteToMemStreamAndReadBack(-MAX_INT32-1,"myInt")

    def testWriteUInt32ToMemStreamAndReadBack(self):
        self._testWriteToMemStreamAndReadBack(MAX_UINT32,"myUInt")

    def testWriteInt64ToMemStreamAndReadBack(self):
        self._testWriteToMemStreamAndReadBack(MAX_INT64,"myInt64")
        self._testWriteToMemStreamAndReadBack(-MAX_INT64-1,"myInt64")

    def testWriteUInt64ToMemStreamAndReadBack(self):
        self._testWriteToMemStreamAndReadBack(MAX_UINT64,"myUInt64")

    def _testWriteToFileAndReadBack(self, testValue, testAttributeName):
        
        x = blue.BlueTestHelperAttributes()
        setattr(x,testAttributeName,testValue)

        writer = blue.BlackWriter()
        writer.WriteObjectToFile(x, "cache:/test.black")

        pathOnDisk = blue.paths.ResolvePath("cache:/test.black")
        self.assertTrue(os.path.exists(pathOnDisk))

        reader = blue.BlackReader()
        z = reader.CreateObjectFromFile("cache:/test.black")

        self.assertEqual(type(x), type(z))
        self.assertEqual(getattr(x,testAttributeName), getattr(z,testAttributeName))

        #TODO: Test references
        #TODO: Test circular references
        #TODO: Test all base types and container types


    def testWriteStringToFileAndReadBack(self):
        self._testWriteToFileAndReadBack("Test String","myString")

    def testWriteInt32ToFileAndReadBack(self):
        self._testWriteToFileAndReadBack(MAX_INT32,"myInt")
        self._testWriteToFileAndReadBack(-MAX_INT32-1,"myInt")

    def testWriteUInt32ToFileAndReadBack(self):
        self._testWriteToFileAndReadBack(MAX_UINT32,"myUInt")

    def testWriteInt64ToFileAndReadBack(self):
        self._testWriteToFileAndReadBack(MAX_INT64,"myInt64")
        self._testWriteToFileAndReadBack(-MAX_INT64-1,"myInt64")

    def testWriteUInt64ToFileAndReadBack(self):
        self._testWriteToFileAndReadBack(MAX_UINT64,"myUInt64")

    def _createTestObject(self):
        """
        Create an object that can be used for testing persistence.
        """
        x = blue.BlueTestHelperAttributes()
        x.myString = "Test string"
        x.sharedString = "Test shared string"
        x.myInt = 42
        x.myFloat = 3.14

        for i in range(10):
            child = blue.BlueTestHelperAttributes()
            child.myString = "This is test string number %d" % (i + 1)
            child.myInt = i
            child.myFloat = i * 3.14
            x.myVector.append(child)

        return x


    def _createValidBlackStream(self):
        """
        Creates a test object and uses BlackWriter to write it to a stream.
        """
        x = self._createTestObject()

        writer = blue.BlackWriter()
        stream = blue.MemStream()
        writer.WriteObjectToStream(x, stream)

        stream.Seek(0)

        return stream

    def _corruptBlackStream(self, stream, pos):
        """
        Corrupts a stream at the given position.
        """
        stream.Seek(pos)
        stream.Write(b"\3")
        stream.Seek(0)


    def testBlackReaderErrors(self):
        """
        Test BlackReader error reporting.
        """
        stream = self._createValidBlackStream()
        self._corruptBlackStream(stream, 0)

        reader = blue.BlackReader()

        self.assertRaises(RuntimeError, reader.CreateObjectFromStream, stream)

        stream = self._createValidBlackStream()
        self._corruptBlackStream(stream, 10)

        self.assertRaises(RuntimeError, reader.CreateObjectFromStream, stream)

        stream = self._createValidBlackStream()
        self._corruptBlackStream(stream, 34)

        self.assertRaises(RuntimeError, reader.CreateObjectFromStream, stream)

        stream = self._createValidBlackStream()
        self._corruptBlackStream(stream, 42)

        self.assertRaises(RuntimeError, reader.CreateObjectFromStream, stream)


    def _compareStreams(self, s0, s1):
        """
        Compare two streams - assert if they're not equal.
        """
        self.assertEqual(s0.size, s1.size, "Streams have different sizes")

        s0.Seek(0)
        s1.Seek(0)

        v0 = s0.Read(s0.size)
        v1 = s1.Read(s1.size)

        self.assertEqual(v0, v1, "Stream contents are different")


    def testBlackWriterReuse(self):
        """
        Verify that one instance of a BlackWriter can write multiple
        objects (in separate Write calls).
        """
        s0 = self._createValidBlackStream()

        writer = blue.BlackWriter()
        for i in range(10):
            x = self._createTestObject()
            stream = blue.MemStream()
            writer.WriteObjectToStream(x, stream)
            self._compareStreams(s0, stream)


    def testBlackReaderReuse(self):
        """
        Verify that one instance of a BlackReader can read multiple
        objects (in separate Read calls).
        """
        s0 = self._createValidBlackStream()

        reader = blue.BlackReader()
        writer = blue.BlackWriter()
        for i in range(10):
            s0.Seek(0)
            x = reader.CreateObjectFromStream(s0)
            stream = blue.MemStream()
            writer.WriteObjectToStream(x, stream)
            self._compareStreams(s0, stream)


    def testBlackReaderVersionsSupported(self):
        reader = blue.BlackReader()
        versions = reader.GetVersionsSupported()
        self.assertEqual(versions,[1])


    def testBlackWriterCurrentVersion(self):
        writer = blue.BlackWriter()
        self.assertEqual(writer.currentVersion, 1)

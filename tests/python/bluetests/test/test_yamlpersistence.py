# Copyright © 2023 CCP ehf.

import unittest
import blue
import tempfile
import shutil

YAML_ERROR_STRINGS_STRICT = [
    # Empty string
    "",

    # Garbage
    "bla bla",

    # Garbage followed by valid yaml
    "bla bla\nmore bla bla\ntype: BlueTestHelperAttributes\n",

    # Type keyword misspelled
    "typ: BlueTestHelperAttributes\nmyString: \"Test String\"\n",

    # Class name misspelled
    "type: BleTestHelperAttributes\nmyString: \"Test String\"\n",

    # Attribute name misspelled    
    "type: BlueTestHelperAttributes\nmString: \"Test String\"\n",

    # End quote missing    
    "type: BlueTestHelperAttributes\nmyString: \"Test String\n",

    # Class name misspelled
    "type: BleTestHelperAttributes\nmyString: \"Test String\"\n"

]

MAX_INT32   = 2147483647
MAX_UINT32  = 4294967295
MAX_INT64   = 9223372036854775807
MAX_UINT64  = 18446744073709551615

class TestYamlReader(unittest.TestCase):
    def testCreateObjectFromString_Simple(self):
        reader = blue.YamlReader()

        s = "type: BlueTestHelperAttributes\nmyString: \"Test String\"\n"

        x = reader.CreateObjectFromString(s)

        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(x.myString, "Test String")

    def testCreateObjectFromStream_Simple(self):
        reader = blue.YamlReader()

        s = b"type: BlueTestHelperAttributes\nmyString: \"Test String\"\n"

        stream = blue.MemStream()
        stream.Write(s)
        stream.Seek(0)

        y = reader.CreateObjectFromStream(stream)

        self.assertEqual(type(y), blue.BlueTestHelperAttributes)
        self.assertEqual(y.myString, "Test String")


class TestYamlWriterAndReader(unittest.TestCase):
    def setUp(self):
        self.cachePath = blue.paths.GetSearchPath("cache")
        self.tmpDir = tempfile.mkdtemp()
        blue.paths.SetSearchPath("cache", self.tmpDir)

    def tearDown(self):
        shutil.rmtree(self.tmpDir)
        blue.paths.SetSearchPath("cache", self.cachePath)



    def _testYamlWriter(self, testHelperAttributes, expected):

       
        writer = blue.YamlWriter()
        s = writer.WriteObjectToString(testHelperAttributes)

        self.assertEqual(s, expected)

        writer.skipDefaults = False
        sWithDefaults = writer.WriteObjectToString(testHelperAttributes)
        writer.skipDefaults = True

        self.assertTrue(len(sWithDefaults) > len(s))

        # Write the same object to a memory stream - results should be the same
        # as writing to a string
        stream = blue.MemStream()
        writer.WriteObjectToStream(testHelperAttributes, stream)

        stream.Seek(0)
        s2 = stream.Read().decode('utf-8')

        self.assertEqual(s, s2)

        # Write the same object a file stream - results should still be the same
        rf = blue.ResFile()
        rf.Create("cache:/test.red")

        writer.WriteObjectToStream(testHelperAttributes, rf)
        rf.Seek(0)
        s3 = rf.Read().decode('utf-8')

        self.assertEqual(s, s3)

        # Write the same object to a file - results should still be the same
        writer.WriteObjectToFile(testHelperAttributes, "cache:/test2.red")

        rf2 = blue.ResFile()
        rf2.Open("cache:/test2.red")
        s4 = rf2.Read().decode('utf-8')

        self.assertEqual(s, s4)

        #TODO: Test references
        #TODO: Test circular references
        #TODO: Test all base types and container types


    def testYamlWriterString(self):

        x = blue.BlueTestHelperAttributes()

        x.myString = "Test String"
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyString: \"Test String\"\n")

    def testYamlWriterUnicodeString(self):

        x = blue.BlueTestHelperAttributes()

        x.myUnicode = "Test String"
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyUnicode: \"Test String\"\n")

    def testYamlWriterBool(self):

        x = blue.BlueTestHelperAttributes()

        x.myBool = True
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyBool: 1\n")

    def testYamlWriterInt32(self):

        x = blue.BlueTestHelperAttributes()

        x.myInt = MAX_INT32
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyInt: {0}\n".format(MAX_INT32))

        x.myInt = -MAX_INT32-1
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyInt: {0}\n".format(-MAX_INT32-1))

    def testYamlWriterUInt32(self):

        x = blue.BlueTestHelperAttributes()

        x.myUInt = MAX_UINT32
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyUInt: {0}\n".format(MAX_UINT32))

    def testYamlWriterDouble(self):

        x = blue.BlueTestHelperAttributes()

        x.myDouble = 12.34
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyDouble: {0}\n".format(12.34))

    def testYamlWriterInt64(self):

        x = blue.BlueTestHelperAttributes()

        x.myInt64 = MAX_INT64
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyInt64: {0}\n".format(MAX_INT64))

        x.myInt64 = -MAX_INT64-1
        self._testYamlWriter(x,"type: BlueTestHelperAttributes\nmyInt64: {0}\n".format(-MAX_INT64-1))


    def _testYamlReader(self, testHelperAttributeName, value):
        reader = blue.YamlReader()

        s = "type: BlueTestHelperAttributes\n{0}: {1}\n".format(testHelperAttributeName, value)

        x = reader.CreateObjectFromString(s)

        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(getattr(x,testHelperAttributeName), value)

        stream = blue.MemStream()
        stream.Write(s.encode())
        stream.Seek(0)

        y = reader.CreateObjectFromStream(stream)

        self.assertEqual(type(y), blue.BlueTestHelperAttributes)
        self.assertEqual(getattr(y,testHelperAttributeName), value)


        rf = blue.ResFile()
        rf.Create("cache:/test.red")
        rf.Write(s.encode())
        rf.close()

        z = reader.CreateObjectFromFile("cache:/test.red")

        self.assertEqual(type(z), blue.BlueTestHelperAttributes)
        self.assertEqual(getattr(z,testHelperAttributeName), value)

        #TODO: Test references
        #TODO: Test circular references
        #TODO: Test all base types and container types
        #TODO: Test malformed yaml

    def testYamlReaderInt32(self):
        self._testYamlReader("myInt",MAX_INT32)
        self._testYamlReader("myInt",-MAX_INT32-1)

    def testYamlReaderUInt32(self):
        self._testYamlReader("myUInt",MAX_UINT32)

    def testYamlReaderInt64(self):
        self._testYamlReader("myInt64",MAX_INT64)
        self._testYamlReader("myInt64",-MAX_INT64-1)

    def testYamlReaderUInt64(self):
        self._testYamlReader("myUInt64",MAX_UINT64)

    def testYamlReaderDouble(self):
        import locale
        orig = locale.setlocale(locale.LC_ALL)
        locale.setlocale(locale.LC_ALL, 'fr_FR')
        self._testYamlReader("myDouble", 222.555);
        self._testYamlReader("myDouble", -222.555);
        locale.setlocale(locale.LC_ALL, orig)

    def testWriteSharedString(self):
        x = blue.BlueTestHelperAttributes()

        x.sharedString = "Test Shared String"

        writer = blue.YamlWriter()
        s = writer.WriteObjectToString(x)

        self.assertEqual(s, "type: BlueTestHelperAttributes\nsharedString: \"Test Shared String\"\n")


    def testReadSharedString(self):
        reader = blue.YamlReader()

        s = "type: BlueTestHelperAttributes\nsharedString: \"Test String\"\n"

        x = reader.CreateObjectFromString(s)

        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(x.sharedString, "Test String")

    
    def _writeStringToFile(self, filename, string):
        rf = blue.ResFile()
        rf.Create(filename)
        rf.Write(string.encode())
        rf.close()


    def testYamlReaderErrorsFromString(self):
        """
        Test error reporting when creating objects from a yaml string.
        """
        
        reader = blue.YamlReader()
        reader.isStrict = True

        for errorString in YAML_ERROR_STRINGS_STRICT:
            self.assertRaises(RuntimeError, reader.CreateObjectFromString, errorString)    

        reader.isStrict = False

        # Attribute name misspelled
        x = reader.CreateObjectFromString("type: BlueTestHelperAttributes\nmString: \"Test String\"\n")

        self.assertEqual(x.myString, "")


    def testYamlReaderErrorsFromFile(self):
        """
        Test error reporting when creating objects from yaml in a file.
        """

        reader = blue.YamlReader()
        reader.isStrict = True

        counter = 0
        for errorString in YAML_ERROR_STRINGS_STRICT:
            filename = "cache:/test_%d.red" % counter
            counter += 1
            self._writeStringToFile(filename, errorString)
            self.assertRaises(RuntimeError, reader.CreateObjectFromFile, filename)

        reader.isStrict = False

        # Attribute name misspelled
        filename = "cache:/test_%d.red" % counter
        counter += 1
        self._writeStringToFile(filename, "type: BlueTestHelperAttributes\nmString: \"Test String\"\n")

        x = reader.CreateObjectFromFile(filename)

        self.assertEqual(x.myString, "")

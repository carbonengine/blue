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


    def testYamlWriterBasics(self):
        x = blue.BlueTestHelperAttributes()

        x.myString = "Test String"

        writer = blue.YamlWriter()
        s = writer.WriteObjectToString(x)

        self.assertEqual(s, "type: BlueTestHelperAttributes\nmyString: \"Test String\"\n")

        writer.skipDefaults = False
        sWithDefaults = writer.WriteObjectToString(x)
        writer.skipDefaults = True

        self.assertTrue(len(sWithDefaults) > len(s))

        # Write the same object to a memory stream - results should be the same
        # as writing to a string
        stream = blue.MemStream()
        writer.WriteObjectToStream(x, stream)

        stream.Seek(0)
        s2 = stream.Read().decode('utf-8')

        self.assertEqual(s, s2)

        # Write the same object a file stream - results should still be the same
        rf = blue.ResFile()
        rf.Create("cache:/test.red")

        writer.WriteObjectToStream(x, rf)
        rf.Seek(0)
        s3 = rf.Read().decode('utf-8')

        self.assertEqual(s, s3)

        # Write the same object to a file - results should still be the same
        writer.WriteObjectToFile(x, "cache:/test2.red")

        rf2 = blue.ResFile()
        rf2.Open("cache:/test2.red")
        s4 = rf2.Read().decode('utf-8')

        self.assertEqual(s, s4)

        #TODO: Test references
        #TODO: Test circular references
        #TODO: Test all base types and container types


    def testYamlReaderBasics(self):
        reader = blue.YamlReader()

        s = "type: BlueTestHelperAttributes\nmyString: \"Test String\"\n"

        x = reader.CreateObjectFromString(s)

        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(x.myString, "Test String")

        stream = blue.MemStream()
        stream.Write(s.encode())
        stream.Seek(0)

        y = reader.CreateObjectFromStream(stream)

        self.assertEqual(type(y), blue.BlueTestHelperAttributes)
        self.assertEqual(y.myString, "Test String")


        rf = blue.ResFile()
        rf.Create("cache:/test.red")
        rf.Write(s.encode())
        rf.close()

        z = reader.CreateObjectFromFile("cache:/test.red")

        self.assertEqual(type(z), blue.BlueTestHelperAttributes)
        self.assertEqual(z.myString, "Test String")

        #TODO: Test references
        #TODO: Test circular references
        #TODO: Test all base types and container types
        #TODO: Test malformed yaml


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

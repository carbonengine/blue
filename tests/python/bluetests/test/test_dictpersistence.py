# Copyright © 2023 CCP ehf.

from . import blueunittest
import blue

class TestDictReader(blueunittest.TestCase):
    def testDictReaderBasics(self):
        reader = blue.DictReader()

        d1 = {"type": "BlueTestHelperAttributes", "myString": "Test String"}

        x = reader.CreateObject(d1)

        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(x.myString, "Test String")


    def test_BasicReferencing(self):
        reader = blue.DictReader()

        d1 = {"type": "BlueTestHelperAttributes", "myString": "Test String"}
        d2 = {"type": "BlueTestHelperAttributes", "myString": "Another test string", "myVector": [d1]}
        d3 = {"type": "BlueTestHelperAttributes", "myString": "This one has references", "myVector": [d1, d2] }
        
        x = reader.CreateObject(d3)
        self.assertEqual(len(x.myVector), 2)
        self.assertEqual(len(x.myVector[0].myVector), 0)
        self.assertEqual(len(x.myVector[1].myVector), 1)

        # Note that we can't use id - use blue.GetID instead
        self.assertEqual(blue.GetID(x.myVector[0]), blue.GetID(x.myVector[1].myVector[0]), "Instancing not maintained")


    def test_DictReaderReuse(self):
        reader = blue.DictReader()

        d1 = {"type": "BlueTestHelperAttributes", "myString": "Test String"}
        d2 = {"type": "BlueTestHelperAttributes", "myString": "Another test string", "myVector": [d1]}
        d3 = {"type": "BlueTestHelperAttributes", "myString": "This one has references", "myVector": [d1, d2] }
        
        x = reader.CreateObject(d3)
        y = reader.CreateObject(d3)
        z = reader.CreateObject(d3)

        self.assertBlueObjectsEqual(x, y)
        self.assertBlueObjectsEqual(x, z)


    def test_AllBaseTypes(self):
        reader = blue.DictReader()

        d = {
            "type": "BlueTestHelperAttributes",
            "myString": "this is a test",
            "myUnicode": u"this is a test",
            "myBool": True,
            "myInt": 42,
            "myFloat": 3.14,
            "myDouble": 2.71828182845904523536,
            "myInt64": 4294967294
            }

        x = reader.CreateObject(d)

        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(x.myString, d["myString"])
        self.assertEqual(x.myUnicode, d["myUnicode"])
        self.assertEqual(x.myBool, d["myBool"])
        self.assertEqual(x.myInt, d["myInt"])
        self.assertAlmostEqual(x.myFloat, d["myFloat"], places=6)
        self.assertEqual(x.myDouble, d["myDouble"])
        self.assertEqual(x.myInt64, d["myInt64"])


    def test_StringFromUnicode(self):
        reader = blue.DictReader()

        d = {
            "type": "BlueTestHelperAttributes",
            "myString": u"this is a test",
            }

        x = reader.CreateObject(d)
        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(x.myString, str(d["myString"]))


    def test_UnicodeFromString(self):
        reader = blue.DictReader()

        d = {
            "type": "BlueTestHelperAttributes",
            "myUnicode": u"this is a test",
            }
        
        x = reader.CreateObject(d)
        self.assertEqual(type(x), blue.BlueTestHelperAttributes)
        self.assertEqual(x.myUnicode, str(d["myUnicode"]))


    def test_BoolErrors(self):
        reader = blue.DictReader()

        d = {
            "type": "TestAttributes",
            "myBool": "False",
            }
        
        self.assertRaises(RuntimeError, reader.CreateObject, d)

        d = {
            "type": "TestAttributes",
            "myBool": None,
            }
        
        self.assertRaises(RuntimeError, reader.CreateObject, d)
        
        d = {
            "type": "TestAttributes",
            "myBool": 3.14,
            }
        
        self.assertRaises(RuntimeError, reader.CreateObject, d)
        

    def test_CircularReference(self):
        reader = blue.DictReader()

        # Set up a circular reference
        d1 = {"type": "TestAttributes", "myString": "Another test string"}
        d2 = {"type": "TestAttributes", "myString": "Another test string", "myVector": [d1]}
        d1["myVector"] = [d2]

        self.assertRaises(RuntimeError, reader.CreateObject, d1)

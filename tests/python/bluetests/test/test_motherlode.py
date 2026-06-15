# Copyright © 2023 CCP ehf.

from . import blueunittest
import blue

class TestMotherLode(blueunittest.TestCase):
    """
    A set of test cases for blue.motherLode.
    """

    def tearDown(self):
        blue.motherLode.clear()

    def testInitialState(self):
        self.assertEqual(blue.motherLode.size(), 0)

    def testInsert_AsciiName(self):
        obj = blue.BlueTestHelperAttributes()
        blue.motherLode.Insert("test", obj)

    def testInsert_UnicodeName(self):
        obj = blue.BlueTestHelperAttributes()
        blue.motherLode.Insert(u"test", obj)

    def testInsert_None(self):
        self.assertRaises(RuntimeError, blue.motherLode.Insert, "test", None)

    def testLookup_ObjectNotInserted(self):
        obj = blue.motherLode.Lookup("test")
        self.assertEqual(obj, None)

    def testLookup_AfterObjectInsertion(self):
        obj = blue.BlueTestHelperAttributes()
        blue.motherLode.Insert("test", obj)
        objFromMotherLode = blue.motherLode.Lookup("test")
        self.assertBlueObjectsEqual(obj, objFromMotherLode)

    def testDelete_NonExistingKey(self):
        result = blue.motherLode.Delete("test")
        self.assertFalse(result)

    def testDelete_ExistingKey(self):
        obj = blue.BlueTestHelperAttributes()
        blue.motherLode.Insert("test", obj)
        result = blue.motherLode.Delete("test")
        self.assertTrue(result)

    def testItems_Empty(self):
        items = blue.motherLode.items()
        self.assertEqual(len(items), 0)

    def testItems(self):
        expectedItems = []
        for i in range(5):
            key = "key%d" % i
            value = blue.BlueTestHelperAttributes()
            value.myString = key
            expectedItems.append((key, value))
            blue.motherLode.Insert(key, value)

        items = blue.motherLode.items()
        self.assertCountEqual(expectedItems, items)

    def testKeys_Empty(self):
        keys = blue.motherLode.keys()
        self.assertEqual(len(keys), 0)

    def testKeys(self):
        expectedKeys = []
        values = []
        for i in range(5):
            key = "key%d" % i
            value = blue.BlueTestHelperAttributes()
            value.myString = key
            expectedKeys.append(key)
            values.append(value)
            blue.motherLode.Insert(key, value)

        keys = blue.motherLode.keys()
        self.assertCountEqual(expectedKeys, keys)

    def testValues_Empty(self):
        values = blue.motherLode.values()
        self.assertEqual(len(values), 0)

    def testValues(self):
        expectedValues = []
        for i in range(5):
            key = "key%d" % i
            value = blue.BlueTestHelperAttributes()
            value.myString = key
            expectedValues.append(value)
            blue.motherLode.Insert(key, value)

        values = blue.motherLode.values()
        self.assertEqual(set(expectedValues), set(values))

    def testSize_Empty(self):
        self.assertEqual(blue.motherLode.size(), 0)

    def testSize_OneInsertion(self):
        obj = blue.BlueTestHelperAttributes()
        blue.motherLode.Insert("test", obj)
        self.assertEqual(blue.motherLode.size(), 1)

    def testSize_MultipleInsertions(self):
        expectedValues = []
        for i in range(5):
            key = "key%d" % i
            value = blue.BlueTestHelperAttributes()
            value.myString = key
            expectedValues.append(value)
            blue.motherLode.Insert(key, value)
        self.assertEqual(blue.motherLode.size(), len(expectedValues))

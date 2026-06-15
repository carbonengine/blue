# Copyright © 2023 CCP ehf.

import unittest
import blue

class TestBlueStructureList(unittest.TestCase):
    """
    A set of unit tests for BlueStructureList.
    """

    def testBasics(self):
        x = blue.BlueTestHelperAttributes()

        # Make sure we start with an empty structure list, but more importantly,
        # exercise 'len' on a structure list.
        self.assertEqual(len(x.myStructureList), 0)

        validStructure = (42, 3.14, (1.0, 2.0, 3.0))
        x.myStructureList.append(validStructure)
        self.assertEqual(len(x.myStructureList), 1)

        retrievedStructure = x.myStructureList[0]

        # We can't simply compare the tuples as Python uses double-precision floats
        # and the structure uses single-precision floats
        self.assertEqual(validStructure[0], retrievedStructure[0])
        self.assertAlmostEqual(validStructure[1], retrievedStructure[1], places=6)
        self.assertAlmostEqual(validStructure[2][0], retrievedStructure[2][0], places=6)
        self.assertAlmostEqual(validStructure[2][1], retrievedStructure[2][1], places=6)
        self.assertAlmostEqual(validStructure[2][2], retrievedStructure[2][2], places=6)

        x.myStructureList.clear()
        self.assertEqual(len(x.myStructureList), 0)

        self.assertRaises(TypeError, x.myStructureList.append, 32)
        self.assertRaises(TypeError, x.myStructureList.append, (42, 3.14, 64.7))
        self.assertRaises(TypeError, x.myStructureList.append, "some random string")

    def _appendAndTest(self, structureList, element):
        structureList.append(element)
        self.assertEqual(structureList[len(structureList) - 1], element)
        
    def _appendAndTestFloat32(self, structureList, element):
        structureList.append(element)
        self.assertAlmostEqual(structureList[len(structureList) - 1], element, delta=abs(element) / 10000)
        
    def _appendAndTestFloat16(self, structureList, element):
        structureList.append(element)
        self.assertAlmostEqual(structureList[len(structureList) - 1], element, delta=abs(element) / 100)
        
    def _assignTest(self, structureList, index, value):
        structureList[index] = value
        self.assertEqual(structureList[index], value)
        
    def testUint8List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.uint8, 0)
        self._appendAndTest(x.uint8, 123)
        self._appendAndTest(x.uint8, 250)
        self.assertRaises(TypeError, self._appendAndTest, x.uint8, -3)
        self.assertRaises(TypeError, self._appendAndTest, x.uint8, 257)
        
    def testInt8List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.int8, 25)
        self._appendAndTest(x.int8, -4)
        self.assertRaises(TypeError, self._appendAndTest, x.int8, 200)
        self.assertRaises(TypeError, self._appendAndTest, x.int8, -150)
        
    def testUint16List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.uint16, 0)
        self._appendAndTest(x.uint16, 123)
        self._appendAndTest(x.uint16, 250)
        self.assertRaises(TypeError, self._appendAndTest, x.uint16, -3)
        self.assertRaises(TypeError, self._appendAndTest, x.uint16, 65600)
        
    def testInt16List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.int16, 25)
        self._appendAndTest(x.int16, -4)
        self.assertRaises(TypeError, self._appendAndTest, x.int8, 40000)
        self.assertRaises(TypeError, self._appendAndTest, x.int8, -40000)
        
    def testUint32List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.uint32, 0)
        self._appendAndTest(x.uint32, 123)
        self._appendAndTest(x.uint32, 250)
        self.assertRaises(TypeError, self._appendAndTest, x.uint32, -3)
        self.assertRaises(TypeError, self._appendAndTest, x.uint32, 4294967296)
        
    def testInt16List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.int32, 25)
        self._appendAndTest(x.int32, -4)
        self.assertRaises(TypeError, self._appendAndTest, x.int32, 2147483747)
        self.assertRaises(TypeError, self._appendAndTest, x.int32, -2147483657)
        
    def testFloat32List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.float32, 25)
        self._appendAndTest(x.float32, -4)
        self._appendAndTestFloat32(x.float32, 123.56)
        self._appendAndTestFloat32(x.float32, -19.9)
        
    def testFloat16List(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.float16, 25)
        self._appendAndTest(x.float16, -4)
        self._appendAndTestFloat16(x.float16, 123.56)
        self._appendAndTestFloat16(x.float16, -19.9)
        
    def testStringList(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.string, "")
        self._appendAndTest(x.string, "abc")

    def testMixedList(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.mixed, (0, ("", "")))
        self._appendAndTest(x.mixed, (123, ("abc", "def")))
        self._assignTest(x.mixed, 0, (2, ("", "a string")))
        self.assertRaises(TypeError, self._appendAndTest, x.mixed, 123)
        self.assertRaises(TypeError, self._appendAndTest, x.mixed, (123, (456, "def")))

    def testMatrixList(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.matrix, ((11, 12, 13, 14), (21, 22, 23, 24), (31, 32, 33, 34), (41, 42, 43, 44)))
        self._assignTest(x.matrix, 0, ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)))
        self.assertRaises(TypeError, self._appendAndTest, x.matrix, 123)
        self.assertRaises(TypeError, self._appendAndTest, x.matrix, ())
        self.assertRaises(TypeError, self._appendAndTest, x.matrix, ((), (), (), ()))
        self.assertRaises(TypeError, self._appendAndTest, x.matrix, ((1, 0, "abc", 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)))

    def testBoolList(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.bool, True)
        self._appendAndTest(x.bool, False)
        self._assignTest(x.bool, 0, False)
        self.assertRaises(TypeError, self._appendAndTest, x.bool, 1)
        self.assertRaises(TypeError, self._appendAndTest, x.bool, 0)

    def testWriteToMemStreamAndReadBack(self):
        x = blue.BlueTestStructureLists()
        self._appendAndTest(x.mixed, (123, ("abc", "def")))
        self._appendAndTest(x.mixed, (34, ("", "some string")))

        writer = blue.BlackWriter()
        reader = blue.BlackReader()

        stream = blue.MemStream()
        writer.WriteObjectToStream(x, stream)

        stream.Seek(0)

        y = reader.CreateObjectFromStream(stream)

        self.assertEqual(type(x), type(y))
        self.assertEqual(len(x.mixed), len(y.mixed))
        self.assertEqual(x.mixed[0], y.mixed[0])
        self.assertEqual(x.mixed[1], y.mixed[1])

    def _testReadListFromDict(self, structureListName, items):
        reader = blue.DictReader()
        x = blue.BlueTestStructureLists()
        structureList = getattr(x, structureListName)

        d1 = {
              "type": "BlueTestStructureLists",
              structureListName: {
                                  "structure": structureList.GetStructureDefinition(),
                                  "items": items
                                  }
              }
        y = reader.CreateObject(d1)
        readStructureList = getattr(y, structureListName)
        self.assertEqual(len(readStructureList), len(items))
        for i in range(len(items)):
            self.assertEqual(readStructureList[i], items[i])

    def testReadEmptyStructureListFromEmptyDict(self):
        reader = blue.DictReader()

        d1 = {
              "type": "BlueTestStructureLists",
              "int8": {}
              }
        y = reader.CreateObject(d1)
        readStructureList = getattr(y, "int8")
        self.assertEqual(len(readStructureList), 0)

    def testReadInt8ListFromDict(self):
        self._testReadListFromDict("int8", [])
        self._testReadListFromDict("int8", [1, -2, 3])

    def testReadUInt8ListFromDict(self):
        self._testReadListFromDict("uint8", [])
        self._testReadListFromDict("uint8", [1, 2, 3])

    def testReadInt16ListFromDict(self):
        self._testReadListFromDict("int16", [])
        self._testReadListFromDict("int16", [1, -2, 3])

    def testReadUInt16ListFromDict(self):
        self._testReadListFromDict("uint16", [])
        self._testReadListFromDict("uint16", [1, 2, 3])

    def testReadInt32ListFromDict(self):
        self._testReadListFromDict("int32", [])
        self._testReadListFromDict("int32", [1, -2, 3])

    def testReadUInt32ListFromDict(self):
        self._testReadListFromDict("uint32", [])
        self._testReadListFromDict("uint32", [1, 2, 3])

    def testReadFloat32ListFromDict(self):
        self._testReadListFromDict("float32", [])
        self._testReadListFromDict("float32", [1, -2.5, 3])

    def testReadFloat16ListFromDict(self):
        self._testReadListFromDict("float16", [])
        self._testReadListFromDict("float16", [1, -2.5, 3])

    def testReadStringListFromDict(self):
        self._testReadListFromDict("string", [])
        self._testReadListFromDict("string", ["abc", "", "def"])

    def testReadMatrixListFromDict(self):
        self._testReadListFromDict("matrix", [])
        self._testReadListFromDict("matrix", [((11, 12, 13, 14), (21, 22, 23, 24), (31, 32, 33, 34), (41, 42, 43, 44))])

    def testReadBoolListFromDict(self):
        self._testReadListFromDict("bool", [])
        self._testReadListFromDict("bool", [True, False, True])

    def testReadMixedListFromDict(self):
        self._testReadListFromDict("mixed", [])
        self._testReadListFromDict("mixed", [(123, ("abc", "def"))])

    def _testListYamlPersistence(self, structureListName, items):
        x = blue.BlueTestStructureLists()
        structureList = getattr(x, structureListName)
        for i in items:
            structureList.append(i)

        writer = blue.YamlWriter()
        s = writer.WriteObjectToString(x)

        reader = blue.YamlReader()
        y = reader.CreateObjectFromString(s)
        readStructureList = getattr(y, structureListName)

        self.assertEqual(len(readStructureList), len(items))
        for i in range(len(items)):
            self.assertEqual(readStructureList[i], items[i])

    def testInt8ListYamlPersistence(self):
        self._testListYamlPersistence("int8", [])
        self._testListYamlPersistence("int8", [1, -2, 3])

    def testUInt8ListYamlPersistence(self):
        self._testListYamlPersistence("uint8", [])
        self._testListYamlPersistence("uint8", [1, 2, 3])

    def testInt16ListYamlPersistence(self):
        self._testListYamlPersistence("int16", [])
        self._testListYamlPersistence("int16", [1, -2, 3])

    def testUInt16ListYamlPersistence(self):
        self._testListYamlPersistence("uint16", [])
        self._testListYamlPersistence("uint16", [1, 2, 3])

    def testInt32ListYamlPersistence(self):
        self._testListYamlPersistence("int32", [])
        self._testListYamlPersistence("int32", [1, -2, 3])

    def testUInt32ListYamlPersistence(self):
        self._testListYamlPersistence("uint32", [])
        self._testListYamlPersistence("uint32", [1, 2, 3])

    def testFloat32ListYamlPersistence(self):
        self._testListYamlPersistence("float32", [])
        self._testListYamlPersistence("float32", [1, -2.5, 3])

    def testFloat16ListYamlPersistence(self):
        self._testListYamlPersistence("float16", [])
        self._testListYamlPersistence("float16", [1, -2.5, 3])

    def testStringListYamlPersistence(self):
        self._testListYamlPersistence("string", [])
        self._testListYamlPersistence("string", ["abc", "", "def"])

    def testMatrixListYamlPersistence(self):
        self._testListYamlPersistence("matrix", [])
        self._testListYamlPersistence("matrix", [((11, 12, 13, 14), (21, 22, 23, 24), (31, 32, 33, 34), (41, 42, 43, 44))])

    def testBoolListYamlPersistence(self):
        self._testListYamlPersistence("bool", [])
        self._testListYamlPersistence("bool", [True, False, True])

    def testMixedListYamlPersistence(self):
        self._testListYamlPersistence("mixed", [])
        self._testListYamlPersistence("mixed", [(123, ("abc", "def"))])

    def testListDefinitionWithoutEnumDoesNotExposeChoosers(self):
        x = blue.BlueTestStructureLists()
        self.assertTrue(x.bool.GetStructureDefinition()[0][3] is None)

    def testListDefinitionWithEnumExposesChoosers(self):
        x = blue.BlueTestStructureLists()
        self.assertTrue(isinstance(x.enum.GetStructureDefinition()[0][3], list))

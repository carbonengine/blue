# Copyright © 2023 CCP ehf.

from . import blueunittest
import blue

class TestCopier(blueunittest.TestCase):
    """
    A set of test cases for copying Blue objects.
    """

    def testDefaultValues(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2)


    def testFloatAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.myFloat = 3.14159
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a float attribute failed")


    def testDoubleAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.myDouble = 3.14159267
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a double attribute failed")


    def testBoolAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.myBool = True
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a bool attribute failed")


    def testStringAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.myString = "this is a test"
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a string attribute failed")


    def testUnicodeAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.myUnicode = u"this is a test"
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a unicode string attribute failed")


    def testIntAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.myInt = 42
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying an int attribute failed")


    def testInt64Attribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.myInt64 = 6442450941
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying an int64 attribute failed")


    def testSharedStringAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()
        obj1.sharedString = "this is a shared string"
        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a shared string attribute failed")


    def testVectorAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()

        values = [(42, "fortytwo"), (1984, "nineteeneightyfour"), (0, "nothing")]

        for intVal, stringVal in values:
            childObj = blue.BlueTestHelperAttributes()
            childObj.myInt = intVal
            childObj.myString = stringVal
            obj1.myVector.append(childObj)

        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a vector attribute failed")


    def testDictAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()

        values = {"first": (42, "fortytwo"), "second": (1984, "nineteeneightyfour"), "third": (0, "nothing")}

        for key, val in values.items():
            intVal, stringVal = val
            childObj = blue.BlueTestHelperAttributes()
            childObj.myInt = intVal
            childObj.myString = stringVal
            obj1.myDict[key] = childObj


        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a dict attribute failed")


    def testStructureListAttribute(self):
        obj1 = blue.BlueTestHelperAttributes()

        obj1.myStructureList.append((42, 3.14, (1.0, 2.0, 3.0)))

        obj2 = blue.classes.Copy(obj1)
        self.assertBlueObjectsEqual(obj1, obj2, "Copying a structure list attribute failed")

# Copyright © 2023 CCP ehf.

import sys
import unittest
import blue

MAX_INT32 = 214783647

class TestBlueExposure(unittest.TestCase):
    """
    A set of test cases focusing on Blue exposure.
    """

    """
    Helper functions for basic assignment. These functions are called with an instance
    of either BlueTestHelperAttributes or BlueTestHelperProperties, and are expected
    to behave the same in either case. Pulling the assignments and assertions into these
    helper functions makes it easier to ensure we're doing the same tests for attributes
    and properties.
    """

    def _StringAssignment(self, x):
        x.myString = "this is a test"
        self.assertEqual(x.myString, "this is a test")

        x.myString = u"this is a test"
        self.assertEqual(x.myString, "this is a test")


    def _WStringAssignment(self, x):
        x.myUnicode = "this is a test"
        self.assertEqual(x.myUnicode, u"this is a test")

        x.myUnicode = u"this is a test"
        self.assertEqual(x.myUnicode, u"this is a test")


    def _BoolAssignment(self, x):
        x.myBool = True
        self.assertEqual(x.myBool, True)

        x.myBool = False
        self.assertEqual(x.myBool, False)

        x.myBool = 42
        self.assertEqual(x.myBool, True)

        x.myBool = 0
        self.assertEqual(x.myBool, False)

        def AssignFloat_ErrorExpected():
            x.myBool = 3.14
        
        self.assertRaises(TypeError, AssignFloat_ErrorExpected)

        def AssignString_ErrorExpected():
            x.myBool = "True"
        
        self.assertRaises(TypeError, AssignString_ErrorExpected)


    def _IntAssignment(self, x):

        values = [0, 63, -1, MAX_INT32, -MAX_INT32-1, True, False]

        for each in values:
            x.myInt = each
            self.assertEqual(x.myInt, each, "Failed to assign %d" % each)

        def AssignLong_ErrorExpected():
            x.myInt = sys.maxsize + 1
        
        self.assertRaises(OverflowError, AssignLong_ErrorExpected)

        def AssignFloat_ErrorExpected():
            x.myInt = 3.14
        
        self.assertRaises(TypeError, AssignFloat_ErrorExpected)

        def AssignString_ErrorExpected():
            x.myInt = "this is not a number"
        
        self.assertRaises(TypeError, AssignString_ErrorExpected)


    def _Int64Assignment(self, x):
        
        values = [0, 63, -1, MAX_INT32, -MAX_INT32-1, 3, MAX_INT32*2, -MAX_INT32*2]

        for each in values:
            x.myInt64 = each
            self.assertEqual(x.myInt64, each)

        def AssignFloat_ErrorExpected():
            x.myInt64 = 3.14
        
        self.assertRaises(TypeError, AssignFloat_ErrorExpected)

        def AssignString_ErrorExpected():
            x.myInt64 = "this is not a number"
        
        self.assertRaises(TypeError, AssignString_ErrorExpected)


    def _FloatAssignment(self, x):
        
        values = [0.0, 3.14, -2.77, 0, 42, -3]

        for each in values:
            x.myFloat = each

            # Note that there are numerical precision issues here as the Blue
            # objects use floats rather than doubles
            self.assertAlmostEqual(x.myFloat, each, places=6)

        def AssignString_ErrorExpected():
            x.myFloat = "this is not a number"
        
        self.assertRaises(TypeError, AssignString_ErrorExpected)


    def _DoubleAssignment(self, x):
    
        values = [0.0, 3.14, -2.77, 0, 42, -3]

        for each in values:
            x.myDouble = each
            self.assertEqual(x.myDouble, each)

        def AssignString_ErrorExpected():
            x.myDouble = "this is not a number"
        
        self.assertRaises(TypeError, AssignString_ErrorExpected)


    def _BoolAssignment(self, x):
    
        x.myBool = True
        self.assertEqual(x.myBool, True)

        x.myBool = False
        self.assertEqual(x.myBool, False)

        x.myBool = 42
        self.assertEqual(x.myBool, True)

        x.myBool = 0
        self.assertEqual(x.myBool, False)

        def AssignFloat_ErrorExpected():
            x.myBool = 3.14
        
        self.assertRaises(TypeError, AssignFloat_ErrorExpected)

        def AssignString_ErrorExpected():
            x.myBool = "true"
        
        self.assertRaises(TypeError, AssignString_ErrorExpected)


    """
    And here come the test functions themselves.
    """

    def testStringAttributeAssignment(self):
        x = blue.BlueTestHelperAttributes()

        self._StringAssignment(x)


    def testStringPropertyAssignment(self):
        x = blue.BlueTestHelperProperties()

        self._StringAssignment(x)


    def testWStringAttributeAssignment(self):
        x = blue.BlueTestHelperAttributes()

        self._WStringAssignment(x)


    def testWStringPropertyAssignment(self):
        x = blue.BlueTestHelperProperties()

        self._WStringAssignment(x)


    def testBoolAttributeAssignment(self):
        x = blue.BlueTestHelperAttributes()

        self._BoolAssignment(x)


    def testIntAttributeAssignment(self):
        x = blue.BlueTestHelperAttributes()

        self._IntAssignment(x)


    def testInt64AttributeAssignment(self):
        x = blue.BlueTestHelperAttributes()

        self._Int64Assignment(x)


    def testFloatAttributeAssignment(self):
        x = blue.BlueTestHelperAttributes()

        self._FloatAssignment(x)


    def testDoubleAttributeAssignment(self):
        x = blue.BlueTestHelperAttributes()

        self._DoubleAssignment(x)


    def testBoolPropertyAssignment(self):
        x = blue.BlueTestHelperProperties()

        self._BoolAssignment(x)


    def testIntPropertyAssignment(self):
        x = blue.BlueTestHelperProperties()

        self._IntAssignment(x)


    def testInt64PropertyAssignment(self):
        x = blue.BlueTestHelperProperties()

        self._Int64Assignment(x)


    def testFloatPropertyAssignment(self):
        x = blue.BlueTestHelperProperties()

        self._FloatAssignment(x)


    def testDoublePropertyAssignment(self):
        x = blue.BlueTestHelperProperties()

        self._DoubleAssignment(x)


    def _verifyDir(self, x):
        members = dir(x)

        self.assertTrue( "myInt" in members )
        self.assertTrue( "myFloat" in members )
        self.assertTrue( "myDouble" in members )
        self.assertTrue( "myBool" in members )
        self.assertTrue( "myInt64" in members )
        self.assertTrue( "myString" in members )
        self.assertTrue( "myUnicode" in members )

    
    def testDirAttributes(self):
        x = blue.BlueTestHelperAttributes()

        self._verifyDir(x)


    def testDirAttributes(self):
        x = blue.BlueTestHelperProperties()

        self._verifyDir(x)

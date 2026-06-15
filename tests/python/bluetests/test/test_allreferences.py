# Copyright © 2024 CCP ehf.

import unittest
import blue

class TestAllReferences(unittest.TestCase):
    """
    A set of test cases for AllReferences object.
    """

    def testFindsReferences(self):
        root = blue.BlueTestHelperAttributes()
        obj = blue.BlueTestHelperAttributes()
        root.myVector.append(obj)
        refs = blue.AllReferences()
        refs.SetRoot(root)
        while True:
            if refs.Update(1):
                break
        self.assertEqual(refs.GetReferences(obj), [(root.myVector, 1, 0)])
        self.assertEqual(refs.GetReferences(root.myVector), [(root, 0, 'myVector')])

    def testFindsInterfaces(self):
        root = blue.BlueTestHelperAttributes()
        obj = blue.BlueTestHelperAttributes()
        root.myVector.append(obj)
        refs = blue.AllReferences()
        refs.SetRoot(root)
        while True:
            if refs.Update(1):
                break
        self.assertEqual(set(refs.FindInterface(root, 'BlueTestHelperAttributes')), {root, obj})
        self.assertEqual(refs.FindInterface(obj, 'BlueTestHelperAttributes'), [obj])

    def testKeepsStrongReferences(self):
        root = blue.BlueTestHelperAttributes()
        root.myVector.append(blue.BlueTestHelperAttributes())
        obj = blue.BluePythonWeakRef(root.myVector[0])
        refs = blue.AllReferences()
        refs.SetRoot(root)
        while True:
            if refs.Update(1):
                break
        del root.myVector[:]
        self.assertTrue(obj.object)

    def testClearsReferences(self):
        root = blue.BlueTestHelperAttributes()
        root.myVector.append(blue.BlueTestHelperAttributes())
        obj = blue.BluePythonWeakRef(root.myVector[0])
        refs = blue.AllReferences()
        refs.SetRoot(root)
        while True:
            if refs.Update(1):
                break
        del root.myVector[:]
        while True:
            if refs.Update(1):
                break
        self.assertFalse(obj.object)

    def testDoesNotLeak(self):
        root = blue.BlueTestHelperAttributes()
        root.myVector.append(blue.BlueTestHelperAttributes())
        obj = blue.BluePythonWeakRef(root.myVector[0])
        refs = blue.AllReferences()
        refs.SetRoot(root)
        while True:
            if refs.Update(1):
                break
        del root.myVector[:]
        del refs
        self.assertFalse(obj.object)

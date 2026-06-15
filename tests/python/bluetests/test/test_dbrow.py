# Copyright © 2024 CCP ehf.

from . import blueunittest
import blue


class TestDBRow(blueunittest.TestCase):
    """
    A set of test cases for the DBRow class.
    """

    def setUp(self):
        self.columns = (("nodeID", 4), ("ipAddress", 129), ("port", 3))
        self.row = blue.DBRow(blue.DBRowDescriptor(self.columns))

    def testVirtualColumnsAreReadOnly(self):
        def getFoo(*args, **kwargs):
            return 4711
        def setFoo(*args, **kwargs):
            pass
        desc = blue.DBRowDescriptor(self.columns)
        with self.assertRaises(TypeError):
            desc.virtual = [("foo", getFoo, setFoo)]
        desc.virtual = [("foo", getFoo)]
        row = blue.DBRow(desc)
        self.assertEqual(row.foo, 4711)

    def testAttemptingToDeleteColumnDoesntCrash(self):
        foo = self.row
        with self.assertRaises(RuntimeError):
            delattr(foo, "nodeID")
        with self.assertRaises(RuntimeError):
            del foo.ipAddress

    def testSliceSubscript(self):
        sliceTest = self.row[:]

        self.assertIsInstance(sliceTest, list)
        self.assertEqual(self.row, blue.DBRow(blue.DBRowDescriptor(self.columns), sliceTest))
        self.assertListEqual(sliceTest, [0.0, None, 0])
        self.assertListEqual(self.row[:1], [0.0])
        self.assertListEqual(self.row[1:], [None, 0])
        self.assertListEqual(self.row[1:2], [None])
        self.assertListEqual(self.row[:-1], [0.0, None])
        self.assertListEqual(self.row[0:], [0.0, None, 0])

        with self.assertRaises(NotImplementedError):
            self.row[::2]

    def testIndexSubscript(self):
        self.assertEqual(self.row[0], 0.0)
        self.assertEqual(self.row[-1], 0)

        with self.assertRaises(IndexError):
            self.row[len(self.row)]

    def testUnicodeSubscript(self):
        self.assertEqual(self.row["nodeID"], 0)

        with self.assertRaises(KeyError):
            self.row["test"]

    def testEquivalencyComparison(self):
        self.assertFalse(self.row == None)
        self.assertNotEqual(self.row, None)

        self.assertFalse(self.row == 1)
        self.assertNotEqual(self.row, 1)

        self.assertFalse(self.row == blue.DBRowDescriptor(self.columns))
        self.assertNotEqual(self.row, blue.DBRowDescriptor(self.columns))

        self.assertEqual(self.row, blue.DBRow(blue.DBRowDescriptor(self.columns)))

    def testColumnNameConstraints(self):
        invalid_column_names = (
            # Python's magic attributes can lead to type confusion and other interesting bits.
            # Therefore, disallow column names starting with a double underscore.
            "__this_might_be_a_python_magic_attribute",
            # An actual magic attribute
            "__class__",
        )
        for invalid_column_name in invalid_column_names:
            with self.assertRaises(ValueError):
                blue.DBRowDescriptor(((invalid_column_name, 0x80),))

        # However, a single underscore as starting character is fine
        _ = blue.DBRowDescriptor((("_dummy", 0x80),))

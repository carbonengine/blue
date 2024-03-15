from . import blueunittest
import blue

class TestDBRow(blueunittest.TestCase):
    """
    A set of test cases for the DBRow class.
    """

    def setUp(self):
        self.columns = (("nodeID", 4), ("ipAddress", 129), ("port", 3))
        self.row = blue.DBRow(blue.DBRowDescriptor(self.columns))

    def test_sliceSubscript(self):
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

    def test_indexSubscript(self):
        self.assertEqual(self.row[0], 0.0)
        self.assertEqual(self.row[-1], 0)

        with self.assertRaises(IndexError):
            self.row[len(self.row)]

    def test_unicodeSubscript(self):
        self.assertEqual(self.row["nodeID"], 0)

        with self.assertRaises(KeyError):
            self.row["test"]

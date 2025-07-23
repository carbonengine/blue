from . import blueunittest
import blue


class TestDBRow(blueunittest.TestCase):
    """
    A set of test cases for the DBRow class.
    """

    def setUp(self):
        self.columns = (("nodeID", 4), ("ipAddress", 129), ("port", 3))
        self.row = blue.DBRow(blue.DBRowDescriptor(self.columns))

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

    def testPythonObjectValidation(self):
        objectStoringRow = blue.DBRow(blue.DBRowDescriptor((("text", 128),("unicodeText", 129),("data", 130))))

        for illegalValue in (
            object(),
            4711,
            0.815,
            (1,2,3,),
            ['a','b', 'c']
        ):
            with self.assertRaises(TypeError):
                setattr(objectStoringRow, "text", illegalValue)
            with self.assertRaises(TypeError):
                setattr(objectStoringRow, "unicodeText", illegalValue)
            with self.assertRaises(TypeError):
                setattr(objectStoringRow, "data", illegalValue)

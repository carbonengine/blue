# Copyright © 2023 CCP ehf.

import unittest
import blue


class TestBlue(unittest.TestCase):
    """
    A set of test cases for Blue.
    """

    def test_classes(self):
        """
        Test access to the 'classes' object
        """

        # Is it even there?
        self.assertTrue(blue.classes)

        # Is live count active?
        d = blue.classes.LiveCount()
        self.assertTrue(len(d))

        self.assertEqual(d["blue.BlueTestHelperAttributes"], 0)

        obj = blue.BlueTestHelperAttributes()

        d = blue.classes.LiveCount()
        self.assertEqual(d["blue.BlueTestHelperAttributes"], 1)

        del obj

        d = blue.classes.LiveCount()
        self.assertEqual(d["blue.BlueTestHelperAttributes"], 0)

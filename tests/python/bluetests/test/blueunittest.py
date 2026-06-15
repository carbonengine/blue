# Copyright © 2023 CCP ehf.

import unittest
import blue


class TestCase(unittest.TestCase):
    def assertBlueObjectsEqual(self, obj1, obj2, msg="Blue objects are not identical"):
        """
        Assert that two Blue objects are equal. 
        """
        self.assertEqual(type(obj1), type(obj2))

        if isinstance(obj1, blue.BlueWrapper):
            for attributeName in obj1.__dict__:
                attr1 = getattr(obj1, attributeName)
                attr2 = getattr(obj2, attributeName)

                self.assertEqual(type(attr1), type(attr2))

                if isinstance(attr1, blue.BlueWrapper):
                    self.assertBlueObjectsEqual(attr1, attr2)

                else:
                    if attributeName != "__iroot__":
                        self.assertEqual(attr1, attr2)
        else:
            self.assertEqual(obj1, obj2)

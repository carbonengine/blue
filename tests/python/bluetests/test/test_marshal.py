__author__ = 'snorri.sturluson'

from . import blueunittest
import blue
import sys

class EmptyObject:
    def __eq__(self, other):
        return isinstance(other, type(self))


class CallbackSerializedObject:
    def __eq__(self, other):
        return isinstance(other, type(self))


class SimpleObject:
    def __init__(self):
        self.a = "this is a string"
        self.b = 42
        self.c = 3.14159267

    def __eq__(self, other):
        return isinstance(other, type(self)) and self.a == other.a and self.b == other.b and self.c == other.c


def SaveCallback(obj):
    if isinstance(obj, CallbackSerializedObject):
        return "magic"
    return None


def LoadCallback(obj):
    if obj == "magic":
        return CallbackSerializedObject()
    return None


class testMarshal(blueunittest.TestCase):
    loaded = []
    saved = []

    @classmethod
    def setUpClass(cls):
        blue.marshal.ResetTypeStats()
        cls.loaded = [0]*48
        cls.saved = [0]*48

    @classmethod
    def tearDownClass(cls):
        IGNORE_TYPES = [0, 3, 13, 16, 12, 24, 26, 29, 30, 33]
        for i in range(48):
            if i in IGNORE_TYPES:
                continue
            if cls.loaded[i] == 0:
                print("Missing coverage for type %d when loading" % i, file=sys.stderr)
            if cls.saved[i] == 0:
                print("Missing coverage for type %d when saving" % i, file=sys.stderr)

    def _update_coverage(self):
        typeStats = blue.marshal.GetTypeStats()
        for i in range(48):
            self.loaded[i] += typeStats[0][i]
            self.saved[i] += typeStats[1][i]

    def verify_round_trip(self, obj):
        blue.marshal.ResetTypeStats()
        s = blue.marshal.Save(obj)
        obj2 = blue.marshal.Load(s)
        self.assertBlueObjectsEqual(obj, obj2)
        typeStats = blue.marshal.GetTypeStats()
        self.assertEqual(typeStats[0], typeStats[1])
        self._update_coverage()

    def test_none(self):
        self.verify_round_trip(None)

    def test_empty_string(self):
        self.verify_round_trip("")

    def test_string(self):
        self.verify_round_trip("this is a test")

    def test_string_from_stringtable(self):
        self.verify_round_trip("ballID")

    def test_empty_unicode(self):
        self.verify_round_trip(u"")

    def test_single_char_unicode(self):
        self.verify_round_trip(u"A")

    def test_unicode(self):
        self.verify_round_trip(u"\u20A8\u20B1\u20A9")

    def test_unicode_as_utf8(self):
        self.verify_round_trip(u"this is a unicode test")

    def test_integer(self):
        self.verify_round_trip(0)
        self.verify_round_trip(1)
        self.verify_round_trip(-1)
        self.verify_round_trip(42)
        self.verify_round_trip(32767)
        self.verify_round_trip(2147483647)
        self.verify_round_trip(-2147483648)

    def test_long(self):
        self.verify_round_trip(42)
        self.verify_round_trip(9223372036854775807)
        self.verify_round_trip(9223372036854775808)

    def test_float(self):
        self.verify_round_trip(0.0)
        self.verify_round_trip(3.14159267)
        self.verify_round_trip(-2.781431508934509809834)

    def test_bool(self):
        self.verify_round_trip(True)
        self.verify_round_trip(False)

    def test_empty_dict(self):
        self.verify_round_trip({})

    def test_empty_object(self):
        self.verify_round_trip(EmptyObject())

    def test_simple_object(self):
        self.verify_round_trip(SimpleObject())

    def test_empty_list(self):
        self.verify_round_trip([])

    def test_list_of_one_string(self):
        self.verify_round_trip(["this is a test"])

    def test_list_of_strings(self):
        self.verify_round_trip(["this", "is", "a", "test"])

    def test_empty_tuple(self):
        self.verify_round_trip(())

    def test_tuple_of_one_string(self):
        self.verify_round_trip(("this is a test",))

    def test_tuple_of_two_strings(self):
        self.verify_round_trip(("this is", "a test"))

    def test_tuple_of_strings(self):
        self.verify_round_trip(("this", "is", "a", "test"))

    def test_instanced_object(self):
        obj = SimpleObject()
        self.verify_round_trip([obj, obj, obj])

    def test_callback(self):
        obj = [CallbackSerializedObject(), SimpleObject(), "this is a test"]
        s = blue.marshal.Save(obj, callback=SaveCallback)
        obj2 = blue.marshal.Load(s, callback=LoadCallback)
        self.assertBlueObjectsEqual(obj, obj2)
        typeStats = blue.marshal.GetTypeStats()
        self.assertEqual(typeStats[0], typeStats[1])
        self._update_coverage()

    def test_checksum(self):
        obj = [SimpleObject(), "this is a test"]
        s = blue.marshal.Save(obj, useChecksum=1)
        obj2 = blue.marshal.Load(s)
        self.assertBlueObjectsEqual(obj, obj2)
        typeStats = blue.marshal.GetTypeStats()
        self.assertEqual(typeStats[0], typeStats[1])
        self._update_coverage()

    def test_empty_dbrow(self):
        rd = blue.DBRowDescriptor(())
        d = blue.DBRow(rd)
        self.verify_round_trip(d)

    def test_wstream(self):
        obj = [SimpleObject(), "this is a test"]
        ws = blue.marshal.Save(obj)
        self.verify_round_trip(ws)

    def test_converting_to_bytes_does_not_crash(self):
        obj = [SimpleObject(), SimpleObject(), "this is a test"]
        ws = blue.marshal.Save(obj)
        self.assertIsInstance(bytes(ws), bytes)

    def test_can_slice(self):
        obj = [SimpleObject(), "this is a test"]
        ws = blue.marshal.Save(obj)
        self.assertIsInstance(ws[:5], bytes)
        self.assertEqual(len(ws[3:6]), 3)

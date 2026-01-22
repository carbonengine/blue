__author__ = 'snorri.sturluson'

from . import blueunittest
import blue

import unittest
import sys
import os

class EmptyObject(object):
    def __eq__(self, other):
        return isinstance(other, type(self))


class NewStyleObject(object):
    def __init__(self):
        self.a = "this is a string"
        self.b = b"this is a string"
        self.c = u"this is a string"
        self.d = 42
        self.e = 3.14159267

    def __eq__(self, other):
        # String, byte and unicode comparisons are type-agnostic
        # Therefore, an unmarshalled Python3 NewStyleObject instance should compare truthfully
        # even though d and e fields differ in type
        return isinstance(self, type(other)) and self.__dict__ == other.__dict__


class OldStyleObject:
    def __init__(self):
        self.a = "this is a string"
        self.b = b"this is a string"
        self.c = u"this is a string"
        self.d = 42
        self.e = 3.14159267

    def __eq__(self, other):
        return isinstance(self, type(other)) and self.__dict__ == other.__dict__


def SaveCallback(obj):
    if isinstance(obj, OldStyleObject):
        return "magic"
    return None


def LoadCallback(obj):
    if obj == "magic":
        return OldStyleObject()
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
                sys.stderr.write("Missing coverage for type %d when loading" % i)
            if cls.saved[i] == 0:
                sys.stderr.write("Missing coverage for type %d when saving" % i)

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
        self.verify_round_trip(9223372036854775807L)

    def test_float(self):
        self.verify_round_trip(0.0)
        self.verify_round_trip(3.14159267)
        self.verify_round_trip(-2.781431508934509809834)

    def test_bool(self):
        self.verify_round_trip(True)
        self.verify_round_trip(False)

    def test_empty_dict(self):
        self.verify_round_trip({})

    def test_dict(self):
        self.verify_round_trip({"key": "test"})

    def test_empty_object(self):
        self.verify_round_trip(EmptyObject())

    def test_new_style_object(self):
        self.verify_round_trip(NewStyleObject())

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
        obj = NewStyleObject()
        self.verify_round_trip([obj, obj, obj])

    def test_instanced_old_style_object(self):
        obj = OldStyleObject()
        self.verify_round_trip([obj, obj, obj])

    def test_callback(self):
        obj = [OldStyleObject(), NewStyleObject(), "this is a test"]
        s = blue.marshal.Save(obj, callback=SaveCallback)
        obj2 = blue.marshal.Load(s, callback=LoadCallback)
        self.assertBlueObjectsEqual(obj, obj2)
        typeStats = blue.marshal.GetTypeStats()
        self.assertEqual(typeStats[0], typeStats[1])
        self._update_coverage()

    def test_checksum(self):
        obj = [OldStyleObject(), NewStyleObject(), "this is a test"]
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
        obj = [OldStyleObject(), NewStyleObject(), "this is a test"]
        ws = blue.marshal.Save(obj)
        self.verify_round_trip(ws)

    def test_dbrow(self):
        rowDesc = blue.DBRowDescriptor((("Test", 20),))
        sourceRow = blue.DBRow(rowDesc, (123, ))
        self.verify_round_trip(sourceRow)

    def test_dbrow_with_invalid_descriptor_in_stream_raises_error(self):
        # Unmarshalled bytes will attempt to create a DBRow and create a blue.Dict rather than
        # expected blue.DBRowDescriptor
        bytes = b'~\x00\x00\x00\x00*",\x02\tblue.Dict$--'
        with self.assertRaises(RuntimeError) as raisedValue:
            blue.marshal.Load(bytes)

        self.assertEqual(raisedValue.exception.args[0], TypeError)

    def test_nullptr_deref_in_readobjectreference(self):
        known_bad_payloads = (
            b"\x7D\x01\x57\x1B\x00",  # entering via `TY_INSTANCE`
            b"\x7D\x01\x62\x1B\x00",  # entering via `TY_REDUCE`
            b"\x7D\x01\x63\x1B\x00",  # entering via `TY_NEWOBJ`
        )
        for bad_payload in known_bad_payloads:
            with self.assertRaises(RuntimeError):
                blue.marshal.Load(bad_payload)

class TestBackwardsCompatibility(blueunittest.TestCase):
    """
    This class adds coverage for objects marshalled in Python 3.
    """
    def test_load_old_style_object(self):
        bytes = b'~\x00\x00\x00\x00#,%\x02*bluetests.test.test_marshal.OldStyleObject\x16\x05.\x10this is a string.\x01a\x13\x10this is a string.\x01b.\x10this is a string.\x01c\x06*.\x01d\n\xcd\x06xV\xfb!\t@.\x01e--'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, OldStyleObject())

    def test_none(self):
        bytes = b'~\x00\x00\x00\x00\x01'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, None)

    def test_string_from_stringtable(self):
        bytes = b'~\x00\x00\x00\x00\x11\x06'
        loaded = blue.marshal.Load(bytes)

        # We expect a str type constructed from marshalled string table index
        self.assertIsInstance(loaded, str)
        self.assertEqual(loaded, "ballID")

    def test_empty_unicode(self):
        bytes = b'~\x00\x00\x00\x00('
        loaded = blue.marshal.Load(bytes)

        self.assertIsInstance(loaded, unicode)
        self.assertEqual(loaded, "")

    def test_single_char_unicode(self):
        bytes = b'~\x00\x00\x00\x00.\x01A'
        loaded = blue.marshal.Load(bytes)

        self.assertIsInstance(loaded, unicode)
        self.assertEqual(loaded, "A")

    def test_unicode(self):
        bytes = b'~\x00\x00\x00\x00.\t\xe2\x82\xa8\xe2\x82\xb1\xe2\x82\xa9'
        loaded = blue.marshal.Load(bytes)

        self.assertIsInstance(loaded, unicode)
        self.assertEqual(loaded, u"\u20A8\u20B1\u20A9")

    def test_unicode_as_utf8(self):
        bytes = b'~\x00\x00\x00\x00.\x16this is a unicode test'
        loaded = blue.marshal.Load(bytes)

        self.assertIsInstance(loaded, unicode)
        self.assertEqual(loaded, "this is a unicode test")

    def test_integer(self):
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x08'), 0)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\t'), 1)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x07'), -1)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x06*'), 42)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x05\xff\x7f'), 32767)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x04\xff\xff\xff\x7f'), 2147483647)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x04\x00\x00\x00\x80'), -2147483648)

    def test_long(self):
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00/\x08\xff\xff\xff\xff\xff\xff\xff\x7f'), 9223372036854775807)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00/\t\x00\x00\x00\x00\x00\x00\x00\x80\x00'), 9223372036854775808)

    def test_float(self):
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x0b'), 0.0)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\n\xcd\x06xV\xfb!\t@'), 3.14159267)
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\nO\x80\xb7)_@\x06\xc0'), -2.781431508934509809834)

    def test_bool(self):
        self.assertTrue(blue.marshal.Load(b'~\x00\x00\x00\x00\x1f'), True)
        self.assertFalse(blue.marshal.Load(b'~\x00\x00\x00\x00 '), False)

    def test_empty_dict(self):
        self.assertEqual(blue.marshal.Load(b'~\x00\x00\x00\x00\x16\x00'), {})

    def test_dict(self):
        bytes = b'~\x00\x00\x00\x00\x16\x01.\x04test.\x03key'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, {"key": "test"})
        # Explicit type checking due to Unicode and str types being implicitly comparable
        for key, value in loaded.items():
            self.assertIsInstance(key, unicode)
            self.assertIsInstance(value, unicode)

    def test_empty_object(self):
        bytes = b"~\x00\x00\x00\x00#%%\x02'bluetests.test.test_marshal.EmptyObject--"
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, EmptyObject())

    def test_new_style_object(self):
        bytes = b'~\x00\x00\x00\x00#,%\x02*bluetests.test.test_marshal.NewStyleObject\x16\x05.\x10this is a string.\x01a\x13\x10this is a string.\x01b.\x10this is a string.\x01c\x06*.\x01d\n\xcd\x06xV\xfb!\t@.\x01e--'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, NewStyleObject())

    def test_empty_list(self):
        bytes = b'~\x00\x00\x00\x00&'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, [])

    def test_list_of_one_string(self):
        bytes = b"~\x00\x00\x00\x00'.\x0ethis is a test"
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, ["this is a test"])
        self.assertIsInstance(loaded[0], unicode)

    def test_list_of_strings(self):
        bytes = b'~\x00\x00\x00\x00\x15\x04.\x04this.\x02is.\x01a.\x04test'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, ["this", "is", "a", "test"])
        for item in loaded:
            self.assertIsInstance(item, unicode)

    def test_empty_tuple(self):
        bytes = b'~\x00\x00\x00\x00$'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, ())

    def test_tuple_of_one_string(self):
        bytes = b'~\x00\x00\x00\x00%.\x0ethis is a test'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, ("this is a test",))
        self.assertIsInstance(loaded[0], unicode)

    def test_tuple_of_two_strings(self):
        bytes = b'~\x00\x00\x00\x00,.\x07this is.\x06a test'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, ("this is", "a test"))
        for item in loaded:
            self.assertIsInstance(item, unicode)

    def test_tuple_of_strings(self):
        bytes = b'~\x00\x00\x00\x00\x14\x04.\x04this.\x02is.\x01a.\x04test'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, ("this", "is", "a", "test"))
        for item in loaded:
            self.assertIsInstance(item, unicode)

    def test_instanced_object(self):
        bytes = b'~\x01\x00\x00\x00\x15\x03c,%\x02*bluetests.test.test_marshal.NewStyleObject\x16\x05.\x10this is a string.\x01a\x13\x10this is a string.\x01b.\x10this is a string.\x01c\x06*.\x01d\n\xcd\x06xV\xfb!\t@.\x01e--\x1b\x01\x1b\x01\x01\x00\x00\x00'
        loaded = blue.marshal.Load(bytes)
        instance = NewStyleObject()

        self.assertEqual(loaded, [instance, instance, instance])

    def test_instanced_old_style_object(self):
        bytes = b'~\x01\x00\x00\x00\x15\x03c,%\x02*bluetests.test.test_marshal.OldStyleObject\x16\x05.\x10this is a string.\x01a\x13\x10this is a string.\x01b.\x10this is a string.\x01c\x06*.\x01d\n\xcd\x06xV\xfb!\t@.\x01e--\x1b\x01\x1b\x01\x01\x00\x00\x00'
        loaded = blue.marshal.Load(bytes)
        instance = OldStyleObject()

        self.assertEqual(loaded, [instance, instance, instance])

    def test_read_callback_called(self):
        def read_callback(obj):
            read_callback.called = True
        read_callback.called = False

        bytes = b'~\x00\x00\x00\x00\x19.\x04test'
        blue.marshal.Load(bytes, callback=read_callback)
        self.assertTrue(read_callback.called)

    def test_checksum(self):
        # Marshalled Python3 object using checksum
        bytes = b'~\x00\x00\x00\x00\x1c\xb9/\x0fL\x15\x02#,%\x02*bluetests.test.test_marshal.NewStyleObject\x16\x05.\x10this is a string.\x01a\x13\x10this is a string.\x01b.\x10this is a string.\x01c\x06*.\x01d\n\xcd\x06xV\xfb!\t@.\x01e--.\x0ethis is a test'
        loaded = blue.marshal.Load(bytes)
        comparison = blue.marshal.Save([NewStyleObject(), "this is a test"], useChecksum=1)

        # Marshalled data will differ due to string fields, so we must load both objects for comparison
        self.assertBlueObjectsEqual(loaded, blue.marshal.Load(comparison))

    def test_empty_dbrow(self):
        bytes = b'~\x00\x00\x00\x00*",\x02\x14blue.DBRowDescriptor%$--\x00'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, blue.DBRow(blue.DBRowDescriptor(())))

    def test_dbrow(self):
        bytes = b'~\x00\x00\x00\x00*",\x02\x14blue.DBRowDescriptor%%,.\x04Test\x06\x14--\x02\xf7{'
        loaded = blue.marshal.Load(bytes)

        self.assertEqual(loaded, blue.DBRow(blue.DBRowDescriptor((("Test", 20),)), (123, )))

    def test_set(self):
        blue.marshal.globalsWhitelist = {set: None}
        blue.marshal.collectWhitelist = False
        bytes = b'~\x00\x00\x00\x00",\x02\x0cbuiltins.set%\x15\x03\t\x06\x02\x06\x03--'
        loaded = blue.marshal.Load(bytes)
        self.assertSetEqual(loaded, {1, 2, 3})

    def test_runtime_error(self):
        blue.marshal.globalsWhitelist = {RuntimeError: None}
        blue.marshal.collectWhitelist = False
        bytes = b'~\x00\x00\x00\x00",\x02\x15builtins.RuntimeError%.\x05Boom!--'
        loaded = blue.marshal.Load(bytes)
        self.assertIsInstance(loaded, RuntimeError)
        self.assertIsInstance(loaded.message, unicode)
        self.assertEqual(loaded.message, u"Boom!")

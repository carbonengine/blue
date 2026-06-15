# Copyright © 2023 CCP ehf.

__author__ = 'snorri.sturluson'

from . import blueunittest
import blue

import unittest
import sys
import os

class EmptyObject:
    def __eq__(self, other):
        return isinstance(other, type(self))


class SimpleObject:
    def __init__(self):
        self.a = "this is a string"
        self.b = 42
        self.c = 3.14159267
        self.d = b'x\x01\x8d\x98{t\xd3' #'\xe5\x19\xc7\x81\x16<\xbc^\xa0\xb5  BJ\x93\x16B\xdf6I\x9b6\xf9\x95B\xf3&\xcd\xb5M\xd2[\xdad\xe0ZJo\x87\xde\xe8\x05(^(\x88\xc8\xa6N&P\x11&V&\x0e\xc5\xa1s\xf5\x1c\xe4(k_\xb5 0E\xe6\x18\xf3\xb2\x15\xc5C\xc51\x99:\xafL\xf7\xfc\xde\xf6\ti\x9bb\xff\t\xb4\xdf\xd3\xcfyn\xef\xf3}\xdf\xdc=~\xdc\xb8qq\x89\xaa\t\xc9\xe5eM\xcb\x1b\xea\x93\xca\x1b\xea\xea\xe0\x9f\xe6\xf2\xa6\x9a\xc6\x96\xa4\xe6\xb6\xe6\xa4\xf2\xa6\x865\xcd\x15-IfG\xfd\x8a\x8a\xb5\x15+\xf2\xc5\x8f\xd3\',O\x9c\x10\xb3\xbc\xb6\xb5"\xc9\xc2\xe0W\x96\x8a\x81?ihR\xc5LN\x8c\xda=)"1\xfa\xfa\xe6\x96\xb2\x96\x9a\x86zwY]\xc5\xc4\x8d\xe3\x12\x17\xac\x9d41qA\x9b\xfc\xb1\x0e>\xa2o\x1c\xd4\x0b\xdb\x1a+\x1c\x16\xf8\x8b\xa8\x0e\xf9c\xb1\xfc\xb1dR\x04\xa5Q\xca\xa8\xdd\xd1\xa4\xbc\xa1\xb6\xb5N@h\xe4\x8d=\xdb"\xd4\xb7\x8e\x9f\'Mt\x9c\xbb\x9fK\'\xdf\x7f\xa6\x82\x915O|r?\x03eG\xc4\xe9\xe4\xae\xd7_{c\xfc\xbe\xa3lJ\xf4\x02g\xc0G\xad\xa9\n\x9f\x82*r\x1b\x1a\xea\x15Z\xf9?e\xf5\xad\x95e\xe5-\xadM5\xf5U\nOkKcCsK\xe4MH}n\xdf\xa2M\\\xda\xa9\xfew>#\x1b\x1e,\xde\xc8A\x11\xd4\xd2\xd7\x07\xa9\x89\x1aj3\xb8=\n\x9f\xc3\xe1\xf8i\xf0\x14\x04\x1f\xd8\x13\x7f\x1f\x97\xb2\xbe\xdf\x13\xcf\x89i\xef\xef7qP\x04\xb8\x1b\xc1\x0bSi\xb65\xa7H\x06#7e\xd4\x80\xa7\x0ep\xe3\xa4\xe5\x1f\xach\xe4R\xa7\xfe\x88\x96On?\xdc\xf1\xd0#lj\xcf\xf6mr\x1d~@\xb0\xda\x9bNS\xf5>\x85#X\x88\xd1\xb9Q\x18\xaf\xb73\xe0fR\xfb\x82}\xb3\x18\xd9\x1f\x88/\xe0\xa0\x88x\x9d\xbdX^\x9d\xc1o\xa4\xde\xb1\x947z\x80\x1a+u\xbf"\xa9\xb9\xab\xbe\xfcr\x0f\xe9\x8bZ"1\x10\x04\xf4\x00B\xd5\xeet\xca\x1c\xfe\x90XGo\xda\xcd\x18\xeb#\x8b\x9e\xbb\x87I\x9d_i$F\xae\xfc\xad\xa3\x96\x81"\xb0?\x06\xb1\x8e\\\x17ui\x15\x8e\x12,\xed\xe8\xd8\x18\x0c\xb6>2\xb6\x89\xb9r^\xbd\xd4C\xce\'\xf5\x97s\x10\x04\xb5\xfa\x18\x8e\x82\xd5TH\xd3\xa0\x02c\x1a\x85i\xc8\xed<tD\xcf\\\x07$\xc2I\xe9\x8b\xfa\xc5\x0c\x04\xc1\xfd\xe0\xc4 W\x1d\xa0v]\xaeiLE\x98\x8eE\x98s\xaa\xac\x8dIY=I\x0b\x19\xe9O\xf8\xd8\xcfA\x11\xd8\x97\xfe<\x88]\x18\xa0n\xa3C\x84\x8bU\xd0\x8d:`\xb7 wa\xdew\xedL\xdaY\xd5\r\'b\xd3\x05\xba\x92\x83"\xb8\x93\xdf\xc2A0\x07,N\xaaU\x04k\x9b:*u\x06R\xcf\xde\xdbr;\x97\xba\x97\xb4\xc41\xd2\xd7\xe1\xd7rP\x04\xf5\x0fA*\x0bP\x8d\xadx,\xe35\x13\xa9\x9f;\x8e\xb5qI\xb3*\xde\xc9\xc9\xaes\xbf^\xcf@\x11\xd4\xd3oci}\xe94\xd7a\x19\xd3 \xcc\x1a\xc0\xc6J}\xd9\x99\xd00G\xc2u\x9c\xec\x8ft\xf89\x08\x82\xfa\xdf \xd5A\x8b\x8a\x1c\xfa15\xecV\xa4v\xff\xb1!\x91\xb9\xbe|\xec#\x13\xb1\xbf\xba\xd3\xcb@\x10T\xf6\x17\xec\x97==\x952\x8d"d\xbaF\xaf\xecl\xac\x01?Z\xda\xca\xa4\xad\xb7\xcdc\x9c\x9c\xa9\x9a\xdc\xccA\x11\xdc\xdb\x91\xab.\t\xd8ij\t\x8c-\x8e\xc1\xe8\xfb\xe06\xc4\xfef\xfa\xc1*&i\xce\xbf\x01X\xf7S3VqP\x04v\xef;\x18\xae%5\x9f\xda\xd3C\xf7\xd7\xe8\xe35\x07\xb9\xbb\x8e\x92;\x98\xd4\xdd=^\tg7mc\x1d\x03Epo\xf8+\xb6LG\xf3\x0b\xcc2\x17\xc3\x1d\x1d;\x17\xb1g\xad\x8f\x06`%\x1c\\\xa1\xe1\xa4Qu\x93\x89\x81"\xb0\xff\x90\xb1\x17\xc1\x1d\xd4\xb9^=u\xe5\x87`G_\t\n\xc4NOl[\xc3\xa4>\xfat\x06`M\x07\\\x0c\x14\x81\xbd\xed\xcc`\xb4\x89\xb9\x1ej\x0b\xe8\xe5*\x8c!\xdeX\x04k\xb6\xdf\xc3`\xdd~\xf3}4#\xd5\x1f\xee\n0P\x04x\x1b\x82\xd5N\x9f\xd6G=c\x9a\xb1y\x88\xed\xdb\xdbl\xe0R\xfb\xffX4\'\xfd\xea\x0b\x85\x0c\x14\x81}\t\xb1\xb0\x14\xd2=\xf2f\x1cK\xb8q\xc8\x1d\xdf{q\x03\x97J\x9f\xa3\x99\x9cx\x0f\xcf\xb73P\x04\xf7\xc3 \xd7F\xf3\xf4\xce!n6z}\x95\xc8\xfd\xfe\xc7\xcc\x8dL\xf2\x16?\x9b\xcdHG\xe0\xc4j\x0e\xca\x8e\x88I\x9a\xae?\xa1\xa9\xabr\n\xa83\xdb;\xec\x00\xe7WT\xd6\xd4\x87\x1a\xba\n\x89\xb7\xbfyj\x15\x93J\x1f\x9d\xb8\x04\xb6\xedM34\x0c\x14A4\xa0=\xc6khqz\xbaa\xf8\xdc\x8e@\xc6#\xb20\x7f\xf1J\x18\x02\xd7\x03\x12\'\x97\xa3t6\x06\x8a@rD*\xad4;75\xef\xea\xee\xd2\xc3\xe8\x8e\x00& \xd0\xf7i\xd7Z\xf0\xf0\x82\xdfB5\xbf\xdb\xf5\xedj\x0e\x8a\x00~\x8b@U\xae>\x85j}!\xc3*o\x82\x11\xc4\xf9H\xb4\xf6(Z\xa0\x8e|\xbe\x87\x93\x80\xc6\x7f\'\x03E\x10-\xe8\x88*g\x8e\x87\x06RB\xea\x18\x96\xb8\x00\x89\xff\x9a~K9\x93\x14\xb1\xf7\x19\x19Q~\x9dY\xca@\x11\xc4\'\x83D\xab\xceDm%!\x03\x1a\x96\xa8\x1e \xc6JY\xa7\xdfWp\xd7\xb1\xcd\xef\x9a\xc8\xfe\xaa\xb9K\x19\x08\x02\xf8\x11\x02\x13,\xe94`2\x0f9J\xf2\x0c\x8dH{!\x069;\xf9\xd3\x15P\xc8\x1d3\xa13[\x0e\x7fV\xc2@\x11L?Zv\x82\x9e\xbau\xd9\xfe!\xcc\xb0\xcdIDf\xe5c\xb5k\xe0\x08u\x92(N6\xa7\xd6\xb50P\x04\xf3\x0b\xb4kU\xb6\x81\xfa\x0b\x8c?\xd5\x1c\x8a\xc49\xd1Uwq\xa9\xe4\xb5\xb7\n\x18)(\xbc{3\x07E\x10{\xd1\xa9\xe3\xad\xf9\x94i\xedr\x94\xb8\xf3\xc2&\x9e\x84\xc8\xbfw\\|\x90I1\x1bn\\\xceIL1\xabd\xa0\x08\xe4\xb7o\x0e\xee\xa5\xf8\x14\xa7\xdbH\xf3\x14%W\x91\xf2\xd6\x1fQ\xcb\xe4\x01\xa4B\xea\xde\xbeC\xc5\x97\xaei7\x11\xeb\xf1\xb4f\x0e\xbf\x17\xc0\x13\xb2?\xcb\xfb3A\xab\xd5R\xabvH%\xc3\x125\x03\xc4X\xa9\xfd\xadm\xb3\xb9kg\xe6{&\xd2\xfd\xf5\x145\x07A \xbb\xd1FUi\x1e\x7f\x1e\r\xc0.\xbaf\xd6Z\xcc\x9a\xdd\xf9T3l\xa1\xdd\x05p\xef{a\xd6\xdc\x16\x0e\x8a \xfe\x07\x89\t\xe6T3\r\xc0\x13 d\xbd\x85\rR\x87\xcc\xa4\x13\x19\xab\x994uC\x87\x85\x91=\x87\x1a\xd71P\x04S\x81\xf6\x99\x90\xc2\\\xd4#|\xee\xdaq\xa6 \xf3\xd2\xc3\x87\xdb\xb9\xb4\xc5\x97\xec\xe5$\xf1\x17\x07W0P\x04s\x192U6\x83\xc9B\xcd\xd0\x1d\xcc\\\xf6\xb8\x11\xcdIE\xa2\xfd\xde\'\x8b\xc05\xcf\x18c\x19\xf9\xe6\x07\xb2\x94\x83"\x88u\xe8\xc6\t\xcc\x05\x1bX7$\xf3\xb03\xa4\x1f`*\\\x95\x9b\xbf0\xb9:\x7f\xf5\xb2\x89(\xfa_\xc9`\xf0{A\xfc\x0c\x89\xaa"\xa6\x0fP\xd8\x18\xc1\x18\xc3\xf2\xd20F\xef\xe1GdK\x9b\xa9\x98\xc9\xc9\xa9\xa7\x1b\xb3\x19(\x82x\x06\x9d=\xde\x96\xa7+\xa0\x86P\xef\t\xdb\x9ctD\xce\x1e\xff\xcbZ.5\x12\x9b\t\xee\xb8\xb7\xbe\xe8\xe7\xa0\x08\xa4\x1em\'^O\xd3\xf2\x8a\x87\\B\xc2"\r\x88\x9c?\xedN?Tr\xfb2\xa8\xe4\xe9\xd6\xadk9(\x02\xb9>\x88t\x07\x8ai@<\xf8\xb09a\x137\x0e c\xa5C\'+\x8a\xb8\xab\xbd\xf4\xf1\x1eR{,\xbf\x9e\x81 \x88O!Qe\xd1e\x07\xe8\xf0\x0b\xd8\x88nK\x18\xe3Y\xfb\x9b?\x87\xdd\x1b\xa5\xd3s\xa2\xac\xad\xaed\xa0\x08\xe2\xd1 \xb1\xd0k\xca\xa7\xb6\x90m\x1e6\xeb\x0c\x0cQ\xf1L.\xdch\xbfL\xee\xef!\xc7\x943K8\x08;"\xae\xd3t\xedG\x9bU\x06r\x02T\x0bGq\xc8\x9b\xd1\\\xdb0\xc4e\x17a\x84[/\xbc\x9e#W\xb1\x17\xaa\xe8\xad<\xc28(\x02hD\x07SZl\xd4\xe0s\xfe\x040\x13\x81\xefj\xf67\x02\xb02%\x89\x93\x92\xaa\xbd>\x0e\x8a\x00\x1eG`|\x1e\xb5\xe5\xd9\xddC\x06\\Nzx\x8c\x8b\x11\xb9\xe7\xd8\xcb\xe0\xb2%\x13\x9f\x80S\xb8\xc7\xd3{7\x03E \xfb\x11\x19g4\xeai\x8am\xa8m\x0f\xe7-A^\xff\x85\xb3\x01x\xd1\xaf\xbdO\xcd\xc9\xf3\xad\x17s\x18(\x827\x1f\rL\x05\x17\xb6\x1c\x87.t\xbe\xe5c=\x9c\x98\x85\xc4-I\xe7a\x16\xdb\xd7\xc6\xde\xccH\xc4\xa5\x7fn`\xa0\x08\xe2&$*\xcd\xd4\x99\x96[\x18REy\x14\x87\x03M\x08\xbc\xf4\xe4\x1d\xf0v\xab~\x95\xf99\xc9\xbab\x8c\xe5\xa0\x08\xe09\x04\xaa\xec\xb4\xd8X\xe8\x0f\xb5\x85p!\xb2\x01b\xdc\x82\xf6\xe4}\xdf=\xca\xa4\xc8\xcf\x8f\x971rHyv\x19\x03I \xadh\xb1J\x9f\x8e\xfa\xccr\xd6x\\d\x83\x1d\x1e\xa3y\x80\x18+m]\x94a\xe5\xae\xfd\xd6\xb7M\xc4{o\x92\x93\x81 x?\xa2\xbd*u\x1e\x9a\x9d]\x1c\x92s\xb86[\x90\xe7\x8e\x99\xd7\xc4]\x13\xd8W=\xe4\xd2\x94S\x9b8\x08\x82\xf7\x04\x9a\xab2\xcd`\x85g;\x0c\x0e\xc6\x17\xae\x86\xd9X\xc3\xc5\xe7-\xab\xb8\xd4}n\xf2|F<\x07\x137rP\x04\xf0,Z\xab\xd2guQ8(\xd7\x06Z1\xc0\xce\xc8\xe4L\xee:\xf1\xde\r\x9ct\xce\xfc]:\x07A\xf0\xf6\xe1\xcb7\xceltS\xb3Y\x11\x8c/\\Gl\x18\xdf\xd4wf\xe7\xc2\xbb\x7f\x91\x07\xbeXj7\xbf=\x95\x83"x\xbb\xd1\x04U\x054\xdb\xe7\xc8\x0b\x1d\xc3p\x19\xdb\x91\xa86L[\x0f\xaf\xe8\x17?t0r\xca\xf1\x89\x93\x81"\x88\x9f 1.\x87\x9aS\x06,\x10k(\xf7\xd8T[[V\xad0\x95\xaf\xac^^\xd6\x14\xe9@^\xff\x031K\xe1\x06u\xb2d\x0e\'\xf6\x17\xcex9(\x82w=\xda_\xbc\xd1lK\xa3\xe2\xbe\x13LZ\xbe<\x0e\x1f\x1a\'"7m{\xb8\x86I\x97\x1fo\x03\x97\x0e8\xcb\xd62P\x04\xf2g\x88T\x1aij\xbe\x17\xa60\xb8\xc0\xc2U\xd1\x85\xc0-\xe7\xc1M\xe0\xfb\x88\xf7\x95\xf0\xb0SW\xe50P\x06\xaa\x88\xf6\xa7tQCA^AH\x97\xc3\x01s\x06\x80\xb1Ri\xd4\\=w\xcd\xca\xfa\xc1D.\x17=\xe4\xe2 \x08\xde\xc7\xc8\x8b\xf3\xb9mN\n\x0f$\xac`\xb8\x9e\xe4b|t\x9aj\x03<\xbf\xf7\x9c6s\xd2\x1b\xd9\xba\x8e\x83"x\xbdh\xa6J\xbb\x81\x06\ne\x9bB`\xb8c\xe7F\xe0\x8c\xd5\xbd\x85\xb0`\xef\xafJfD\xf3|W\x16\x03E\x00\xfb\x83@\xb7\'\x87\xe6\x86^k\xc3%\xec\xc1\x84\xbf\xb9\xd8\x99\xcf\\\'\xef\xfa\xccD\xfa\x8eLHa \x08^\x0bz\x94\xd2\x12H\xa3#\xbe$\x19\xdeb/\x06\xb8\xee\xba\xde&\xc88+\xc3\xce\x89v\xe1\xe3\xf70P\x04\xf0Y\x04\xaa\xd2\nS\x1c4\x072\xbef\xcayH\xbcR\xf3y)\xbc\xdb\x1a\xbe\xd61\xb2\xb3\xafk\x19\x03E<Z\xaf\xa0\xeb\xcd\xf36\xd4\x965\xd54\x0f\x161\xecW\xc5\xf9\x88K5\xf4V\xc3\xcbr\xfdK\x19p\x07\xbb9)\x8f\x83"\\9\x88\x9bs\x15\'G8\xc2\xe0\x0b\x10\xa5\xfc\x82V\x00jnW:#/\x1f\x8f^\xc9@\x11\xb9\x86E\xc9\xacae\xa3\xf2\xb7\xf8\xff\x07\xdaK3S'

    def __eq__(self, other):
        if not isinstance(self, type(other)):
            return False

        if self.__dict__ == other.__dict__:
            return True
        else:
            # Equivalency function must be agnostic towards byte -> string comparisons for python 2.7 backwards compatibility
            if self.__dict__.keys() != other.__dict__.keys():
                return False

            # Iterate over __dict__ while decoding any byte -> string comparisons
            for key in self.__dict__:
                if self.__dict__[key] != other.__dict__[key]:
                    if isinstance(self.__dict__[key], bytes) and isinstance(other.__dict__[key], str):
                        if self.__dict__[key].decode() != other.__dict__[key]:
                            return False
                    elif isinstance(self.__dict__[key], str) and isinstance(other.__dict__[key], bytes):
                        if self.__dict__[key] != other.__dict__[key].decode():
                            return False
                    else:
                        return False

        return True

class NewStyleObject(object):
    pass

class NewStyleWithSetState(object):
    def __init__(self, string, number):
        self.string = string
        self.number = number

    def __getstate__(self):
        return self.string, self.number

    def __setstate__(self, state):
        self.string = state[0]
        self.number = state[1]

    def __eq__(self, other):
        return (self.string == other.string and
                self.number == other.number)

class OldStyleObject:
    pass

class ObjectWithData(object):
    def __init__(self, data):
        self.data = data

class CustomException(Exception):
    def __init__(self, message=None, data=None):
        self.message = message
        self.data = data

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

    def test_dict(self):
        self.verify_round_trip({"key": "test"})

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

    def test_write_callback_called(self):
        def callback(obj):
            callback.called = True
        callback.called = False
        obj = SimpleObject()
        blue.marshal.Save(obj, callback=callback)
        self.assertTrue(callback.called)

    def test_read_callback_called(self):
        def write_callback(obj):
            return "whatever"

        def read_callback(obj):
            read_callback.called = True
        read_callback.called = False

        obj = SimpleObject()
        s = blue.marshal.Save(obj, callback=write_callback)
        blue.marshal.Load(s, callback=read_callback)
        self.assertTrue(read_callback.called)

    def test_read_and_write_callbacks_used(self):
        def write_callback(obj):
            return 2

        def read_callback(obj):
            ret = SimpleObject()
            ret.b = obj * 3
            return ret

        obj = SimpleObject()
        savedObj = blue.marshal.Save(obj, callback=write_callback)
        loadedObj = blue.marshal.Load(savedObj, callback=read_callback)
        self.assertEqual(6, loadedObj.b)

    def test_write_uses_default_pickle_method_when_callback_raises_error(self):
        def write_callback(obj):
            raise RuntimeError("Write callback failed spectacularly!")

        obj = SimpleObject()
        s = blue.marshal.Save(obj, callback=write_callback)
        loaded_obj = blue.marshal.Load(s)
        self.assertBlueObjectsEqual(obj, loaded_obj)

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

    def test_str_method(self):
        obj = [SimpleObject(), "this is a test"]
        ws = blue.marshal.Save(obj)
        self.assertIsInstance(str(ws), str)

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
            b"\x7D\x01\x62\x1B\x00",  # entering via `TY_REDUCE`
            b"\x7D\x01\x63\x1B\x00",  # entering via `TY_NEWOBJ`
        )
        for bad_payload in known_bad_payloads:
            with self.assertRaises(RuntimeError):
                blue.marshal.Load(bad_payload)

    def test_deeply_nested_read(self):
        depth = 10000
        payload = b"\x7D\x01" + b"\x25" * depth + b"\x01"
        with self.assertRaises(RuntimeError):
            blue.marshal.Load(payload)


@unittest.skipUnless("PY27_COMPATIBILITY_MODE" in os.environ, "Skipping because tests will fail when compatibility mode is off")
class TestBackwardsCompatibility(blueunittest.TestCase):
    """
    This class adds coverage for objects marshalled in Python 2.7.
    """
    def test_load_legacy_unicode_string(self):
        bytes = b'~\x00\x00\x00\x00.\x0fUnicode string.'
        unicode_string = blue.marshal.Load(bytes)
        self.assertEqual(unicode_string, "Unicode string.")

    def test_load_legacy_byte_string(self):
        bytes = b'~\x00\x00\x00\x00\x13\x0cByte string.'
        byte_string = blue.marshal.Load(bytes)
        self.assertEqual(byte_string, b"Byte string.")

    def test_load_old_style_object(self):
        bytes = b'~\x00\x00\x00\x00\x17\x13*bluetests.test.test_marshal.OldStyleObject\x16\x00'
        obj = blue.marshal.Load(bytes)
        self.assertIsInstance(obj, OldStyleObject)

    def test_load_new_style_object(self):
        bytes = b'~\x00\x00\x00\x00#,%\x02*bluetests.test.test_marshal.NewStyleObject\x16\x00--'
        obj = blue.marshal.Load(bytes)
        self.assertIsInstance(obj, NewStyleObject)

    def test_load_object_with_data(self):
        bytes = b'~\x00\x00\x00\x00#,%\x02*bluetests.test.test_marshal.ObjectWithData\x16\x01\x13\x04test\x13\x04data--'
        obj = blue.marshal.Load(bytes)
        self.assertEqual(obj.data, b"test")

    def test_old_style_object_with_data(self):
        bytes = b'~\x00\x00\x00\x00\x17\x13*bluetests.test.test_marshal.ObjectWithData\x16\x01\x13\x04test\x13\x04data'
        obj = blue.marshal.Load(bytes)
        self.assertEqual(obj.data, b"test")


    def test_dbrow(self):
        bytes = b'~\x00\x00\x00\x00*",\x02\x14blue.DBRowDescriptor%%,\x13\x04Test\x06\x14--\x02\xf7{'
        dbrow = blue.marshal.Load(bytes)
        self.assertBlueObjectsEqual(dbrow, blue.DBRow(blue.DBRowDescriptor((("Test", 20),)), (123, )))

    def test_empty_dbrow(self):
        bytes = b'~\x00\x00\x00\x00*",\x02\x14blue.DBRowDescriptor%$--\x00'
        dbrow = blue.marshal.Load(bytes)
        self.assertBlueObjectsEqual(dbrow, blue.DBRow(blue.DBRowDescriptor(())))

    def test_none(self):
        bytes = b'~\x00\x00\x00\x00\x01'
        self.assertIsNone(blue.marshal.Load(bytes))

    def test_empty_string(self):
        bytes = b'~\x00\x00\x00\x00\x0e'
        string = blue.marshal.Load(bytes)
        self.assertIsInstance(string, str)
        self.assertEqual(string, "")

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
        bytes = b'~\x00\x00\x00\x00\x16\x01\x13\x04test\x13\x03key'
        self.assertEqual(blue.marshal.Load(bytes), {b"key": b"test"})

    def test_empty_unicode(self):
        bytes = b'~\x00\x00\x00\x00.\x00'
        string = blue.marshal.Load(bytes)
        self.assertIsInstance(string, str)
        self.assertEqual(string, u"")

    def test_empty_object(self):
        self.assertBlueObjectsEqual(blue.marshal.Load(b"~\x00\x00\x00\x00\x17\x13'bluetests.test.test_marshal.EmptyObject\x16\x00"), EmptyObject())

    def test_empty_list(self):
        bytes = b'~\x00\x00\x00\x00&'
        self.assertEqual(blue.marshal.Load(bytes), [])

    def test_bytes_string(self):
        bytes = b'~\x00\x00\x00\x00\x13\x0ethis is a test'
        self.assertEqual(blue.marshal.Load(bytes), b'this is a test')

    def test_string_from_stringtable(self):
        bytes = b'~\x00\x00\x00\x00\x11\x06'
        self.assertEqual(blue.marshal.Load(bytes), "ballID")

    def test_single_char(self):
        bytes = b'~\x00\x00\x00\x00\x13\x01A'
        self.assertEqual(blue.marshal.Load(bytes), b"A")

    def test_single_char_unicode(self):
        bytes = b'~\x00\x00\x00\x00.\x01A'
        self.assertEqual(blue.marshal.Load(bytes), u"A")

    def test_unicode(self):
        bytes = b'~\x00\x00\x00\x00.\t\xe2\x82\xa8\xe2\x82\xb1\xe2\x82\xa9'
        self.assertEqual(blue.marshal.Load(bytes), u"\u20A8\u20B1\u20A9")

    def test_unicode_as_utf8(self):
        bytes = b'~\x00\x00\x00\x00.\x16this is a unicode test'
        self.assertEqual(blue.marshal.Load(bytes), u"this is a unicode test")

    def test_simple_object(self):
        bytes = b'~\x00\x00\x00\x00\x17\x13(bluetests.test.test_marshal.SimpleObject\x16\x04\x13\x10this is a string\x0fa\n\xcd\x06xV\xfb!\t@\x0fc\x06*\x0fb\x13\x07x\x01\x8d\x98{t\xd3\x0fd'
        self.assertBlueObjectsEqual(blue.marshal.Load(bytes), SimpleObject())

    def test_list_of_one_string(self):
        bytes = b"~\x00\x00\x00\x00'\x13\x0ethis is a test"
        self.assertEqual(blue.marshal.Load(bytes), [b"this is a test"])

    def test_list_of_strings(self):
        bytes = b'~\x00\x00\x00\x00\x15\x04\x13\x04this\x13\x02is\x13\x01a\x13\x04test'
        self.assertEqual(blue.marshal.Load(bytes), [b"this", b"is", b"a", b"test"])

    def test_empty_tuple(self):
        bytes = b'~\x00\x00\x00\x00$'
        self.assertEqual(blue.marshal.Load(bytes), ())

    def test_tuple_of_one_string(self):
        bytes = b'~\x00\x00\x00\x00%\x13\x0ethis is a test'
        self.assertEqual(blue.marshal.Load(bytes), (b"this is a test",))

    def test_tuple_of_two_strings(self):
        bytes = b'~\x00\x00\x00\x00,\x13\x07this is\x13\x06a test'
        self.assertEqual(blue.marshal.Load(bytes), (b"this is", b"a test"))

    def test_tuple_of_strings(self):
        bytes = b'~\x00\x00\x00\x00\x14\x04\x13\x04this\x13\x02is\x13\x01a\x13\x04test'
        self.assertEqual(blue.marshal.Load(bytes), (b"this", b"is", b"a", b"test"))

    def test_instanced_object(self):
        obj = SimpleObject()
        bytes = b'~\x01\x00\x00\x00\x15\x03W\x13(bluetests.test.test_marshal.SimpleObject\x16\x04\x13\x10this is a string\x0fa\n\xcd\x06xV\xfb!\t@\x0fc\x06*\x0fb\x13\x07x\x01\x8d\x98{t\xd3\x0fd\x1b\x01\x1b\x01\x01\x00\x00\x00'
        self.assertEqual(blue.marshal.Load(bytes), [obj, obj, obj])

    def test_read_callback_called(self):
        def read_callback(obj):
            read_callback.called = True
        read_callback.called = False

        bytes =  b'~\x00\x00\x00\x00\x19\x13\x04test'
        blue.marshal.Load(bytes, callback=read_callback)
        self.assertTrue(read_callback.called)

    def test_checksum(self):
        # Marshalled Python 2.7 object using checksum
        bytes = b'~\x00\x00\x00\x00\x1c6$\xfa\xd9\x15\x02\x17\x13(bluetests.test.test_marshal.SimpleObject\x16\x04\x13\x10this is a string\x0fa\n\xcd\x06xV\xfb!\t@\x0fc\x06*\x0fb\x13\x07x\x01\x8d\x98{t\xd3\x0fd\x13\x0ethis is a test'
        comparison = blue.marshal.Save([SimpleObject(), b"this is a test"], useChecksum=1)
        # Marshalled data will differ due to string fields, so we must load both objects for comparison
        self.assertBlueObjectsEqual(blue.marshal.Load(bytes), blue.marshal.Load(comparison))

    def test_custom_exception(self):
        bytes = b'~\x00\x00\x00\x00"\x14\x03\x02+bluetests.test.test_marshal.CustomException$\x16\x02.\x1aTotally expected exception\x13\x07message\x06*\x13\x04data--'
        blue.marshal.Load(bytes)

    def test_new_style_with_setstate(self):
        bytes = b'~\x00\x00\x00\x00#,%\x020bluetests.test.test_marshal.NewStyleWithSetState,\x13\x05Pizza\x06C--'
        loaded = blue.marshal.Load(bytes)
        self.assertBlueObjectsEqual(loaded, NewStyleWithSetState(b"Pizza", 67))

    def test_set(self):
        blue.marshal.globalsWhitelist = {set: 0}
        blue.marshal.collectWhitelist = False
        bytes = b'~\x00\x00\x00\x00",\x02\x0f__builtin__.set%\x15\x03\t\x06\x02\x06\x03--'
        loaded = blue.marshal.Load(bytes)
        self.assertEqual(loaded, {1, 2, 3})

    def test_runtime_error(self):
        blue.marshal.globalsWhitelist = {RuntimeError: 0}
        blue.marshal.collectWhitelist = False
        bytes = b'~\x00\x00\x00\x00",\x02\x17exceptions.RuntimeError%\x13\x05Boom!--'
        loaded = blue.marshal.Load(bytes)
        self.assertIsInstance(loaded, RuntimeError)
        self.assertEqual(loaded.args, (b"Boom!",))

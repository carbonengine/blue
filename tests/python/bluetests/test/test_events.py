# Copyright © 2023 CCP ehf.

import unittest
import blue


class Metaclass(type):
    def __new__(mcs, name, bases, dict):
        cls = type.__new__(mcs, name, bases, dict)
        cls.__persistvars__ = []
        cls.__nonpersistvars__ = []
        return cls

    def __call__(cls):
        # noinspection PyUnresolvedReferences
        inst = blue.classes.CreateInstance(cls.__cid__)
        inst.__klass__ = cls
        inst.__init__()
        return inst


class SendEventReceiver(object, metaclass=Metaclass):
    __cid__ = "blue.BlueTestEvents"

    def __init__(self):
        self.event_received = False
        self.received_parameters = None

    def DoSendEvent(self, *args):
        self.received_parameters = args


class TestSendEvent(unittest.TestCase):
    def setUp(self) -> None:
        self._event_receiver = SendEventReceiver()

    def test_event_gets_sent(self):
        event_parameters = 4, 3.141592
        success = self._event_receiver.SendEvent(*event_parameters)
        self.assertTrue(success)
        # SendEvent should process the event right away,
        # so by this point we should already have received a callback.
        self.assertEqual(event_parameters, self._event_receiver.received_parameters)


class PostEventReceiver(object, metaclass=Metaclass):
    __cid__ = "blue.BlueTestEvents"

    def __init__(self):
        self.event_received = False
        self.received_parameters = None

    def OnPostEvent(self, *args):
        self.event_received = True
        self.received_parameters = args


class TestPostEvent(unittest.TestCase):
    def setUp(self) -> None:
        self._event_receiver = PostEventReceiver()

    def test_event_gets_sent(self):
        event_parameters = 4, 3.141592
        success = self._event_receiver.PostEvent(*event_parameters)
        self.assertTrue(success)
        while not self._event_receiver.event_received:
            # PostEvent should process the event asynchronously,
            # so we may need to wait for the callback.
            blue.synchro.Yield()
        self.assertEqual(event_parameters, self._event_receiver.received_parameters)


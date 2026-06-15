# Copyright © 2023 CCP ehf.

import unittest
from bluepycore import TaskletExt, tasklets
import blue


CONTEXT = "test::context"


def noop():
    pass


class TestCallable:
    def __init__(self):
        self.call_counter = 0
        self.args = None
        self.kwargs = None

    def __call__(self, *args, **kwargs):
        self.call_counter += 1
        self.args = args
        self.kwargs = kwargs


class RaisingCallable:
    def __init__(self, exception):
        self._exc = exception

    def __call__(self, *args, **kwargs):
        raise self._exc


class TestTaskletExt(unittest.TestCase):
    def test_context_is_set(self):
        t = TaskletExt(CONTEXT)
        self.assertEqual(CONTEXT, t.context)

    def test_created_tasklet_not_scheduled(self):
        t = TaskletExt(CONTEXT, noop)
        self.assertFalse(t.scheduled)

    def test_repr(self):
        t = TaskletExt(CONTEXT, noop)
        r = t.__repr__()
        self.assertIsInstance(r, str)
        self.assertTrue(r.startswith("<TaskletExt object at 0x"))

    def test_timers(self):
        CONTEXT_NONE = "<NO CONTEXT>"
        CONTEXT_LEVEL_ONE = "context::level_one"
        CONTEXT_LEVEL_TWO = "context::level_two"
        blue.pyos.taskletTimer.active = True
        blue.pyos.taskletTimer.telemetryOn = True
        try:
            t = TaskletExt(CONTEXT, noop)
            level_zero = t.GetCurrent()
            self.assertEqual(CONTEXT_NONE, level_zero)
            t.PushTimer(CONTEXT_LEVEL_ONE)
            self.assertEqual(CONTEXT_LEVEL_ONE, t.GetCurrent())
            t.PushTimer(CONTEXT_LEVEL_TWO)
            self.assertEqual(CONTEXT_LEVEL_TWO, t.GetCurrent())
            t.PopTimer(CONTEXT_LEVEL_TWO)
            self.assertEqual(CONTEXT_LEVEL_ONE, t.GetCurrent())
            t.PopTimer(CONTEXT_LEVEL_ONE)
            self.assertEqual(CONTEXT_NONE, t.GetCurrent())
        finally:
            blue.pyos.taskletTimer.telemetryOn = False
            blue.pyos.taskletTimer.active = False

    def test_get_wallclock_time_before_starting_tasklet(self):
        t = TaskletExt(CONTEXT, noop)
        time = t.GetWallclockTime()
        self.assertIsNone(time)

    def test_get_wallclock_time_after_starting_tasklet(self):
        t = TaskletExt(CONTEXT, noop)
        t()
        while t.startTime == 0.0:
            blue.synchro.Yield()
        time = t.GetWallclockTime()
        self.assertIsInstance(time, float)
        self.assertGreaterEqual(time, 0)

    def test_get_runtime_before_starting_tasklet(self):
        t = TaskletExt(CONTEXT, noop)
        time = t.GetRunTime()
        self.assertIsInstance(time, float)
        self.assertEqual(time, 0.0)

    def test_get_runtime_after_starting_tasklet(self):
        t = TaskletExt(CONTEXT, noop)
        t()
        while not hasattr(t, "startTime"):
            blue.synchro.Yield()
        time = t.GetRunTime()
        self.assertIsInstance(time, float)
        self.assertGreaterEqual(time, 0.0)

    def test_id_counter_increments(self):
        first = TaskletExt(CONTEXT, noop)
        second = TaskletExt(CONTEXT, noop)
        self.assertEqual(second.tasklet_id, first.tasklet_id + 1)

    def test_tasklet_added_to_weakref_dict(self):
        t = TaskletExt(CONTEXT, noop)
        self.assertIn(t, tasklets)


class TestTaskletExtWithCallable(unittest.TestCase):
    def test_called(self):
        callable = TestCallable()
        t = TaskletExt(CONTEXT, callable)
        t()
        while t.scheduled:
            blue.synchro.Yield()
        self.assertEqual(1, callable.call_counter)

    def test_args_get_passed(self):
        callable = TestCallable()
        args = (1, 2, 3)
        kwargs = {"a": 1, "b": 2, "c": 3}
        t = TaskletExt(CONTEXT, callable)
        t(*args, **kwargs)
        while t.scheduled:
            blue.synchro.Yield()
        self.assertTupleEqual(args, callable.args)
        self.assertDictEqual(kwargs, callable.kwargs)

def test_set_exception_notification_handler(self):
    callable = RaisingCallable(RuntimeError("Something bad happened"))

    def handler(_):
        handler.called = True
    handler.called = False
    TaskletExt.SetExceptionHandler(handler)

    t = TaskletExt(CONTEXT, callable)
    t()
    while t.scheduled:
        blue.synchro.Yield()
    self.assertTrue(handler.called)

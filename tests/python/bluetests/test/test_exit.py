# Copyright © 2026 CCP ehf.

import gc
import sys
import unittest


class Payload:
    pass


class TestExitPatching(unittest.TestCase):
    """
    Importing blue replaces sys.exit and sys.excepthook with versions that record the process exit code.
    These tests check that the replacements behave like well-mannered C API citizens.

    Note that calling the patched functions updates the exit code blue will terminate the process with, but
    unittest.main() calls sys.exit() with the real result once all tests have run.
    """
    def setUp(self):
        self.assertIn("Part of Blue exit patching", sys.exit.__doc__)
        self.assertIn("Part of Blue exit patching", sys.excepthook.__doc__)

    def _assert_exit_keeps_refcount(self, value, calls=1):
        # Extra references so that over-releasing the value fails the assertion instead of freeing it
        keepalive = [value] * (calls + 1)
        before = sys.getrefcount(value)
        for _ in range(calls):
            with self.assertRaises(SystemExit) as cm:
                sys.exit(value)
            self.assertIs(cm.exception.code, value)
            del cm
        self.assertEqual(sys.getrefcount(value), before)
        del keepalive

    def test_exit_with_string_does_not_release_argument(self):
        self._assert_exit_keeps_refcount("".join(["x"] * 50))

    def test_exit_with_int_does_not_release_argument(self):
        # Ints outside [-5, 256] are not immortal
        self._assert_exit_keeps_refcount(int("1000"))

    def test_exit_with_int_out_of_c_long_range_does_not_release_argument(self):
        self._assert_exit_keeps_refcount(10 ** int("20"))

    def test_exit_with_object_does_not_release_argument(self):
        self._assert_exit_keeps_refcount(Payload())

    def test_repeated_exit_with_same_object_does_not_release_argument(self):
        self._assert_exit_keeps_refcount(Payload(), calls=20)

    def test_original_exit_is_kept_alive_by_blue(self):
        # Holding a reference to the original sys.exit from Python would hide the problem, so look for references
        # that aren't accounted for by any Python container: those are held from C, i.e. by blue.
        candidates = [
            o for o in gc.get_objects()
            if type(o) is type(len) and o.__name__ == "exit" and o.__self__ is sys and o is not sys.exit
        ]
        self.assertEqual(len(candidates), 1)
        original = candidates[0]
        del candidates
        referrers = gc.get_referrers(original)
        # Subtract the references held by `original` and by the getrefcount argument
        untracked = sys.getrefcount(original) - len(referrers) - 2
        del referrers
        self.assertGreaterEqual(untracked, 1)

    def test_excepthook_without_original_excepthook_raises(self):
        saved = sys.__excepthook__
        del sys.__excepthook__
        try:
            with self.assertRaises(RuntimeError):
                sys.excepthook(ValueError, ValueError("boom"), None)
        finally:
            sys.__excepthook__ = saved

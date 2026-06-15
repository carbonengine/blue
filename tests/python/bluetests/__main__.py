# Copyright © 2023 CCP ehf.

import unittest
import blue
import scheduler


def main():
    import sys
    if sys.argv[0].endswith("__main__.py"):
        import os.path
        # We change sys.argv[0] to make help message more useful
        # use executable without path, unquoted
        # (it's just a hint anyway)
        # (if you have spaces in your executable you get what you deserve!)
        executable = os.path.basename(sys.executable)
        sys.argv[0] = executable + " -m unittest"
        del os

    class TaskletTestRunner(unittest.TextTestRunner):
        def __init__(self, *args, **kwargs):
            self.result = None
            super().__init__(*args, **kwargs)

        def run(self, test):
            scheduler.tasklet(self._run_impl)(test)
            # Required to avoid sleep when all tasklets yield
            blue.os.sleeptime = 0
            while self.result is None:
                blue.os.Pump()

            return self.result

        def _run_impl(self, test):
            self.result = super().run(test)

    unittest.main(module=None, testRunner=TaskletTestRunner())


main()

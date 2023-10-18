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

    # initialize platform dependent socket functionality.
    import _slsocket
    sys.modules["_socket"] = _slsocket
    import platform
    if platform.system() == "Windows":
        import carbonio
        carbonio._socket = _slsocket
        _slsocket.use_carbonio(True)
    elif platform.system() == "Darwin":
        import stacklessio
        stacklessio._socket = _slsocket
        _slsocket.use_carbonio(False)
        import slselect
        sys.modules["select"] = slselect
    else:
        raise RuntimeError(f"{platform.system()} is not a supported platform")

    # Cannot import unittest before we patch the socket, because unittest imports socket.
    import unittest
    __unittest = True
    unittest.main(module=None)


main()

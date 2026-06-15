# Copyright © 2023 CCP ehf.

__author__ = 'snorri.sturluson'

from . import blueunittest
import blue
import tempfile
import shutil
import os

class testFileStream(blueunittest.TestCase):
    def setUp(self):
        self.tmpDir = tempfile.mkdtemp()

    def tearDown(self):
        def retry_handler(func, path, _):
            import time
            t0 = time.time()
            while True:
                try:
                    func(path)
                    break
                except:
                    if time.time() - t0 >= 5.0:
                        raise
        shutil.rmtree(self.tmpDir, onerror=retry_handler)

    def test_create(self):
        s = blue.BlueFileStream()
        s.close()

    def test_open_read_entire_file(self):
        expectedContents = "this is a test"

        with tempfile.NamedTemporaryFile(mode="w", delete=False) as fp:
            fp.write(expectedContents)
            fp.close()

            file_stream = blue.BlueFileStream()
            try:
                contents = file_stream.ReadEntireFile(fp.name)
                self.assertEqual(contents, expectedContents)
            finally:
                file_stream.close()

    def test_open_read_entire_file_with_0_terminator(self):
        file_path = os.path.join(self.tmpDir, "test_open_read_entire_file_with_0_terminator.txt")
        expectedContents = "this is a test\0with a zero terminator"
        self._create_file(file_path, expectedContents)

        file_stream = blue.BlueFileStream()
        try:
            contents = file_stream.ReadEntireFile(file_path)
            self.assertEqual(contents, expectedContents)
        finally:
            file_stream.close()

    def _create_file(self, name, contents):
        with open(name, "w") as createdFile:
            createdFile.write(contents)

    def _create_file_with_blue_paths_open(self, name, contents):
        createdFile = blue.paths.open(name, "w")
        createdFile.write(contents.encode())
        createdFile.close()

    def test_open_for_reading(self):
        file_path = os.path.join(self.tmpDir, "test_open_for_reading.txt")
        expectedContents = "this is a test\0with a zero terminator"
        self._create_file(file_path, expectedContents)

        file_stream = blue.paths.open(file_path, "r")
        try:
            contents = file_stream.read()
            self.assertEqual(contents, expectedContents.encode())
        finally:
            file_stream.close()

    def test_open_for_writing_when_file_does_not_exist(self):
        file_path = os.path.join(self.tmpDir, "test_open_for_writing_when_file_does_not_exist.txt")
        expectedContents = "this is a test\0with a zero terminator"
        self._create_file_with_blue_paths_open(file_path, expectedContents)

        with open(file_path, "r") as file_stream:
            contents = file_stream.read()
            self.assertEqual(contents, expectedContents)

    def test_open_for_writing_when_file_exists(self):
        file_path = os.path.join(self.tmpDir, "test_open_for_writing_when_file_exists.txt")
        self._create_file(file_path, "This is not the expected content")

        expectedContents = "this is a test\0with a zero terminator"
        self._create_file_with_blue_paths_open(file_path, expectedContents)

        with open(file_path, "r") as file_stream:
            contents = file_stream.read()
            self.assertEqual(contents, expectedContents)

    def test_open_for_append(self):
        file_path = os.path.join(self.tmpDir, "test_open_for_append.txt")
        expectedContents = "This is just the beginning"
        self._create_file(file_path, expectedContents)

        file_stream = blue.paths.open(file_path, "a")
        try:
            addition = "And here is the end"
            file_stream.write(addition.encode())
        finally:
            file_stream.close()

        expectedContents += addition
        with open(file_path, "r") as file_stream:
            contents = file_stream.read()
            self.assertEqual(contents, expectedContents)

    def test_seek_from_beginning(self):
        file_path = os.path.join(self.tmpDir, "test_seek_from_beginning.txt")
        expectedContents = "01234567890123456789012345678901234567890123456789"
        self._create_file(file_path, expectedContents)

        file_stream = blue.paths.open(file_path, "r")
        try:
            file_stream.seek(14)
            character_read = file_stream.read(1)
            self.assertEqual(character_read, b"4")
        finally:
            file_stream.close()

    def test_seek_from_end(self):
        file_path = os.path.join(self.tmpDir, "test_seek_from_end.txt")
        expectedContents = "01234567890123456789012345678901234567890123456789"
        self._create_file(file_path, expectedContents)

        file_stream = blue.paths.open(file_path, "r")
        try:
            file_stream.seek(-4, os.SEEK_END)
            character_read = file_stream.read(1)
            self.assertEqual(character_read, b"6")
        finally:
            file_stream.close()

    def test_seek_from_current(self):
        file_path = os.path.join(self.tmpDir, "test_seek_from_current.txt")
        expectedContents = "01234567890123456789012345678901234567890123456789"
        self._create_file(file_path, expectedContents)

        file_stream = blue.paths.open(file_path)
        try:
            file_stream.seek(14)
            file_stream.seek(-4, os.SEEK_CUR)
            character_read = file_stream.read(1)
            self.assertEqual(character_read, b"0")
        finally:
            file_stream.close()

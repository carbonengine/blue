# Copyright © 2023 CCP ehf.

from . import blueunittest
import blue
import os
import tempfile
import shutil

class TestResFile(blueunittest.TestCase):
    """
    A set of test cases for blue.ResFile.
    """

    def setUp(self):
        self.tmpDir = tempfile.mkdtemp()
        self.cachePath = blue.paths.GetSearchPath("cache")
        self.resPath = blue.paths.GetSearchPath("res")
        blue.paths.SetSearchPath("cache", self.tmpDir)
        blue.paths.SetSearchPath("res", self.tmpDir)

    
    def tearDown(self):
        blue.paths.SetSearchPath("cache", self.cachePath)
        blue.paths.SetSearchPath("res", self.resPath)

        def retry_handler(func, path, _):
            import time
            last_exc = None
            t0 = time.time()
            while time.time() - t0 < 2.5:
                try:
                    func(path)
                except Exception as exc:
                    last_exc = exc
                else:
                    last_exc = None
                    break
            if last_exc:
                raise last_exc
        shutil.rmtree(self.tmpDir, onerror=retry_handler)

        blue.os.languageID = "EN"

        blue.os.Pump()


    def _createFile(self, name, contents):
        with open(name, "wb") as createdFile:
            createdFile.write(contents)


    def test_OpenCapitalOExistingFile(self):
        filePath = os.path.join(self.tmpDir, "test_OpenCapitalOExistingFile.txt")
        self._createFile(filePath, b"this is a test")

        resFile = blue.ResFile()
        openResult = resFile.Open("cache:/test_OpenCapitalOExistingFile.txt")

        self.assertTrue(openResult)

        resFile.Close()


    def test_OpenExistingFile(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        self._createFile(filePath, b"this is a test")

        resFile = blue.ResFile()
        openResult = resFile.open("cache:/test.txt")

        self.assertBlueObjectsEqual(resFile, openResult)

        resFile.Close()


    def test_OpenCapitalOExistingFileForWriting(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        self._createFile(filePath, b"this is a test")

        resFile = blue.ResFile()
        openResult = resFile.Open("cache:/test.txt", 0)

        self.assertTrue(openResult)

        resFile.Close()


    def test_OpenExistingFileForWriting(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        self._createFile(filePath, b"this is a test")

        resFile = blue.ResFile()
        openResult = resFile.open("cache:/test.txt", 0)

        self.assertBlueObjectsEqual(resFile, openResult)

        resFile.Close()


    def test_OpenCapitalOExistingFileForWritingAndWriteToIt(self):
        filePath = os.path.join(self.tmpDir, "test_OpenCapitalOExistingFileForWritingAndWriteToIt.txt")
        self._createFile(filePath, b"this is a test")

        resFile = blue.ResFile()
        openResult = resFile.Open("cache:/test_OpenCapitalOExistingFileForWritingAndWriteToIt.txt", 0)

        self.assertTrue(openResult)

        expectedContents = "contents of the file have been overwritten"
        resFile.Write(expectedContents.encode())
        resFile.Close()

        with open(filePath) as f:
            contents = f.read()

        self.assertEqual(contents, expectedContents)


    def test_OpenExistingFileForWritingAndWriteToIt(self):
        filePath = os.path.join(self.tmpDir, "test_OpenExistingFileForWritingAndWriteToIt.txt")
        self._createFile(filePath, b"this is a test")

        resFile = blue.ResFile()
        openResult = resFile.open("cache:/test_OpenExistingFileForWritingAndWriteToIt.txt", 0)

        self.assertTrue(openResult)

        expectedContents = "contents of the file have been overwritten"
        resFile.Write(expectedContents.encode())
        resFile.Close()

        with open(filePath) as f:
            contents = f.read()

        self.assertEqual(contents, expectedContents)


    def test_OpenCapitalOWhenFileDoesNotExist(self):
        resFile = blue.ResFile()
        openResult = resFile.Open("cache:/test.txt")

        self.assertFalse(openResult)

        resFile.Close()


    def test_OpenWhenFileDoesNotExist(self):
        resFile = blue.ResFile()
        self.assertRaises(blue.error, resFile.open, "cache:/test.txt")


    def test_ReadFromExistingFile(self):
        filePath = os.path.join(self.tmpDir, "test_ReadFromExistingFile.txt")
        expectedContents = b"this is a test"
        self._createFile(filePath, expectedContents)

        resFile = blue.ResFile()
        openResult = resFile.Open("cache:/test_ReadFromExistingFile.txt")

        self.assertTrue(openResult)

        try:
            contents = resFile.read()
            self.assertEqual(contents, expectedContents)
        finally:
            resFile.Close()


    def test_ReadPartialContentsFromExistingFile(self):
        filePath = os.path.join(self.tmpDir, "test_ReadPartialContentsFromExistingFile.txt")
        fullContents = b"012345678901234567890123456789"
        self._createFile(filePath, fullContents)

        resFile = blue.ResFile()
        openResult = resFile.Open("cache:/test_ReadPartialContentsFromExistingFile.txt")

        self.assertTrue(openResult)

        contents = resFile.read(10)
        expectedContents = fullContents[:10]

        self.assertEqual(contents, expectedContents)

        resFile.Close()


    def test_ReadPartialContentsWithSeekFromExistingFile(self):
        filePath = os.path.join(self.tmpDir, "test_ReadPartialContentsWithSeekFromExistingFile.txt")
        fullContents = b"012345678901234567890123456789"
        self._createFile(filePath, fullContents)

        resFile = blue.ResFile()
        openResult = resFile.Open("cache:/test_ReadPartialContentsWithSeekFromExistingFile.txt")

        self.assertTrue(openResult)

        resFile.seek(5)
        contents = resFile.read(10)
        expectedContents = fullContents[5:15]

        self.assertEqual(contents, expectedContents)

        resFile.Close()


    def test_CreateFile(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        resFile = blue.ResFile()
        resFile.Create("cache:/test.txt")
        self.assertTrue(os.path.exists(filePath))


    def test_CreateFileAndWrite(self):
        filePath = os.path.join(self.tmpDir, "test_CreateFileAndWrite.txt")
        resFile = blue.ResFile()
        resFile.Create("cache:/test_CreateFileAndWrite.txt")

        s0 = "This is a test"
        resFile.Write(s0.encode())
        resFile.Close()

        with open(filePath) as f:
            s1 = f.read()
            self.assertEqual(s0, s1)


    def test_CreateFileAndWriteAndReadBack(self):
        resFile = blue.ResFile()
        resFile.Create("cache:/test_CreateFileAndWriteAndReadBack.txt")

        s0 = b"This is a test"
        try:
            resFile.Write(s0)
            resFile.Seek(0)
            s1 = resFile.Read()
            self.assertEqual(s0, s1)
        finally:
            resFile.Close()



    def test_CreateFileAndWriteWhenFileExists(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        prevContents = b"this is a test with some long text that should be overwritten"
        self._createFile(filePath, prevContents)

        resFile = blue.ResFile()
        resFile.Create("cache:/test.txt")

        try:
            s0 = b"Here is the new (shorter) text"
            resFile.Write(s0)
            resFile.Seek(0)
            s1 = resFile.Read()
            self.assertEqual(s0, s1)
        finally:
            resFile.Close()


    def test_FileExists_WithExistingFile(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        self._createFile(filePath, b"")

        resFile = blue.ResFile()
        self.assertTrue(resFile.FileExists("cache:/test.txt"))


    def test_FileExists_FileDoesNotExist(self):
        resFile = blue.ResFile()
        self.assertFalse(resFile.FileExists("cache:/test.txt"))

    
    def _test_ReadUsingContextManager(self):
        """
        TODO: Test doesn't pass - has this ever worked?
        """
        filePath = os.path.join(self.tmpDir, "test.txt")
        expectedContents = "this is a test"
        self._createFile(filePath, expectedContents)

        with blue.ResFile() as resFile:
            openResult = resFile.Open("cache:/test.txt")
            self.assertTrue(openResult)

            contents = resFile.read()
            self.assertEqual(contents, expectedContents)


    def CreateLanguageFiles(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        fullContents = b"Default file"
        self._createFile(filePath, fullContents)
        zhFilePath = os.path.join(self.tmpDir, "test.ZH.txt")
        zhContents = b"Chinese file"
        self._createFile(zhFilePath, zhContents)


    def test_OpenLanguageSpecificFile_NoLanguageSet(self):
        self.CreateLanguageFiles()

        f = blue.ResFile()
        f.Open("res:/test.txt")
        contents = f.Read()
        f.Close()

        self.assertEqual(contents, b"Default file")


    def test_OpenLanguageSpecificFile_ChineseLanguageSet(self):
        self.CreateLanguageFiles()
        blue.os.languageID = "ZH"

        f = blue.ResFile()
        f.Open("res:/test.txt")
        contents = f.Read()
        f.Close()

        self.assertEqual(contents, b"Chinese file")


    def test_OpenLanguageSpecificFile_EnglishLanguageSet(self):
        self.CreateLanguageFiles()
        blue.os.languageID = "EN"

        f = blue.ResFile()
        f.Open("res:/test.txt")
        contents = f.Read()
        f.Close()

        self.assertEqual(contents, b"Default file")

# Copyright © 2023 CCP ehf.

from . import blueunittest
import blue
import os
import sys
import tempfile
import shutil


class TestPaths(blueunittest.TestCase):
    """
    A set of test cases for blue.paths.
    """
    def setUp(self):
        self.searchPaths = blue.paths.GetAllSearchPaths()

    def tearDown(self):
        for k, v in self.searchPaths.items():
            blue.paths.SetSearchPath(k, v)

    def testInitialWorkingDirectory(self):
        """
        Ensure that initial working directory equals current working directory.
        This assumes we don't change the cwd, so don't do that!
        """
        self.skipTest("disabled until we figure out if this test is still useful as the assertion breaks interpreter mode")

        expectedPath = os.getcwd().replace("\\", "/")
        self.assertEqual(expectedPath, str(blue.paths.initialWorkingDirectory))


    def testSettingCircularSearchPathFails(self):
        self.assertRaises(RuntimeError, blue.paths.SetSearchPath, "selfRef1", "selfRef1:")

        blue.paths.SetSearchPath("selfRef1", "selfRef2:")
        self.assertRaises(RuntimeError, blue.paths.SetSearchPath, "selfRef2", "selfRef1:")
        self.assertEqual(u"", blue.paths.GetSearchPath("selfRef2"))


    def testGetAndSetSearchPath(self):
        self.assertEqual(u"", blue.paths.GetSearchPath("test"))

        blue.paths.SetSearchPath("test", u"some/value")
        self.assertEqual(u"some/value", blue.paths.GetSearchPath("test"))

        blue.paths.SetSearchPath("test", u"")
        self.assertEqual(u"", blue.paths.GetSearchPath("test"))


    def testGetAllSearchPaths(self):
        blue.paths.ClearSearchPaths()
        allPaths = blue.paths.GetAllSearchPaths()
        self.assertEqual(0, len(allPaths))

        blue.paths.SetSearchPath("test", u"some/value")
        allPaths = blue.paths.GetAllSearchPaths()
        self.assertEqual(1, len(allPaths))
        self.assertEqual({"test": u"some/value"}, allPaths)

        blue.paths.SetSearchPath("test", u"")


class TestAPathsWithDiskAccess(blueunittest.TestCase):
    def setUp(self):
        self.tmpDir = os.path.realpath(tempfile.mkdtemp())
        self.searchPaths = blue.paths.GetAllSearchPaths()

    
    def tearDown(self):
        shutil.rmtree(self.tmpDir)
        blue.paths.ClearSearchPaths()
        for k, v in self.searchPaths.items():
            blue.paths.SetSearchPath(k, v)


    def testResolvePath_WithNoPathsSet(self):
        orgPath = "justSome.junk"
        resolvedPath = blue.paths.ResolvePath(orgPath)

        expectedPath = os.path.join(os.getcwd(), orgPath)
        expectedPath = expectedPath.replace("\\", "/")
        self.assertEqual(expectedPath, resolvedPath)


    def testResolvePath_WithNonExistingFile(self):
        blue.paths.SetSearchPath("cache", self.tmpDir)

        orgPath = "cache:/file.is.not.here"
        resolvedPath = blue.paths.ResolvePath(orgPath)

        expectedPath = os.path.join(self.tmpDir, "file.is.not.here")
        expectedPath = expectedPath.replace("\\", "/")

        self.assertEqual(resolvedPath, expectedPath)

        blue.paths.SetSearchPath("cache", u"")


    def testResolvePath_WithExistingFile(self):
        blue.paths.SetSearchPath("cache", self.tmpDir)

        expectedPath = os.path.join(self.tmpDir, "file.is.here")
        createdFile = open(expectedPath, "w")
        createdFile.close()

        orgPath = "cache:/file.is.here"
        resolvedPath = blue.paths.ResolvePath(orgPath)

        expectedPath = expectedPath.replace("\\", "/")

        self.assertEqual(resolvedPath, expectedPath)

        blue.paths.SetSearchPath("cache", u"")


    def _createFile(self, name, contents):
        createdFile = open(name, "w")
        createdFile.write(contents)
        createdFile.close()


    def _prepareResOneTwoThree(self):
        dirName1 = os.path.join(self.tmpDir, "resOne")
        os.mkdir(dirName1)
        dirName2 = os.path.join(self.tmpDir, "resTwo")
        os.mkdir(dirName2)
        dirName3 = os.path.join(self.tmpDir, "resThree")
        os.mkdir(dirName3)

        blue.paths.SetSearchPath("root", self.tmpDir)
        blue.paths.SetSearchPath("resOne", u"root:/resOne")
        blue.paths.SetSearchPath("resTwo", u"root:/resTwo")
        blue.paths.SetSearchPath("resThree", u"root:/resThree")
        blue.paths.SetSearchPath("res", u"resOne:;resTwo:;resThree:")


    def testResolvePath_WithMultipleFolders(self):
        self._prepareResOneTwoThree()

        filename = os.path.join(self.tmpDir, "resOne", "file1.txt")
        self._createFile(filename, "This is file one in resOne")

        filename = os.path.join(self.tmpDir, "resTwo", "file1.txt")
        self._createFile(filename, "This is file one in resTwo")

        filename = os.path.join(self.tmpDir, "resThree", "file2.txt")
        self._createFile(filename, "This is file two in resThree")

        resolvedPath = blue.paths.ResolvePath(u"res:/file1.txt")
        expectedPath = os.path.join(self.tmpDir, "resOne", "file1.txt")
        expectedPath = expectedPath.replace("\\", "/")

        self.assertEqual(resolvedPath, expectedPath)

        resolvedPath = blue.paths.ResolvePath(u"resTwo:/file1.txt")
        expectedPath = os.path.join(self.tmpDir, "resTwo", "file1.txt")
        expectedPath = expectedPath.replace("\\", "/")

        self.assertEqual(resolvedPath, expectedPath)


    def testResolvePathForWriting_WithMultipleFolders(self):
        self._prepareResOneTwoThree()

        resolvedPath = blue.paths.ResolvePathForWriting(u"res:/file1.txt")
        expectedPath = os.path.join(self.tmpDir, "resOne", "file1.txt")
        expectedPath = expectedPath.replace("\\", "/")

        self.assertEqual(resolvedPath, expectedPath)


    def testResolvePath_CaseMismatch(self):
        if sys.platform != "linux2":
            self.skipTest("Not applicable on this platform")

        blue.paths.SetSearchPath("cache", self.tmpDir)
        expectedPath = os.path.join(self.tmpDir, "FileWithCamelCase")
        createdFile = open(expectedPath, "w")
        createdFile.close()

        orgPath = "cache:/filewithcamelcase"
        resolvedPath = blue.paths.ResolvePath(orgPath)

        expectedPath = expectedPath.replace("\\", "/")

        self.assertEqual(resolvedPath, expectedPath)


    def testResolvePath_CaseMismatchMultipleCandidates(self):
        if sys.platform != "linux2":
            self.skipTest("Not applicable on this platform")

        dirName1 = os.path.join(self.tmpDir, "DirOne")
        os.mkdir(dirName1)
        dirName2 = os.path.join(self.tmpDir, "dirone")
        os.mkdir(dirName2)
        dirName3 = os.path.join(self.tmpDir, "DIRONE")
        os.mkdir(dirName3)

        blue.paths.SetSearchPath("cache", self.tmpDir)
        expectedPath = os.path.join(self.tmpDir, "DIRONE/FileWithCamelCase")
        createdFile = open(expectedPath, "w")
        createdFile.close()

        orgPath = "cache:/dirone/filewithcamelcase"
        resolvedPath = blue.paths.ResolvePath(orgPath)

        expectedPath = expectedPath.replace("\\", "/")

        self.assertEqual(resolvedPath, expectedPath)

    def testResolvePath_Folder(self):
        self._prepareResOneTwoThree()

        expectedPath = os.path.join(self.tmpDir, "resOne")
        expectedPath = expectedPath.replace("\\", "/") + "/"
        resolvedPath = blue.paths.ResolvePath("res:/")

        self.assertEqual(resolvedPath, expectedPath)

    def testResolvePathToRoot_SimpleCase(self):
        self._prepareResOneTwoThree()

        dirName = os.path.join(self.tmpDir, "resTwo")
        resolvedName = blue.paths.ResolvePathToRoot("resTwo", dirName)

        self.assertEqual(resolvedName, "resTwo:/")

    def testlistdir(self):
        expectedDirectoryContents = []
        dirName = os.path.join(self.tmpDir, "dirtest")
        os.mkdir(dirName)
        expectedDirectoryContents.append("dirtest")
        for i in range(10):
            fileName = "file%d.txt" % i
            expectedDirectoryContents.append(fileName)
            filePath = os.path.join(self.tmpDir, fileName)
            createdFile = open(filePath, "w")
            createdFile.write("this file %s" % filePath)
            createdFile.close()

        blue.paths.SetSearchPath("cache", self.tmpDir)
        directoryContents = blue.paths.listdir("cache:/")

        self.assertEqual(directoryContents, expectedDirectoryContents)


    def test_isdir(self):
        dirName = os.path.join(self.tmpDir, "dirTest")
        os.mkdir(dirName)

        filePath = os.path.join(self.tmpDir, "file.is.here")
        createdFile = open(filePath, "w")
        createdFile.close()

        blue.paths.SetSearchPath("cache", self.tmpDir)

        self.assertTrue(blue.paths.isdir("cache:/"))
        self.assertTrue(blue.paths.isdir("cache:/dirTest"))
        self.assertFalse(blue.paths.isdir("cache:/file.is.here"))
        self.assertFalse(blue.paths.isdir("cache:/file.is.not.here"))

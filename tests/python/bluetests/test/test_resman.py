# Copyright © 2023 CCP ehf.

import os
import unittest
import blue
import tempfile
import shutil

class TestResMan(unittest.TestCase):
    """
    A set of test cases for the Blue Resource Manager.
    """

    def setUp(self):
        blue.motherLode.clear()
        self.tmpDir = tempfile.mkdtemp()
        self.cacheSearchPath = blue.paths.GetSearchPath("cache")
        self.resSearchPath = blue.paths.GetSearchPath("res")
        blue.paths.SetSearchPath("cache", self.tmpDir)
        blue.paths.SetSearchPath("res", self.tmpDir)

    def tearDown(self):
        blue.paths.SetSearchPath("cache", self.cacheSearchPath)
        blue.paths.SetSearchPath("res", self.resSearchPath)
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

    def _createFile(self, name, contents):
        createdFile = open(name, "w")
        createdFile.write(contents)
        createdFile.close()

    def testInitialState(self):
        self.assertTrue(blue.resMan)
        self.assertTrue(blue.resMan.pendingLoads == 0)
        self.assertTrue(blue.resMan.pendingPrepares == 0)
        self.assertTrue(blue.motherLode)
        self.assertTrue(blue.motherLode.size() == 0)

    def testGetResource(self):
        # Get a resource. BlueTestAsyncRes doesn't really load anything
        # but simulates a load by sleeping when it should be loading
        # from disk and preparing the resource. This allows us to test
        # the mechanics of the resource manager.
        res1 = blue.resMan.GetResource("ss_001.blueasync")
        self.assertEqual(type(res1), blue.BlueTestAsyncRes)
        
        self.assertTrue(res1.isLoading)

        blue.resMan.Wait()

        self.assertFalse(res1.isLoading)
        self.assertTrue(res1.isGood)
        self.assertEqual(blue.motherLode.size(), 1)

    def testGetResource_UnicodeName(self):
        res1 = blue.resMan.GetResource(u"ss_001.blueasync")
        self.assertEqual(type(res1), blue.BlueTestAsyncRes)
        
        self.assertTrue(res1.isLoading)

        blue.resMan.Wait()

        self.assertFalse(res1.isLoading)
        self.assertTrue(res1.isGood)
        self.assertTrue(blue.motherLode.size() == 1)

    def testGetResource_WithEx(self):
        # Get a resource. BlueTestAsyncRes doesn't really load anything
        # but simulates a load by sleeping when it should be loading
        # from disk and preparing the resource. This allows us to test
        # the mechanics of the resource manager.
        res1 = blue.resMan.GetResource("ss_001.blueasync", "ex")
        self.assertEqual(type(res1), blue.BlueTestAsyncRes)
        self.assertTrue(res1.ex)
       
        self.assertTrue(res1.isLoading)

        blue.resMan.Wait()

        self.assertFalse(res1.isLoading)
        self.assertTrue(res1.isGood)
        self.assertTrue(blue.motherLode.size() == 1)

    def testGetResource_UnknownResourceType(self):
        self.assertRaises( RuntimeError, blue.resMan.GetResource, "ss_001.bogus_type")

    def testGetResource_SecondGetShouldBeShared(self):
        res1 = blue.resMan.GetResource("ss_001.blueasync")
        blue.resMan.Wait()
        res2 = blue.resMan.GetResource("ss_001.blueasync")
        blue.resMan.Wait()

        self.assertTrue(res1 == res2)

    def testGetResource_Caching(self):
        res1 = blue.resMan.GetResource("ss_001.blueasync")
        blue.resMan.Wait()
        del res1
        res2 = blue.resMan.GetResource("ss_001.blueasync")

        self.assertTrue(res2.isGood)
        self.assertFalse(res2.isLoading)

    def CreateLanguageFiles(self):
        filePath = os.path.join(self.tmpDir, "test.txt")
        fullContents = "Default file"
        self._createFile(filePath, fullContents)
        zhFilePath = os.path.join(self.tmpDir, "test.ZH.txt")
        zhContents = "Chinese file"
        self._createFile(zhFilePath, zhContents)

    def test_OpenLanguageSpecificFile_NoLanguageSet(self):
        self.CreateLanguageFiles()

        res = blue.resMan.GetResource("res:/test.txt")
        blue.resMan.Wait()
        self.assertEqual(res.text, "Default file")

    def test_OpenLanguageSpecificFile_ChineseLanguageSet(self):
        self.CreateLanguageFiles()
        blue.os.languageID = "ZH"
        res = blue.resMan.GetResource("res:/test.txt")
        blue.resMan.Wait()
        self.assertEqual(res.text, "Chinese file")

    def test_OpenLanguageSpecificFile_EnglishLanguageSet(self):
        self.CreateLanguageFiles()
        blue.os.languageID = "EN"

        res = blue.resMan.GetResource("res:/test.txt")
        blue.resMan.Wait()
        self.assertEqual(res.text, "Default file")

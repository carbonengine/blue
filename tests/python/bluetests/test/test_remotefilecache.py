# Copyright © 2023 CCP ehf.

from . import blueunittest
import unittest
import blue

SIMPLE_INDEX = """
res:/a,a,0,0
res:/b,b,0,0
res:/f1/a,f1_a,0,0
res:/f1/b,f1_b,0,0
res:/f1/c,f1_c,0,0
"""

ADDITIONAL_INDEX = """
res:/c,c,0,0
res:/d,d,0,0
res:/f2/a,f2_a,0,0
res:/f2/b,f2_b,0,0
res:/f2/c,f2_c,0,0
"""


@unittest.skipUnless(hasattr(blue, "RemoteFileCache"), "RemoteFileCache not in yet")
class TestRemoteFileCache(blueunittest.TestCase):
	"""
	A set of test cases for blue.RemoteFileCache.
	"""

	def setUp(self):
		self.remoteFileCache = blue.RemoteFileCache()

	
	def testFileExists_WithNoIndexSet(self):
		result = self.remoteFileCache.FileExists("res:/test.txt")
		self.assertFalse(result)

	
	def testFileExists_NotAResFile(self):
		result = self.remoteFileCache.FileExists("cache:/test.txt")
		self.assertFalse(result)

	
	def testFileExists(self):
		index = "res:/test.txt,cachedNameForTestDotText,0,0"
		self.remoteFileCache.AddFileIndex(index)
		result = self.remoteFileCache.FileExists("res:/test.txt")
		self.assertTrue(result)

	def testFilesExist_MultipleIndexes(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)
		self.remoteFileCache.AddFileIndex(ADDITIONAL_INDEX)
		fileFromFormerIndexExists = self.remoteFileCache.FileExists("res:/a")
		fileFromLatterIndexExists = self.remoteFileCache.FileExists("res:/b")
		self.assertTrue(fileFromFormerIndexExists)
		self.assertTrue(fileFromLatterIndexExists)

	def testFoldersExist_MultipleIndexes(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)
		self.remoteFileCache.AddFileIndex(ADDITIONAL_INDEX)
		folderFromFormerIndexExists = self.remoteFileCache.isdir("res:/f1")
		folderFromLatterIndexExists = self.remoteFileCache.isdir("res:/f2")
		self.assertTrue(folderFromFormerIndexExists)
		self.assertTrue(folderFromLatterIndexExists)
	
	def testlistdir_rootfolder(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)

		result = self.remoteFileCache.ListDir("res:/")
		self.assertEqual(["a", "b", "f1"], result)

	
	def testlistdir_subfolder(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)

		result = self.remoteFileCache.ListDir("res:/f1")
		self.assertEqual(["a", "b", "c"], result)

	
	def testlistdir_NotAResPath(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)

		self.assertRaises(RuntimeError, self.remoteFileCache.ListDir, "bla:/f1")

	
	def testlistdir_FolderNotFound(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)

		self.assertRaises(RuntimeError, self.remoteFileCache.ListDir, "res:/f2")

	
	def testisdir_rootfolder(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)
		
		result = self.remoteFileCache.isdir("res:/")
		self.assertTrue(result)

	
	def testisdir_subfolder(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)
		
		result = self.remoteFileCache.isdir("res:/f1")
		self.assertTrue(result)

	
	def testisdir_FileInRootFolder(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)
		
		result = self.remoteFileCache.isdir("res:/a")
		self.assertFalse(result)

	
	def testisdir_FileInSubFolder(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)
		
		result = self.remoteFileCache.isdir("res:/f1/a")
		self.assertFalse(result)

	
	def testisdir_NonExistingFolder(self):
		self.remoteFileCache.AddFileIndex(SIMPLE_INDEX)
		
		result = self.remoteFileCache.isdir("res:/f2")
		self.assertFalse(result)

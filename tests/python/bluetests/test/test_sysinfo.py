# Copyright © 2023 CCP ehf.

import binascii
import ctypes
import multiprocessing
import unittest
import platform
import os
import sys
import tempfile
import time

import blue


class TestSysInfo(unittest.TestCase):
    def _testDirectoryExists(self, dirPath):
        self.assertTrue(os.path.exists(dirPath))
        self.assertTrue(os.path.isdir(dirPath))

    def _testDirectoryIsWritable(self, dirPath):
        try:
            tempfile.NamedTemporaryFile(dir=dirPath)
        except (OSError, IOError) as e:
            self.fail('failed to create file %s' % e)

    def testProcessBitCount(self):
        self.assertEqual(blue.sysinfo.processBitCount, 64 if sys.maxsize > 2 ** 32 else 32)

    def testSystemBitCountIsInRange(self):
        self.assertIn(blue.sysinfo.systemBitCount, (32, 64))
        self.assertGreaterEqual(blue.sysinfo.systemBitCount, blue.sysinfo.processBitCount)
        self.assertLessEqual(blue.sysinfo.systemBitCount, blue.sysinfo.cpu.bitCount)

    def testSharedApplicationDataDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetSharedApplicationDataDirectory())

    def testUserDocumentsDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetUserDocumentsDirectory())

    def testUserDocumentsDirectoryIsWritable(self):
        self._testDirectoryIsWritable(blue.sysinfo.GetUserDocumentsDirectory())

    def testUserApplicationDataDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetUserApplicationDataDirectory())

    def testUserApplicationDataDirectoryIsWritable(self):
        self._testDirectoryIsWritable(blue.sysinfo.GetUserApplicationDataDirectory())

    def testSharedFontsDirectoryExists(self):
        self._testDirectoryExists(blue.sysinfo.GetSharedFontsDirectory())

    def testProcessTimesReturnsSaneValues(self):
        """
        System and user times can be 0 if
        GetProcessTimes is called early enough
        in the process.
        """
        times = blue.sysinfo.GetProcessTimes()
        self.assertGreaterEqual(times.systemTime, 0)
        self.assertGreaterEqual(times.userTime, 0)

    def testProcessTimesIncrease(self):
        before = blue.sysinfo.GetProcessTimes()
        count = 0
        # This is flaky
        for i in range(1000000):
            count += i
        self.assertGreater(blue.sysinfo.GetProcessTimes().userTime, before.userTime)

    def testThreadTimesReturnsSaneValues(self):
        """
        System and user times can be 0 if
        GetThreadTimes is called early enough
        in the thread.
        """
        times = blue.sysinfo.GetThreadTimes()
        self.assertGreaterEqual(times.systemTime, 0)
        self.assertGreaterEqual(times.userTime, 0)

    def testThreadTimesNotGreaterThanProcessTimes(self):
        thread = blue.sysinfo.GetThreadTimes()
        process = blue.sysinfo.GetThreadTimes()
        self.assertGreaterEqual(process.systemTime, thread.systemTime)
        self.assertGreaterEqual(process.userTime, thread.userTime)

    def testThreadTimesIncrease(self):
        before = blue.sysinfo.GetThreadTimes()
        count = 0
        # This is flaky
        for i in range(1000000):
            count += i
        self.assertGreater(blue.sysinfo.GetThreadTimes().userTime, before.userTime)

    def testProcessStartTimeIsReasonable(self):
        start = blue.sysinfo.processStartTime / 10000000 - 11644473600
        now = time.time()
        self.assertLess(start, now)
        self.assertLess(now - start, 10 * 60 * 60)

    def testMachineUuidIsNotEmpty(self):
        self.assertNotEqual(blue.sysinfo.machineUuid, '')


class TestCpu(unittest.TestCase):
    def testCpuCountIsCorrect(self):
        self.assertEqual(blue.sysinfo.cpu.logicalCpuCount, multiprocessing.cpu_count())

    def testIdentifierIsNotEmpty(self):
        self.assertNotEqual(blue.sysinfo.cpu.identifier, '')

    def testBrandIsNotEmpty(self):
        self.assertNotEqual(blue.sysinfo.cpu.brand, '')


class TestOs(unittest.TestCase):
    def testOsPlatformIsCorrect(self):
        if sys.platform == 'win32':
            self.assertEqual(blue.sysinfo.os.platform, blue.OsPlatform.WINDOWS)
        elif sys.platform == 'darwin':
            self.assertEqual(blue.sysinfo.os.platform, blue.OsPlatform.OSX)

    def testOsVersionIsCorrect(self):
        if sys.platform == 'win32':
            # This is intentionally non-functioning since the version returned by python is deprecated
            return
        elif sys.platform == 'darwin':
            platformVersion = [int(x) for x in platform.mac_ver()[0].split('.')]
        else:
            return
        self.assertEqual(blue.sysinfo.os.majorVersion, platformVersion[0])
        self.assertEqual(blue.sysinfo.os.minorVersion, platformVersion[1])
        if len(platformVersion) > 2:  # platform.mac_ver() doesn't provide a buildNumber
            self.assertEqual(blue.sysinfo.os.buildNumber, platformVersion[2])


class TestMemory(unittest.TestCase):
    def testPageFileMemorySizeInRange(self):
        memory = blue.sysinfo.GetMemory()
        self.assertGreaterEqual(memory.pageFile, 0)
        self.assertLessEqual(memory.pageFile, memory.workingSet)

    def testWorkingSetSizeIncreasesWithAllocation(self):
        before = blue.sysinfo.GetMemory()
        bufferSize = 1024 * 1024 * 4
        memory = ctypes.create_string_buffer(bufferSize)
        self.assertGreaterEqual(blue.sysinfo.GetMemory().workingSet, before.workingSet + bufferSize)
        del memory

    def testTotalPhysicalMemoryReasonable(self):
        memory = blue.sysinfo.GetMemory().totalPhysical
        self.assertGreaterEqual(memory, 1024 * 1024 * 1024)
        self.assertLess(memory, 1024 * 1024 * 1024 * 256)

    def testAvailablePhysicalMemoryIsLessThanTotal(self):
        memory = blue.sysinfo.GetMemory()
        self.assertLess(memory.availablePhysical, memory.totalPhysical)


class TestNetworkAdapters(unittest.TestCase):
    def test_name(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.name, str)

    def test_uuid(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.uuid, str)

    def test_macAddress(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.macAddress, bytes)

    def test_macAddressString(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            self.assertIsInstance(adapter.macAddressString, str)

    def test_macAddressString_matches_macAddress(self):
        for adapter in blue.sysinfo.GetNetworkAdapters():
            expected = binascii.a2b_hex("".join([b for b in adapter.macAddressString.split(":")]))
            self.assertEqual(expected, adapter.macAddress)

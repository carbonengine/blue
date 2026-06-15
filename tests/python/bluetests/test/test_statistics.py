# Copyright © 2026 CCP ehf.

import unittest
import blue


class TestBlueStatistics(unittest.TestCase):
    """
    A set of test cases for BlueStatistics.
    """

    def test_blueStatisticsTelemetryConfig(self):
        """
        Test access to the BlueStatisticsTelemetryConfig object.
        """

        # Is it even there?
        self.assertTrue(blue.BlueStatisticsTelemetryConfig)

        # Can we create an instance of it and populate its member attributes?
        config = blue.BlueStatisticsTelemetryConfig()
        config.applicationName = "test app"
        config.captureDuration = 42
        config.trackMemory = True

        # Does it have the expected attributes with the correct values?
        self.assertTrue(hasattr(config, "applicationName"))
        self.assertTrue(hasattr(config, "captureDuration"))
        self.assertTrue(hasattr(config, "trackMemory"))
        self.assertFalse(hasattr(config, "attributeDoesNotExist"))
        self.assertEqual(config.applicationName, "test app")
        self.assertEqual(config.captureDuration, 42)
        self.assertEqual(config.trackMemory, True)

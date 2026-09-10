# Copyright © 2026 CCP ehf.

import unittest


# Note that blue is imported by each test case rather than at module scope, as the blue module is
# mocked away while test cases are discovered and must not be touched at import time.


class TestBlueStatistics(unittest.TestCase):
    """
    A set of test cases for BlueStatistics.
    """

    def test_blueStatisticsTelemetryConfig(self):
        """
        Test access to the BlueStatisticsTelemetryConfig object.
        """
        import blue

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

    def test_registerTelemetryCategory(self):
        """
        Test registering a Telemetry category.
        """
        import blue

        testColor = blue.BlueTelemetryColor.Tomato

        category = blue.statistics.RegisterTelemetryCategory(
            "test category", testColor
        )

        self.assertEqual(category.name, "test category")
        self.assertEqual(category.color, testColor)

    def test_registerTelemetryCategoryWithColorObject(self):
        """
        Test that a category can be registered with the color of another category, and that the
        color it ends up with is the very same color it was registered with.
        """
        import blue

        first = blue.statistics.RegisterTelemetryCategory(
            "test category with a color", blue.BlueTelemetryColor.Tomato
        )
        second = blue.statistics.RegisterTelemetryCategory("test category sharing a color", first.color)

        self.assertEqual(second.color, first.color)
        self.assertEqual(second.color, blue.BlueTelemetryColor.Tomato)

    def test_registerTelemetryCategoryWithInvalidColor(self):
        """
        Test that a category cannot be registered with something that is not a color.
        """
        import blue

        with self.assertRaises(TypeError):
            blue.statistics.RegisterTelemetryCategory("test category with an invalid color", "Tomato")

        with self.assertRaises(TypeError):
            blue.statistics.RegisterTelemetryCategory("test category with a numeric color", 0xFF6347)

    def test_registerTelemetryCategoryTwice(self):
        """
        Test that registering an already registered category returns the existing one, keeping the
        color it was first registered with.
        """
        import blue

        first = blue.statistics.RegisterTelemetryCategory(
            "test category registered twice", blue.BlueTelemetryColor.Tomato
        )
        second = blue.statistics.RegisterTelemetryCategory(
            "test category registered twice", blue.BlueTelemetryColor.Teal
        )

        self.assertEqual(second.name, first.name)
        self.assertEqual(second.color, first.color)
        self.assertEqual(second.color, blue.BlueTelemetryColor.Tomato)

    def test_registerTelemetryCategoryWithoutName(self):
        """
        Test that a category without a name cannot be registered.
        """
        import blue

        with self.assertRaises(blue.TelemetryCategoryError):
            blue.statistics.RegisterTelemetryCategory("")

    def test_telemetryCategoryErrorIsARuntimeError(self):
        """
        Test that a category that cannot be registered can be caught as a RuntimeError.
        """
        import blue

        self.assertTrue(issubclass(blue.TelemetryCategoryError, RuntimeError))

        with self.assertRaises(RuntimeError):
            blue.statistics.RegisterTelemetryCategory("")

    def test_getRegisteredTelemetryCategories(self):
        """
        Test that the registered categories contain the built-in ones, the category registered by
        blue itself, and categories registered from Python.
        """
        import blue

        blue.statistics.RegisterTelemetryCategory("test category in category list")

        names = [category.name for category in blue.statistics.GetRegisteredTelemetryCategories()]

        self.assertIn("general", names)
        self.assertIn("cpp", names)
        self.assertIn("core", names)
        self.assertIn("blue", names)
        self.assertIn("test category in category list", names)

    def restoreActiveTelemetryCategories(self):
        """
        Restore the currently active categories once the running test is done. The active set is
        process wide state, so a test that changes it has to put it back for the tests that follow.
        """
        import blue

        active = blue.statistics.GetActiveTelemetryCategories()
        self.addCleanup(blue.statistics.SetActiveTelemetryCategories, active)

    def test_setActiveTelemetryCategories(self):
        """
        Test that the categories passed to SetActiveTelemetryCategories are the ones reported as
        active afterwards.
        """
        import blue

        self.restoreActiveTelemetryCategories()

        first = blue.statistics.RegisterTelemetryCategory("test category set active")
        second = blue.statistics.RegisterTelemetryCategory("test category also set active")

        blue.statistics.SetActiveTelemetryCategories([first, second])

        names = [category.name for category in blue.statistics.GetActiveTelemetryCategories()]

        # Active categories come back in registration order rather than the order they were passed in
        self.assertCountEqual(names, [first.name, second.name])

    def test_setActiveTelemetryCategoriesReplacesThePreviousOnes(self):
        """
        Test that setting the active categories replaces the previously active ones rather than
        adding to them.
        """
        import blue

        self.restoreActiveTelemetryCategories()

        first = blue.statistics.RegisterTelemetryCategory("test category active first")
        second = blue.statistics.RegisterTelemetryCategory("test category active second")

        blue.statistics.SetActiveTelemetryCategories([first])
        blue.statistics.SetActiveTelemetryCategories([second])

        names = [category.name for category in blue.statistics.GetActiveTelemetryCategories()]

        self.assertEqual(names, [second.name])

    def test_setActiveTelemetryCategoriesFromAnySequence(self):
        """
        Test that the categories can be handed over in any kind of sequence, not just a list.
        """
        import blue

        self.restoreActiveTelemetryCategories()

        category = blue.statistics.RegisterTelemetryCategory("test category set active as a tuple")

        blue.statistics.SetActiveTelemetryCategories((category,))

        names = [category.name for category in blue.statistics.GetActiveTelemetryCategories()]

        self.assertEqual(names, ["test category set active as a tuple"])

    def test_setNoActiveTelemetryCategories(self):
        """
        Test that an empty sequence deactivates every category.
        """
        import blue

        self.restoreActiveTelemetryCategories()

        category = blue.statistics.RegisterTelemetryCategory("test category deactivated again")
        blue.statistics.SetActiveTelemetryCategories([category])

        blue.statistics.SetActiveTelemetryCategories([])

        self.assertEqual(blue.statistics.GetActiveTelemetryCategories(), [])

    def test_setUnsetResetActiveTelemetryCategory(self):
        import blue

        self.restoreActiveTelemetryCategories()

        category = blue.statistics.RegisterTelemetryCategory("cpp")
        blue.statistics.SetActiveTelemetryCategories([category])
        self.assertIn(category, blue.statistics.GetActiveTelemetryCategories())
        blue.statistics.SetActiveTelemetryCategories([])
        self.assertNotIn(category, blue.statistics.GetActiveTelemetryCategories())
        blue.statistics.SetActiveTelemetryCategories([category])
        self.assertIn(category, blue.statistics.GetActiveTelemetryCategories())

    def test_telemetryCategoriesCompareByName(self):
        """
        Test that two lookups of the same registered category compare equal and hash the same, even
        though every lookup hands out a wrapper of its own.
        """
        import blue

        name = "test category compared by name"

        registered = blue.statistics.RegisterTelemetryCategory(name)
        lookedUp = [
            category
            for category in blue.statistics.GetRegisteredTelemetryCategories()
            if category.name == name
        ][0]
        other = blue.statistics.RegisterTelemetryCategory("test category compared to another")

        self.assertIsNot(registered, lookedUp)
        self.assertEqual(registered, lookedUp)
        self.assertNotEqual(registered, other)

        # Categories that compare equal have to hash the same to work as keys and in sets
        self.assertEqual(hash(registered), hash(lookedUp))
        self.assertEqual({registered: "value"}[lookedUp], "value")
        self.assertEqual(len({registered, lookedUp}), 1)

        # The type picked up both when it was finalized, so the methods on it agree with the
        # operators rather than with the identity comparison of the wrapper they are inherited from
        self.assertTrue(type(registered).__eq__(registered, lookedUp))
        self.assertFalse(type(registered).__eq__(registered, other))
        self.assertEqual(type(registered).__hash__(registered), hash(registered))

        # Comparing against anything but a category falls back to identity rather than raising
        self.assertNotEqual(registered, name)
        self.assertNotEqual(registered, None)

        with self.assertRaises(TypeError):
            registered < other

    def test_setActiveTelemetryCategoriesWithInvalidCategories(self):
        """
        Test that only categories can be activated, and that a rejected call leaves the active
        categories alone.
        """
        import blue

        self.restoreActiveTelemetryCategories()

        category = blue.statistics.RegisterTelemetryCategory("test category surviving a bad call")
        blue.statistics.SetActiveTelemetryCategories([category])

        with self.assertRaises(TypeError):
            blue.statistics.SetActiveTelemetryCategories(["test category surviving a bad call"])

        with self.assertRaises(TypeError):
            blue.statistics.SetActiveTelemetryCategories([category, None])

        with self.assertRaises(TypeError):
            blue.statistics.SetActiveTelemetryCategories(42)

        names = [category.name for category in blue.statistics.GetActiveTelemetryCategories()]

        self.assertEqual(names, ["test category surviving a bad call"])

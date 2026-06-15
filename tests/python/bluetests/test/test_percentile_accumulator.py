# Copyright © 2023 CCP ehf.

from . import blueunittest
import blue


class TestPercentileAccumulator(blueunittest.TestCase):

    def setUp(self):
        self.acc = blue.PercentileAccumulator()

    def testGettingPercentilesWithoutAddingResultsInEmptyList(self):
        actual = self.acc.GetPercentiles(0.0, 1.0)
        self.assertListEqual([], actual)

    def testTwoValues(self):
        self.acc.Add(1.0)
        self.acc.Add(2.0)
        actual = self.acc.GetPercentiles(1.0, 1.0)
        expected = [
            0.0,  # % < 1
            0.5,  # % < 2
        ]
        self.assertListEqual(expected, actual)

    def testClearingResetsAccumulator(self):
        self.acc.Add(1.0)
        self.acc.Add(2.0)
        self.acc.Clear()
        actual = self.acc.GetPercentiles(0.0, 1.0)
        expected = []
        self.assertListEqual(expected, actual)

    def testStartValueHigherThanMaxValueResultsInEmptyList(self):
        self.acc.Add(1.0)
        self.acc.Add(2.0)
        actual = self.acc.GetPercentiles(10.0, 1.0)
        expected = []
        self.assertListEqual(expected, actual)

    def testGetValuesForPercentiles(self):
        for i in range(100):
            self.acc.Add(i)
        expected = [89.0, 94.0, 99.0]
        actual = self.acc.GetValuesForPercentiles(0.9, 0.05)
        self.assertListEqual(expected, actual)

    def testGetValuesForPercentilesShouldReturnSameValuesForLargerSetWithSameDistribution(self):
        for i in range(10):
            self.acc.Add(i)
        smaller_dataset_results = self.acc.GetValuesForPercentiles(0.9, 0.05)
        for i in range(10):
            self.acc.Add(i)
        larger_dataset_results = self.acc.GetValuesForPercentiles(0.9, 0.05)
        self.assertListEqual(smaller_dataset_results, larger_dataset_results)

    def testGetValuesForPercentilesStopsAtFirstValueThatStepsOverThreshold(self):
        for i in range(10):
            self.acc.Add(i)
        expected = [7.0, 9.0]
        actual = self.acc.GetValuesForPercentiles(0.9, 0.2)
        self.assertListEqual(expected, actual)

    def testGetValuesForPercentilesStopsAtFirstValueThatEqualsThreshold(self):
        for i in range(10):
            self.acc.Add(i)
        expected = [8.0, 9.0]
        actual = self.acc.GetValuesForPercentiles(0.9, 0.1)
        self.assertListEqual(expected, actual)

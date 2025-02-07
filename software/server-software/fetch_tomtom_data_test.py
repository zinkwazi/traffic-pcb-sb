import unittest
from fetch_tomtom_three import *

# ================================
# Configuration Options
# ================================

TEST_DATA_DIR = "test_data"
TEST_TEMP_DIR_NAME = "test_temp"
TYPICAL_SMALL_FILENAME = "typical_small.csv"
MISSING_NUM_FILENAME = "missing_num.csv"
MANY_REFERENCES_FILENAME = "many_references.csv"

# ================================
# Tests
# ================================

class TestPruneCSVEntries(unittest.TestCase):

    def testTypical(self):
        test_input_file = TEST_DATA_DIR + "/" + TYPICAL_SMALL_FILENAME
        test_expected_dir = TEST_DATA_DIR + "/prune_csv_entries"

        # test north direction
        with open(test_expected_dir + "/testTypical_expected1.txt", 'r') as file:
            expected_ret = eval(file.read())

        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            ret = pruneCSVEntries(csv_reader, Direction.NORTH, False)
        self.assertEqual(ret, expected_ret, "Unexpected result for NORTH direction")

        # test south direction
        with open(test_expected_dir + "/testTypical_expected2.txt", 'r') as file:
            expected_ret = eval(file.read())

        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            ret = pruneCSVEntries(csv_reader, Direction.SOUTH, False)
        self.assertEqual(ret, expected_ret, "Unexpected result for SOUTH direction")
    
    def testMissing(self):
        test_input_file = TEST_DATA_DIR + "/" + MISSING_NUM_FILENAME
        test_expected_dir = TEST_DATA_DIR + "/prune_csv_entries"
        
        # test allow_missing = False
        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            self.assertRaises(RuntimeError, pruneCSVEntries, csv_reader, Direction.NORTH, False)
        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            self.assertRaises(RuntimeError, pruneCSVEntries, csv_reader, Direction.SOUTH, False)

        # test allow_missing = True
        with open(test_expected_dir + "/testMissing_expected1.txt", 'r') as file:
            expected_ret = eval(file.read())

        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            ret = pruneCSVEntries(csv_reader, Direction.NORTH, True)
        self.assertEqual(ret, expected_ret, "Unexpected result for NORTH direction")

        with open(test_expected_dir + "/testMissing_expected2.txt", 'r') as file:
            expected_ret = eval(file.read())

        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            ret = pruneCSVEntries(csv_reader, Direction.SOUTH, True)
        self.assertEqual(ret, expected_ret, "Unexpected result for SOUTH direction")

    def testManyReferences(self):
        test_input_file = TEST_DATA_DIR + "/" + MANY_REFERENCES_FILENAME
        test_expected_dir = TEST_DATA_DIR + "/prune_csv_entries"

        # test north
        with open(test_expected_dir + "/testManyReferences_expected1.txt", 'r') as file:
            expected_ret = eval(file.read())
        
        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            ret = pruneCSVEntries(csv_reader, Direction.NORTH, False)
        self.assertEqual(ret, expected_ret, "Unexpected result for NORTH direction")
        
        # test south
        with open(test_expected_dir + "/testManyReferences_expected2.txt", 'r') as file:
            expected_ret = eval(file.read())

        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            ret = pruneCSVEntries(csv_reader, Direction.SOUTH, False)
        self.assertEqual(ret, expected_ret, "Unexpected result for SOUTH direction")   

    def testInputGuards(self):
        test_input_file = TEST_DATA_DIR + "/" + TYPICAL_SMALL_FILENAME

        self.assertRaises(TypeError, pruneCSVEntries, None, Direction.NORTH, False)
        self.assertRaises(TypeError, pruneCSVEntries, None, Direction.NORTH, True)
        
        with open(test_input_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            self.assertRaises(ValueError, pruneCSVEntries, csv_reader, Direction.UNKNOWN, False)
            self.assertRaises(ValueError, pruneCSVEntries, csv_reader, Direction.UNKNOWN, True)

# ================================
# Test Setup
# ================================

if __name__ == "__main__":
    unittest.main()
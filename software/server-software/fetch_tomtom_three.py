#!/usr/bin/env python3

import sys
import requests
import csv
import tile_schema_pb2
import random
from enum import Enum
from typing import Any
import os
from datetime import datetime

# ================================
# Configuration Options
# ================================

USE_RANDOM_DATA = False
USE_FAKE_KEY = False

INPUT_CSV = "input/led_locations_V1_0_5.csv"
V2_0_0_ADD_INPUT_CSV = "input/led_loc_addendum_V2_0_0.csv"
OUTPUT_NORTH = "output/data_northV1_0_5.csv"
OUTPUT_SOUTH = "output/data_southV1_0_5.csv"
OUTPUT_NORTH_2 = "output/data_north_V1_0_3.dat"
OUTPUT_SOUTH_2 = "output/data_south_V1_0_3.dat"
OUTPUT_NORTH_TYPICAL = "output/typical_data_north.csv"
OUTPUT_SOUTH_TYPICAL = "output/typical_data_south.csv"
LOG_FILE = "output/fetch_tomtom_data.log"

class Direction(Enum):
    NORTH = 1
    SOUTH = 2
    UNKNOWN = 3

class SpeedType(Enum):
    CURRENT = 1
    TYPICAL = 2
    UNKNOWN = 3

# ================================
# Definitions
# ================================

LED_NUM_KEY = "LED Number"
FREEWAY_KEY = "Freeway"
DIRECTION_KEY = "Direction"
LONGITUDE_KEY = "Longitude"
LATITUDE_KEY = "Latitude"
TILE_KEY = "Tile"
OPENLR_CODE_KEY = "openLr Code"
FREE_FLOW_SPEED_KEY = "Free Flow Speed"
REFERENCE_KEY = "Reference"

JSON_TRUE_STRING = "true"
JSON_FALSE_STRING = "false"

# ================================
# Ensure necessary files exist
# ================================

def ensure_folder_exists(folder_path, log_path):
    """Create the folder if it doesn't exist. If log_file
    is not None, then an error creating the file will be
    written to the log."""
    if not os.path.exists(folder_path):
        try:
            os.mkdir(folder_path)
        except Exception as e:
            if log_path != None:
                log(e)

def ensure_file_exists(file_path, log_path, default_content=""):
    """Create the file with default content if it doesn't exist.
    If log_file is not None, then an error creating the file will
    be written to the log."""
    if not os.path.exists(file_path):
        try:
            with open(file_path, 'wb') as f:
                f.write(bytearray(default_content, 'utf-8'))
        except Exception as e:
            if file_path == log_path:
                print(e)
            if log_path != None and log_path != file_path:
                log(e)

# ================================
# Logging Function
# ================================

def log(message):
    with open(LOG_FILE, 'a') as log_file:
        log_file.write(f"{datetime.now()}: {message}\n")

# ================================
# API Functions
# ================================

def validEntrySegmentData(entry: dict[Any, str | Any]) -> bool:
    """
    Returns true if the entry contains information valid for use with the
    TomTom Flow Segment Data API endpoint, otherwise false.
    
    Requires:
        entry is not None
    """
    if entry[LATITUDE_KEY] is None:
        return False
    if entry[LONGITUDE_KEY] is None:
        return False
    if entry[OPENLR_CODE_KEY] is None:
        return False
    return True

def requestSegmentData(entry, speed_type, api_key):
    """
    Requests speed data from the TomTom Flow Segment Data endpoint. 

    Parameters:
        entry: The CSV entry holding information about the physical location
            to query.
        api_key: The API key to use when making requests.

    Returns:
        The speed retrieved from the endpoint if successful, otherwise -1.
        
        If there is a road closure, 0 is returned when speed_type is CURRENT 
        and -1 is returned when speed_type is TYPICAL.

        If the endpoint returns an openLR code that differs from that of the
        entry, then -1 is returned.

        Throws an exception if arguments are unexpected.
    """
    if entry is None:
        raise(ValueError, "entry is None")
    if speed_type == SpeedType.UNKNOWN:
        raise(ValueError, "speed_type is UNKNOWN")
    if api_key == "":
        raise(ValueError, "api_key is empty string")
    if not validEntrySegmentData(entry):
        log(f"LED {entry[LED_NUM_KEY]} contains invalid segment data.")
        return -1
    
    # random data check
    if USE_RANDOM_DATA:
        return random.randint(20, 75)

    # perform API request
    longitude, latitude = entry[LONGITUDE_KEY], entry[LATITUDE_KEY]
    log(f"Requesting segment data for coordinates ({longitude}, {latitude})...")
    try:
        response = requests.get(
            f"https://api.tomtom.com/traffic/services/4/flowSegmentData/relative0/10/json?key={api_key}&point={longitude},{latitude}&unit=mph&openLr=true"
        )
    except:
        log(f"request failed.")
        return -1
    if response.status_code != 200:
        log(f"Failed to retrieve data: {response.status_code} - {response.text}")
        return -1
    json_segment_data = response.json().get("flowSegmentData", {})
    
    # check for road closure
    closure_str = json_segment_data.get("roadClosure", "false")
    if closure_str == JSON_TRUE_STRING and speed_type == SpeedType.CURRENT:
        return 0
    elif closure_str == JSON_TRUE_STRING:
        return -1 # implicitly handles all other cases of speed_type

    # parse speed from JSON response
    if speed_type == SpeedType.CURRENT:
        speed = int(json_segment_data.get("currentSpeed", -1))
    elif speed_type == SpeedType.TYPICAL:
        speed = int(json_segment_data.get("freeFlowSpeed", -1))
    else:
        log(f"Requested data for unknown speed type")
        speed = -1
    if speed == -1:
        log(f"No speed data found in response: {response.text}")
        return -1
    
    # verify openLR codes match
    returned_openLR_code = json_segment_data.get("openlr", "")
    if returned_openLR_code != entry[OPENLR_CODE_KEY]:
        log(f"Query openLR code {returned_openLR_code} is not the expected code: {entry[OPENLR_CODE_KEY]}")
        return -1
    return speed

def validEntryTileData(entry: dict[Any, str | Any]) -> bool:
    """
    Returns true if the entry contains information valid for use with the
    TomTom Vector Flow Tiles API endpoint, otherwise false.

    Requires:
        entry is not None.
    """
    if entry[TILE_KEY] is None:
        return False
    value: str = entry[TILE_KEY]
    if value.count('/') != 2:
        return False
    return True
    
def requestTileData(entry: dict[Any, str | Any],
                    api_key: str
                    ) -> int:
    """
    Requests speed data from the TomTom Vector Flow Tiles endpoint.

    Parameters:
        entry: The CSV entry holding information about the physical location
            to query.
        api_key: The API key to use when making requests.
    
    Returns:
        The speed retrieved from the endpoint if successful, otherwise -1.

        Throws an exception if arguments are unexpected.
    """
    if entry is None:
        raise(ValueError, "entry is None")
    if api_key == "":
        raise(ValueError, "api_key is empty string")
    if not validEntryTileData(entry):
        log(f"LED {entry[LED_NUM_KEY]} contains invalid tile data (potentially by design).")
        return -1
    
    # random data check
    if USE_RANDOM_DATA:
        return random.randint(20, 75)
    
    # perform API request
    tile = entry[TILE_KEY]
    log(f"Requesting tile endpoint data for tile {tile}...")
    try:
        response = requests.get(
            f"https://api.tomtom.com/traffic/map/4/tile/flow/absolute/{tile}.pbf?key={api_key}&roadTypes=[0,1,2,3,4]"
        )
    except:
        log(f"Tile endpoint request failed.")
        return -1
    if response.status_code != 200:
        log(f"Failed to retrieve data: {response.status_code} - {response.text}")
        return -1

    # parse speed from Google Protobuf response
    response_tile = tile_schema_pb2.Tile()
    response_tile.ParseFromString(response.content)
    if len(response_tile.layers) != 1:
        log(f"Response contains {len(response_tile.layers)} layers when 1 is expected: {response_tile}")
        return -1
    response_layer = response_tile.layers[0]
    if len(response_layer.features) != 1:
        log(f"Response contains {len(response_layer.features)} features when 1 is expected: {response_tile}")
        return -1
    response_feature = response_layer.features[0]

    # determine traffic level tag number
    traffic_level_tag = -1
    for num, key in enumerate(response_layer.keys):
        if key == "traffic_level":
            traffic_level_tag = num
            break
    if traffic_level_tag == -1:
        log(f"Response layer does not contain a tag for \'traffic_level\': {response_tile}")
        return -1
    
    # find traffic level value index in feature
    traffic_level_feature = -1
    for tag, val in zip(*[iter(response_feature.tags)] * 2):
        if tag == traffic_level_tag:
            traffic_level_feature = val
            break
    if traffic_level_feature == -1:
        log(f"Response feature does not contain a tag corresponding to the traffic level: {response_tile}")
        return -1
    
    # retrieve current speed from traffic level value
    if len(response_layer.values) < traffic_level_feature + 1:
        log(f"Response layer does not contain a value at index {traffic_level_feature}: {response_tile}")
        return -1
    traffic_level_value = response_layer.values[traffic_level_feature]
    if not traffic_level_value.HasField("double_value"): # TODO: traffic level could be other type, although it has always been double so far
        log(f"Response traffic level is not a double: {response_tile}")
        return -1
    current_speed = round(traffic_level_value.double_value * 0.621371) # response gives kmph, convert to mph
    if current_speed == -1:
        log(f"No speed data found in response: {response_tile}")
        return -1
    return current_speed

# ================================
# General Functions
# ================================

def getReference(entry: dict[Any, str | Any]) -> int | None:
    """
    Returns the entry's reference if an LED number, otherwise returns None.
    """
    try:
        reference = int(entry.get("Reference", None))
    except:
        reference = None
    return reference

def pruneCSVEntries(csv_reader: csv.DictReader, 
                    direction: Direction, 
                    allow_missing: bool=False
                    ) -> list[tuple[dict[Any, str | Any], list[int]]]:
    """
    Searches through entries in csv_reader and compiles a list of pairs of
    entries and lists of LED numbers. This function disregards any entries whose 
    'Direction' field does not match the specified direction function argument.

    Note that entries with references to 0 are treated as non-reference entries.

    Parameters:
        csv_reader: Entries in the reader correspond to LEDs, with necessary
            fields being 'LED Number', 'Direction', 'Latitude', and 'Longitude'.
            An optional field is 'Reference', which can an entry can specify to 
            be one of another entry's 'LED Number' in order to tie multiple LED 
            numbers to the same physical coordinates.
        direction: Specifies which entries in the csv_reader the function should
            focus on; those with field 'Direction' not equal to this direction
            are skipped.
        allow_missing: False if the function should throw an error when the
            compiled entries do not form a set of strictly sequential LED
            numbers with no missing numbers. True if this should not be checked.

    Returns:
        A list of tuples containing pairs of entries to lists of LEDs. The LEDs
        in these pairs are LEDs that correspond to the physical coordinates of
        the pair's entry.

        Throws an exception if multiple entries with the specified direction
        and the same LED number are present in the csv_reader, in which case
        a log message is written with more details.

        Throws an exception if there are no entries with the specified direction.

        Throws an exception if allow_missing is false and LED numbers with the
        specified direction are not present in the csv_reader, in which case a
        log message is written with more details.

        Throws an exception if arguments are unexpected.
    """
    if csv_reader is None:
        raise TypeError("csv_reader argument is None")
    if direction is Direction.UNKNOWN:
        raise ValueError("direction argument is UNKNOWN")
    
    # the list of entry to LED pairings to be returned
    ret: list[tuple[dict[Any, str | Any], list[int]]] = []
    # a dictionary of LED nums to a list of LED nums that reference them
    ref_to_leds: dict[int, list[int]] = {}
    # a list of LEDs that have entries that have been iterated over
    seen_leds: set[int] = set([])
    # this is true if something funky is happening with the entries
    bad_entries: bool = False

    # populate ret and ref_to_leds with entries with no reference and with
    # a reference, respectively
    for entry in csv_reader:
        if entry['Direction'] == "North" and direction == Direction.SOUTH:
            continue
        if entry['Direction'] == "South" and direction == Direction.NORTH:
            continue
        led_num = int(entry['LED Number'])
        if led_num in seen_leds:
            log(f"Duplicate LED Number encountered: {led_num}")
            bad_entries = True
            continue
        seen_leds.add(led_num)
        reference = getReference(entry)
        if reference is None or reference == 0:
            ret.append((entry, [led_num]))
            continue
        # deal with reference if present
        if ref_to_leds.get(reference) is None:
            ref_to_leds[reference] = [led_num]
        else:
            ref_to_leds[reference].append(led_num)
    
    # runtime checks on entries
    if bad_entries:
        raise RuntimeError(f"Duplicate {direction} entries found in csv file")
    if not allow_missing:
        prev_led: int | None = None
        for num in sorted(seen_leds):
            if prev_led is None:
                prev_led = num
                continue
            if num != prev_led + 1:
                bad_entries = True
                prev_led += 1
                while prev_led != num:
                    log(f"missing {direction} LED {prev_led} in csv file")
                    prev_led += 1
            prev_led = num
    if bad_entries:
        raise RuntimeError(f"Missing {direction} entries in csv file")
    
    # add referenced LEDs to ret list
    # note that all entries in ret are no-reference entries
    for entry, leds in ret:
        ref_num = int(entry['LED Number'])
        if not ref_num in ref_to_leds:
            continue
        referencing_leds = ref_to_leds[ref_num]
        leds.extend(referencing_leds)
        del ref_to_leds[ref_num]
    
    # check for bad entries, which are the only remaining in ref_to_leds
    for ref_num, leds in ref_to_leds.items():
        bad_entries = True
        log(f"Entries {direction} LEDs {leds} reference {ref_num} invalidly")
    if bad_entries:
        raise RuntimeError(f"Bad {direction} references in csv file")
    
    return ret

def requestSpeeds(entry_led_pairs: list[tuple[dict[Any, str | Any], list[int]]],
                  speed_type: SpeedType,
                  api_key: str,
                  fail_with_zero: bool=True
                  ) -> list[tuple[int, int]]:
    """
    Performs TomTom API requests for each provided entry, first attempting to 
    use the Vector Flow Tiles endpoint then defaulting to the Flow Segment Data
    endpoint if it fails. Creates a list of tuples containing an LED number and
    speed pair.

    Note that if an entry freeway is \"Special\", then 0 is returned for the
    speed.

    If an API request fails, it is logged. Then, pairs with the corresponding
    LED numbers are created with -1 speed if fail_with_zero is true. If
    fail_with_zero is false, then pairs are created with -1 speed. A 0 speed
    is necessary for pre-V1_0_5 firmware versions, which require binary arrays
    of unsigned integers.

    Parameters:
        entry_led_pairs: A list returned from pruneCSVEntries containing tuples
            of entry, led list pairs. Each pair defines an API request that must
            be made and the LEDs to update based on the result.
        speed_type: The type of data to retrieve from TomTom.
        api_key: The API key to use when making API requests.
        fail_with_zero: Whether to create a pair with a speed of 0 or -1 when an 
            API request fails. If true, 0 is added to the pair, otherwise -1.

    Returns:
        A list of tuples containing an LED number and speed pair.
    """
    if entry_led_pairs is None:
        raise(TypeError, "entry_led_pairs argument is None")
    if speed_type is SpeedType.UNKNOWN:
        raise(ValueError, "speed_type argument is UNKNOWN")
    if api_key == "":
        raise(ValueError, "api_key argument is an empty string")

    ret: list[tuple[int, int]] = []
    for entry, leds in entry_led_pairs:
        if entry[FREEWAY_KEY] == "Special":
            log(f"Found special entry for LEDs {leds} from LED number {entry[LED_NUM_KEY]} entry")
            speed = 0
        else:
            speed = requestTileData(entry, api_key)
            if speed == -1:
                speed = requestSegmentData(entry, speed_type, api_key)
            if speed == -1 and fail_with_zero:
                log(f"failed to retrieve speed for LED {entry[LED_NUM_KEY]}, setting to 0")
                speed = 0
            log(f"Retrieved speed {speed} for LEDs {leds} from LED number {entry[LED_NUM_KEY]} entry")
        for led_num in leds:
            ret.append((led_num, speed))
    return ret

def createAddendum(addendum_folder, version, prev_addendum, addendum_data_file, direction, speed_type, api_key):
    '''Creates a new file in the addendum_folder that
    specifies changes or additions to an original file,
    prev_addendum. prev_addendum can either point to another
    addendum file, .add, or the original file. addendum_data_file
    is the led location data that will be used in creating the addendum.

    The addendum_key is a key used to generate the addendum verification
    string, which is then used by the device requesting data to verify
    that interpretation of the decoded data is correct. This key is specific
    to the device version specified and must be agreed upon between device
    firmware and the server; this ensures that only this version of device and
    firmware is using this addendum file. This key is generated based on the
    mapping of openLR numbers to LED numbers used in the addendum. This means
    that the device will only accept the data if the openLR numbers are what
    it expects and the key is the same.
    
    Addendum metadata is the first thing in the file, marked by
    {metadata}'\n\n' '''
    try:
        with open(addendum_data_file, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            entry_leds_pairs = pruneCSVEntries(csv_reader, direction, allow_missing=True)
        speeds = requestSpeeds(entry_leds_pairs, speed_type, api_key, fail_with_zero=False)

        log(f"Addendum Speeds: {speeds}")

        addendum_filename = addendum_folder + "/" + version + ".add"
        ensure_folder_exists(addendum_folder, LOG_FILE)
        ensure_file_exists(addendum_filename, LOG_FILE, "")
        with open(addendum_filename, 'w', newline='') as output_file:
            output_file.write(f"{{{prev_addendum}}}\n\n")
            csv_writer = csv.writer(output_file, dialect='excel')
            csv_writer.writerows(speeds)
    except Exception as e:
        log(e)
        return

# ================================
# Main Function
# ================================

def main(speed_type, direction, api_key, csv_filename, output_filename, output_filename_2=None):
    if direction == Direction.UNKNOWN:
        log("Invalid direction provided. Try North or South.")
        return False
    # Update main file
    try:
        with open(csv_filename, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            entry_leds_pairs = pruneCSVEntries(csv_reader, direction, allow_missing=False)
        speeds = requestSpeeds(entry_leds_pairs, speed_type, api_key, fail_with_zero=False)
        speeds = sorted(speeds, key=lambda ele: ele[0])

        # Debug log to check the contents of current_speeds
        log(f"Speeds: {speeds}")

        # Save the results to the output file in CSV format
        log(f"Writing csv file of length {len(speeds)} to {output_filename}")
        with open(output_filename, 'w', newline='') as out_file:
            csv_writer = csv.writer(out_file, dialect='excel')
            csv_writer.writerows(speeds)

        # Save the results to the secondary output file in binary format
        raw_speeds = [speed[1] for speed in sorted(speeds, key=lambda ele: ele[0])] # keep sort to be extra sure
        raw_speeds = [speed if speed != -1 else 0 for speed in raw_speeds] # backwards compatibility
        raw_speeds.insert(0, 0) # backwards compatibility
        byte_array = bytearray(raw_speeds)
        log(f"Writing byte array of length {len(byte_array)} to {output_filename_2}")
        with open(output_filename_2, 'wb') as out_file:
            out_file.write(byte_array)

        # Update version addendums
        log("Updating addendum V2_0_0")
        addendum_folder_name = output_filename + "_add"
        createAddendum(addendum_folder_name, "V2_0_0", output_filename, V2_0_0_ADD_INPUT_CSV, direction, speed_type, api_key)

        log(f"Successfully completed request for {direction.name}")
        return True
    except Exception as e:
        log(f"Error processing {direction.name} direction: {e}")
        return False
    
# ================================
# Run Requests for Both Directions
# ================================

if __name__ == "__main__":
    if len(sys.argv) < 1 and not USE_FAKE_KEY:
        print("Incorret usage: script api_key [typical]")
        exit()
    if USE_FAKE_KEY:
        api_key = "fakekey"
    else:
        api_key = sys.argv[1]

    # Ensure the log file exists
    ensure_file_exists(LOG_FILE, LOG_FILE, "")

    # Ensure output files exist
    ensure_file_exists(OUTPUT_NORTH, LOG_FILE, "")
    ensure_file_exists(OUTPUT_NORTH_2, LOG_FILE, "")
    ensure_file_exists(OUTPUT_SOUTH, LOG_FILE, "")
    ensure_file_exists(OUTPUT_SOUTH_2, LOG_FILE, "")

    if len(sys.argv) > 2 and sys.argv[2] == "typical":
        log("")
        log("Retrieving typical Northbound speeds")
        log("")
        main(SpeedType.TYPICAL, Direction.NORTH, api_key, INPUT_CSV, OUTPUT_NORTH_TYPICAL)
        log("")
        log("Retrieving typical Northbound speeds")
        log("")
        main(SpeedType.TYPICAL, Direction.SOUTH, api_key, INPUT_CSV, OUTPUT_SOUTH_TYPICAL)
    else:
        log("")
        log("Retrieving current Northbound speeds")
        log("")
        main(SpeedType.CURRENT, Direction.NORTH, api_key, INPUT_CSV, OUTPUT_NORTH, OUTPUT_NORTH_2)
        log("")
        log("Retrieving current Southbound speeds")
        log("")
        main(SpeedType.CURRENT, Direction.SOUTH, api_key, INPUT_CSV, OUTPUT_SOUTH, OUTPUT_SOUTH_2)
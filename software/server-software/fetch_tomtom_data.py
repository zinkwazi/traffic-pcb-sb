#!/usr/bin/env python3

import sys
import shutil
import requests
import csv
import json
from enum import Enum
import os
from datetime import datetime

# ================================
# Configuration Options
# ================================

API_KEY = ""
INPUT_CSV = "led_locations.csv"
OUTPUT_NORTH = "data_north_V1_0_0.json"
OUTPUT_SOUTH = "data_south_V1_0_0.json"
OUTPUT_NORTH_2 = "data_north_V1_0_3.dat"
OUTPUT_SOUTH_2 = "data_north_V1_0_3.dat"
LOG_FILE = "fetch_tomtom_data.log"

class Direction(Enum):
    NORTH = 1
    SOUTH = 2
    UNKNOWN = 3

# ================================
# Ensure necessary files exist
# ================================

def ensure_file_exists(file_path, log_path, default_content=""):
    """Create the file with default content if it doesn't exist.
    If log_file is not None, then an error creating the file will
    be written to the log."""
    if not os.path.exists(file_path):
        try:
            with open(file_path, 'wb') as f:
                f.write(bytearray(default_content, 'utf-8'))
        except Exception as e:
            if log_path != None:
                log(e)

# Ensure the log file exists
ensure_file_exists(LOG_FILE, "") # script will fail silently if
                                 # log file cannot be opened

# Ensure output files exist
ensure_file_exists(OUTPUT_NORTH, LOG_FILE, "")
ensure_file_exists(OUTPUT_SOUTH, LOG_FILE, "")
ensure_file_exists(OUTPUT_NORTH_2, LOG_FILE, "")
ensure_file_exists(OUTPUT_SOUTH_2, LOG_FILE, "")

# ================================
# Logging Function
# ================================

def log(message):
    with open(LOG_FILE, 'a') as log_file:
        log_file.write(f"{datetime.now()}: {message}\n")

# ================================
# API Functions
# ================================


def validEntry(entry):
    '''Determines whether the given entry 
    is a valid entry to use in requestData.'''

    return entry["Latitude"] != None and entry["Longitude"] != None

def requestData(entry):
    '''Requests speed data from the TomTom API. Fails 
    if entry is invalid, which is checked with validEntry.
    Returns found speed if successful, -1 otherwise.'''

    longitude, latitude = entry["Longitude"], entry["Latitude"]

    log(f"Requesting data for coordinates ({longitude}, {latitude})")
    try:
        response = requests.get(
            f"https://api.tomtom.com/traffic/services/4/flowSegmentData/relative0/10/json?key={API_KEY}&point={longitude},{latitude}&unit=mph&openLr=true"
        )
    except:
        log(f"request failed.")
        return -1
    if response.status_code != 200:
        log(f"Failed to retrieve data: {response.status_code} - {response.text}")
        return -1

    json_response = response.json()
    current_speed = int(json_response.get("flowSegmentData", {}).get("currentSpeed", -1))
    if current_speed == -1:
        log(f"No speed data found in response: {json_response}")
        return -1
    return current_speed

# ================================
# General Functions
# ================================

def decodeReferences(led_to_entry, max_led_num):
    '''Iterates through led_to_entry dictionary and converts
    its data to entry_to_leds. That is, it compiles all leds
    that reference a particular entry under one key. max_led_num
    is the maximum led number that exists, not one past it. Returns
    entry_to_leds if successful, otherwise None.'''

    entry_to_leds = {}
    bad_references = False # do not return straight away if successful,
                           # rather log all issues with the references
    
    for led_num, entry in led_to_entry.items():
        try:
            reference = int(entry.get("Reference", -1))
        except:
            reference = -1
        if reference != -1:
            # check reference validity and update entry
            try:
                double_ref = int(led_to_entry.get(reference, {}).get("Reference", -1))
            except:
                double_ref = -1
            if reference <= 0 or reference > max_led_num or double_ref != -1:
                log(f"Invalid reference at LED Number: {led_num}")
                bad_references = True
                continue
            entry = led_to_entry[reference]
            if entry == None or not validEntry(entry):
                log(f"Entry is invalid or missing from led_to_entry for reference at LED Number: {led_num}")
                bad_references = True
                continue
        if entry_to_leds.get(entry["internal_row_num"], None) == None:
            entry_to_leds[entry["internal_row_num"]] = []
        entry_to_leds[entry["internal_row_num"]].append(led_num)
    
    return entry_to_leds if not bad_references else None

# ================================
# Main Function
# ================================

def main(direction, key, csv_filename, output_filename, output_filename_2):
    if direction == Direction.UNKNOWN:
        log("Invalid direction provided. Try North or South.")
        return False

    try:
        with open(csv_filename, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            led_to_entry = {}
            max_led_num = 0
            bad_locations = False

            # Retrieve appropriate entries from csv file
            row_to_entry = {} # allows entry to be used as key in dictionary. See entry["internal_row_num"]
            for row_num, entry in enumerate(csv_reader):
                if entry['Direction'] == "North" and direction == Direction.SOUTH:
                    continue
                if entry['Direction'] == "South" and direction == Direction.NORTH:
                    continue
                if int(entry['LED Number']) in led_to_entry:
                    log(f"Duplicate LED Number encountered: {int(entry['LED Number'])}")
                    bad_locations = True
                else:
                    led_to_entry[int(entry['LED Number'])] = entry
                    row_to_entry[row_num] = entry
                    entry["internal_row_num"] = row_num # allows entry to be used as key in dictionary
                max_led_num = max(max_led_num, int(entry['LED Number']))

            # Search for missing LED numbers
            for led_num in range(1, max_led_num + 1):
                if led_num not in led_to_entry:
                    log(f"Missing LED Number: {led_num}")
                    bad_locations = True

            if bad_locations:
                log("Aborting due to bad location entries in input file.")
                return False

            # Decode entries that reference other LED numbers
            entry_to_leds = decodeReferences(led_to_entry, max_led_num)
            if entry_to_leds == None:
                log("Aborting due to bad references in input file.")
                return False                

            # Iterate through LED entries and retrieve data
            current_speeds = [-1] * (max_led_num + 1)
            for entry_row_num, leds in entry_to_leds.items():
                entry = row_to_entry[entry_row_num]
                current_speed = requestData(entry)
                if current_speed == -1:
                    log(f"No speed data found for LEDs {leds}")
                    continue

                for led_num in leds:
                    current_speeds[led_num] = current_speed
                log(f"Retrieved speed {current_speed} for LEDs {leds}")

            # Replace negative values with 0
            current_speeds = [0 if speed < 0 else speed for speed in current_speeds]

            # Debug log to check the contents of current_speeds
            log(f"Final current_speeds: {current_speeds}")

            # Save the results to the output file in binary format
            byte_array = bytearray(current_speeds)
            log(f"Writing byte array of length {len(byte_array)} to {output_filename}")
            with open(output_filename, 'wb') as out_file:
                out_file.write(byte_array)
            with open(output_filename_2, 'wb') as out_file:
                out_file.write(byte_array)

            log(f"Successfully completed request for {direction.name}")
            return True

    except Exception as e:
        log(f"Error processing {direction.name} direction: {e}")
        return False

# ================================
# Run Requests for Both Directions
# ================================

if __name__ == "__main__":
    # North direction
    main(Direction.NORTH, API_KEY, INPUT_CSV, OUTPUT_NORTH, OUTPUT_NORTH_2)

    # South direction
    main(Direction.SOUTH, API_KEY, INPUT_CSV, OUTPUT_SOUTH, OUTPUT_SOUTH_2)
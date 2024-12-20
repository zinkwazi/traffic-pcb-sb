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

API_KEY = "SLnwSJGmT5XAKAQpmNT3Ajf5Up75aIQP"
INPUT_CSV = "/home/bearanvil/public_html/current_data/led_locations.csv"
OUTPUT_NORTH = "/home/bearanvil/public_html/current_data/data_north_V1_0_0.json"
OUTPUT_SOUTH = "/home/bearanvil/public_html/current_data/data_south_V1_0_0.json"
LOG_FILE = "/home/bearanvil/scripts/fetch_tomtom_data.log"

class Direction(Enum):
    NORTH = 1
    SOUTH = 2
    UNKNOWN = 3

# ================================
# Ensure necessary files exist
# ================================

def ensure_file_exists(file_path, default_content=""):
    """Create the file with default content if it doesn't exist."""
    if not os.path.exists(file_path):
        with open(file_path, 'wb') as f:
            f.write(bytearray(default_content, 'utf-8'))

# Ensure the log file exists
ensure_file_exists(LOG_FILE, "")

# Ensure output files exist
ensure_file_exists(OUTPUT_NORTH, "")
ensure_file_exists(OUTPUT_SOUTH, "")

# ================================
# Logging Function
# ================================

def log(message):
    with open(LOG_FILE, 'a') as log_file:
        log_file.write(f"{datetime.now()}: {message}\n")

# ================================
# Main Function
# ================================

def main(direction, key, csv_filename, output_filename):
    if direction == Direction.UNKNOWN:
        log("Invalid direction provided. Try North or South.")
        return False

    try:
        with open(csv_filename, 'r') as input_file:
            csv_reader = csv.DictReader(input_file, dialect='excel')
            led_to_entry = {}
            max_led_num = 0
            bad_locations = False

            for entry in csv_reader:
                if entry['Direction'] == "North" and direction == Direction.SOUTH:
                    continue
                if entry['Direction'] == "South" and direction == Direction.NORTH:
                    continue
                if int(entry['LED Number']) in led_to_entry:
                    log(f"Duplicate LED Number encountered: {int(entry['LED Number'])}")
                    bad_locations = True
                else:
                    led_to_entry[int(entry['LED Number'])] = entry
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
            locs_to_leds = {}
            bad_references = False
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
                
                if entry["Longitude"] == None or entry["Latitude"] == None:
                    log(f"Missing longitude/latitude at LED Number: {led_num}")
                    bad_references = True
                    continue
                location = (entry["Longitude"], entry["Latitude"])
                if locs_to_leds.get(location, None) == None:
                    locs_to_leds[location] = []
                locs_to_leds[location].append(led_num)

            if bad_references:
                log("Aborting due to bad references in input file.")
                return False

            # Iterate through LED entries and retrieve data
            current_speeds = [-1] * (max_led_num + 1)
            for location, leds in locs_to_leds.items():
                longitude, latitude = location
                
                log(f"Requesting data for ({longitude}, {latitude})")
                response = requests.get(
                    f"https://api.tomtom.com/traffic/services/4/flowSegmentData/relative0/10/json?key={key}&point={longitude},{latitude}&unit=mph&openLr=true"
                )
                if response.status_code != 200:
                    log(f"Failed to retrieve data: {response.status_code} - {response.text}")
                    continue
                json_response = response.json()
                current_speed = int(json_response.get("flowSegmentData", {}).get("currentSpeed", -1))
                if current_speed == -1:
                    log(f"No speed data found for LEDs {leds}: {json_response}")
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
    main(Direction.NORTH, API_KEY, INPUT_CSV, OUTPUT_NORTH)

    # South direction
    main(Direction.SOUTH, API_KEY, INPUT_CSV, OUTPUT_SOUTH)

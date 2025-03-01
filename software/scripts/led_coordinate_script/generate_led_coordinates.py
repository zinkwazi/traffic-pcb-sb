"""
generate_led_coordinates.py

Assists in the generation of C arrays which describe the x, y PCB coordinates
of each LED on a version of the board.

Input: A CSV file containing the CPL data of every designator on the board. This
should include at least the following headers: "Designator", "Mid X", "Mid Y".
The script will prune all designators not of the form "LEDx", where x is a 
unique integer.

Example Input:

Designator,Val,Package,Mid X,Mid Y,Rotation,Layer
F1,3A,Fuse_1206_3216Metric,9.271,-182.499,-90,top
F2,3A,Fuse_1206_3216Metric,58,-163.5,180,top
J2,USB C,USB_C_Receptacle_HRO_TYPE-C-31-M-12,4,-177.8,-90,top
LED1,LED_ARGB,LED_RGB_0606,5.0292,-27.0256,0,top
LED2,LED_ARGB,LED_RGB_0606,39.7256,-29.972,0,top
LED2,LED_ARGB,LED_RGB_0606,500,500,0,top
LED6,LED_ARGB,LED_RGB_0606,197.9168,-86.5124,0,top
LED5,LED_ARGB,LED_RGB_0606,201.1172,-88.8492,0,top
LED4,LED_ARGB,LED_RGB_0606,204.8256,-90.4748,0,top

Output: A text file containing the C array describing the PCB's LED coordinates,
where each index 'i' in the array corresponds to LED 'i' and index 0 not
corresponding to any LED. A comment is created alongside any indices that did
not have a corresponding entry in the input file. Fractional portions of the
input coordinates are rounded to the nearest integer. Output data is arbitrarily
chosen to correspond to a single designator if there are duplicates, with
a comment at the index documenting it.

Example Output:

/* A mapping from LED number to corresponding 
x, y PCB coordinates. */
static const LEDCoord LEDNumToCoord[] = {
    {0, 0}, // there is no LED with number 0
    {5, -27}, // 1
    {40, -30}, // 2, duplicate detected
    {0, 0}, // there is no LED with number 3
    {205, -90}, // 4
    {201, -89}, // 5
};

Example Explanation: Input designators F1, F2, and J2 are ignored and do
not affect the output. Similarly, the order of designators "LEDx" is ignored.
Index 1 of the output contains the "Mid X" and "Mid Y" respectively, rounded
to the nearest integer. The LED2 designator is duplicated, so index 2 
arbitrarily corresponds to only one of the entries. The fact that it is 
duplicated is commented. There is no "LED3" designator, so index 3 of the 
output is set to {0, 0} and a comment is created documenting its absence.

Usage: python3 generate_led_coordinates.py inputCPL.csv [output.txt]

If no output file is provided, the script will present the generated struct
to stdout.
"""

import sys
import os
import csv
import re

# ================================
# Configuration Options
# ================================



# ================================
# Definitions
# ================================

DESIGNATOR_KEY = "Designator"
MID_X_KEY = "Mid X"
MID_Y_KEY = "Mid Y"
LED_REGEX = "LED\d+"
LED_NUM_REGEX = "\d+"

# ================================
# General Functions
# ================================

def ensure_file_exists(file_path, default_content=""):
    """Create the file with default content if it doesn't exist.
    If log_file is not None, then an error creating the file will
    be written to the log."""
    if not os.path.exists(file_path):
        try:
            with open(file_path, 'wb') as f:
                f.write(bytearray(default_content, 'utf-8'))
        except Exception as e:
            print(e)

def pruneCSVEntries(csv_reader):
    """Searches through entries in csv_reader and compiles a dictionary
    of led nums to pairs of (entry, comment). It contains all LED designators 
    matching LED_REGEX, with the LED number extracted by LED_NUM_REGEX."""
    ret = {}
    for entry in csv_reader:
        match = re.match(LED_REGEX, entry[DESIGNATOR_KEY])
        if match == None:
            continue
        led_num_match = re.search(LED_NUM_REGEX, entry[DESIGNATOR_KEY])
        if led_num_match == None:
            print(f"Regex matched {entry[DESIGNATOR_KEY]}, but led num did not match.")
            exit()
        led_num = int(led_num_match.group(0))
        if ret.get(led_num) == None:
            ret[led_num] = (entry, f"{led_num}")
        else:
            ret[led_num] = (ret[led_num][0], f"{led_num}, duplicate detected")
    return ret

        

# ================================
# Main Function
# ================================

def main(input_filename, output_filename):
    with open(input_filename, 'r') as input_file:
        csv_reader = csv.DictReader(input_file, dialect='excel')
        led_to_entry = pruneCSVEntries(csv_reader)
        
    max_led_num = max(led_to_entry.keys())
    
    if output_filename == None:
        print("/* A mapping from LED number to corresponding")
        print("x, y PCB coordinates. */")
        print("static const LEDCoord LEDNumToCoord[] = {")
        for ndx in range(0, max_led_num + 1):
            if led_to_entry.get(ndx) == None:
                print(f"\t{{0, 0}}, // there is no LED with number {ndx}")
            else:
                entry, comment = led_to_entry[ndx]
                x = round(float(entry[MID_X_KEY]))
                y = round(float(entry[MID_Y_KEY]))
                print(f"\t{{{x}, {y}}}, // {comment}")
        print("};")
    else:
        with open(output_filename, 'w') as output_file:
            output_file.write("/* A mapping from LED number to corresponding\n")
            output_file.write("x, y PCB coordinates. */\n")
            output_file.write("static const LEDCoord LEDNumToCoord[] = {\n")
            for ndx in range(0, max_led_num + 1):
                if led_to_entry.get(ndx) == None:
                    output_file.write(f"\t{{0, 0}}, // there is no LED with number {ndx}\n")
                else:
                    entry, comment = led_to_entry[ndx]
                    x = round(float(entry[MID_X_KEY]))
                    y = round(float(entry[MID_Y_KEY]))
                    output_file.write(f"\t{{{x}, {y}}}, // {comment}\n")
            output_file.write("};\n")



if __name__ == "__main__":
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print("Incorrect usage: script input.csv [output.txt]")
        exit()
    
    if sys.argv[1] == "help" or sys.argv[1] == "Help":
        print("See README.md, or read comment in script.")
        print()
        print("usage: script input.csv [output.txt]")
        exit()
    
    if len(sys.argv) == 2:
        main(sys.argv[1], None)
    elif len(sys.argv) == 3:
        main(sys.argv[1], sys.argv[2])
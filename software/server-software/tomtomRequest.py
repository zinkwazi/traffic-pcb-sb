from enum import Enum
import sys
import shutil
import tempfile
import requests
import csv
import json

class Direction(Enum):
    NORTH = 1
    SOUTH = 2
    UNKNOWN = 3

def main(direction, key, csvDict, outFile):
    if direction == Direction.UNKNOWN:
        print("Invalid direction provided. Try North/north/NORTH or South/south/SOUTH as the first argument.")
        return
    # create dictionary based on led number to entries for referencing
    ledDict = {}
    maxLEDNum = 0
    badLocations = False
    for entry in csvDict:
        if entry['Direction'] == "North" and direction == Direction.SOUTH:
            continue
        if entry['Direction'] == "South" and direction == Direction.NORTH:
            continue
        if int(entry['LED Number']) in ledDict:
            print(f"Encountered duplicate LED Number: {int(entry['LED Number'])}")
            badLocations = True
        else:
            ledDict[int(entry['LED Number'])] = entry
        if int(entry['LED Number']) > maxLEDNum:
            maxLEDNum = int(entry['LED Number'])
    # Search for missing LED numbers
    for LEDNum in range(1, maxLEDNum + 1):
        if LEDNum not in ledDict:
            print(f"Did not find LED Number: {LEDNum}")
            badLocations = True
    if badLocations:
        print("Aborting due to bad location entries in input file")
        return False # reverts current file changes
    # iterate through LED entries and retrieve data
    invalidReference = False
    currentSpeeds = [-1] * (maxLEDNum + 1)
    for ledNum, entry in ledDict.items():
        ledNum = int(entry['LED Number'])
        currentSpeed = -1
        isReference = False
        reference = -1
        if int(entry['Free Flow Speed']) < 0:
            # this LED references another
            isReference = True
            reference = -int(entry['Free Flow Speed'])
            if reference > maxLEDNum:
                print(f"Invalid reference encountered at LED Number: {ledNum}")
                invalidReference = True
                continue
            entry = ledDict[reference]
            if currentSpeeds[reference] != -1:
                currentSpeed = currentSpeeds[reference]
        # check for a double reference, which is not allowed
        if int(entry['Free Flow Speed']) < 0:
            print(f"Double reference encountered at LED Number: {ledNum}")
            invalidReference = True
            continue # print out all double references
        # request current speed from TomTom if unknown
        if currentSpeed == -1:
            response = requests.get(f"https://api.tomtom.com/traffic/services/4/flowSegmentData/relative0/10/json?key={key}&point={entry['Longitude']},{entry['Latitude']}&unit=mph&openLr=true")
            if response.status_code != 200:
                print(response)
                print(f"Failed to retrieve data for LED Number {ledNum}")
                continue
            jsonResponse = json.loads(response.text)
            currentSpeed = int(jsonResponse["flowSegmentData"]["currentSpeed"])
            currentSpeeds[ledNum] = currentSpeed
            if isReference and currentSpeeds[reference] == -1:
                # update reference speed if necessary
                currentSpeeds[reference] = currentSpeed
            print(f"retrieved speed {currentSpeed} for LED {int(entry['LED Number'])}")
        else:
            currentSpeeds[ledNum] = currentSpeed
    if invalidReference:
        return False # reverts current file changes
    # convert to JSON and send to output file
    json.dump(currentSpeeds, outFile)
    return True
    
            

if __name__ == "__main__":
    # Read command line arguments
    if len(sys.argv) != 4 + 1:
        print(f"Invalid number of command line arguments provided. 4 expected, {len(sys.argv) - 1} found.")
        sys.exit()

    directionStr = sys.argv[1]
    direction = Direction.UNKNOWN
    if directionStr == "North" or directionStr == "north" or directionStr == "NORTH":
        direction = Direction.NORTH
    elif directionStr == "South" or directionStr == "south" or directionStr == "SOUTH":
        direction = Direction.SOUTH

    key = sys.argv[2]
    print(f"key: {key}")

    csvFilename = sys.argv[3]
    print(f"input filename: {csvFilename}")
    try:
        inputFile = open(csvFilename, "r")
    except:
        print(f"Could not open csv file: {csvFilename}")
        sys.exit()
    try:
        csvDict = csv.DictReader(inputFile, dialect='excel')
        print(f"csv fieldnames: {csvDict.fieldnames}")
    except:
        print("Could not open csv file: " + csvFilename)
        sys.exit()
    # Save current output file in case of bad execution
    outFilename = sys.argv[4]
    print(f"output filename: {outFilename}")
    savedOutputFile = tempfile.TemporaryFile(mode='r+')
    try:
        outFile = open(outFilename, "r")
    except:
        print(f"Could not open output file: {outFilename}")
        sys.exit()
    shutil.copyfileobj(outFile, savedOutputFile)
    outFile.close()
    # open output file for writing
    try:
        outFile = open(outFilename, "w", newline='') # newline='' necessary for csv writer
    except:
        print(f"Could not open output file for writing: {outFilename}")
        sys.exit()
    # Execute script and handle failure
    successful = main(direction, key, csvDict, outFile)
    outFile.close()
    if not successful:
        savedOutputFile.seek(0)
        outFile = open(outFilename, "w")
        outFile.write("")
        shutil.copyfileobj(savedOutputFile, outFile)
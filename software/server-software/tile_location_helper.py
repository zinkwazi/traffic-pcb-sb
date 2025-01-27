#!/usr/bin/env python3

import os
import sys
import math
import requests
import tile_schema_pb2

# ================================
# Configuration
# ================================

API_KEY = ""

# ================================
# General Functions
# ================================

def requestData(tile, key):
    '''Requests speed data from the TomTom API. Fails 
    if entry is invalid, which is checked with validEntry.
    Returns found speed if successful, -1 otherwise.'''

    print(f"Requesting data for tile {tile}")
    try:
        response = requests.get(
            f"https://api.tomtom.com/traffic/map/4/tile/flow/absolute/{tile}.pbf?key={key}&roadTypes=[0,1,2,3,4]"
        )
    except:
        print(f"request failed.")
        return -1
    if response.status_code != 200:
        print(f"Failed to retrieve data: {response.status_code} - {response.text}")
        return -1

    response_tile = tile_schema_pb2.Tile()
    response_tile.ParseFromString(response.content)
    print()
    print(response_tile)
    if len(response_tile.layers) != 1:
        print(f"Response contains {len(response_tile.layers)} layers when only 1 is expected.")
        return -1
    response_layer = response_tile.layers[0]
    if len(response_layer.features) != 1:
        print(f"Response contains {len(response_layer.features)} features when only 1 is expected.")
        return -1
    response_feature = response_layer.features[0]

    # determine traffic level tag number
    traffic_level_tag = -1
    for num, key in enumerate(response_layer.keys):
        if key == "traffic_level":
            traffic_level_tag = num
            break
    if traffic_level_tag == -1:
        print(f"Response layer does not contain a tag for \'traffic_level\'.")
        return -1
    
    # find traffic level value index in feature
    traffic_level_feature = -1
    for tag, val in zip(*[iter(response_feature.tags)] * 2):
        if tag == traffic_level_tag:
            traffic_level_feature = val
            break
    if traffic_level_feature == -1:
        print(f"Response feature does not contain a tag corresponding to the traffic level.")
        return -1
    
    # retrieve current speed from traffic level value
    if len(response_layer.values) < traffic_level_feature + 1:
        print(f"Response layer does not contain a value at index {traffic_level_feature}.")
        return -1
    traffic_level_value = response_layer.values[traffic_level_feature]
    if not traffic_level_value.HasField("double_value"): # TODO: traffic level could be other type, although it has always been double so far
        print(f"Response traffic level is not a double. If an integer, tell Jaden :)")
        return -1
    current_speed = round(traffic_level_value.double_value * 0.621371) # response gives kmph, convert to mph
    if current_speed == -1:
        print(f"No speed data found in response.")
        return -1
    return current_speed

def latLonToTileZXY(lat, lon):
    ZOOM_LEVEL = 22
    MIN_LAT = -85.051128779807
    MAX_LAT = 85.051128779806
    MIN_LON = -180.0
    MAX_LON = 180.0

    if lat < MIN_LAT or lat > MAX_LAT:
        print(f"Latitude is out of range [{MIN_LAT}, {MAX_LAT}].")
    
    if lon < MIN_LON or lon > MAX_LON:
        print(f"Longitude is out of range [{MIN_LON}, {MAX_LON}]")

    xyTilesCount = pow(2, ZOOM_LEVEL)
    x = int(math.floor(((lon + 180.0) / 360.0) * xyTilesCount))
    y = int(math.floor(
      ((1.0 -
        math.log(
          math.tan((lat * math.pi) / 180.0) +
            1.0 / math.cos((lat * math.pi) / 180.0)
        ) /
          math.pi) /
        2.0) *
        xyTilesCount
    ))
    return f"{ZOOM_LEVEL}/{x}/{y}"
    

# ================================
# Main Function
# ================================

def main(latitude, longitude, key):
    print()
    tile = latLonToTileZXY(float(latitude), float(longitude))
    print(f"lat: {latitude}")
    print(f"lon: {longitude}")
    print(f"zxy: {tile}")
    print()
    print(f"{latitude},{longitude},{tile}")
    print()
    current_speed = requestData(tile, key)
    if current_speed != -1:
        print(f"speed: {current_speed}")
    print()


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("invalid number of arguments. example usage: script.py latitude, longitude.")
    latitude, longitude = sys.argv[1], sys.argv[2]
    main(latitude, longitude, API_KEY)
    
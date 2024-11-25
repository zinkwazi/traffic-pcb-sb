# traffic-pcb-sb
This project is a decorative and interactive display piece that displays the current traffic in the greater Los Angeles area. It uses an ESP32 to connect to [TomTom's](https://www.tomtom.com/) traffic flow API, which supplies real-time traffic data of OpenLR road segments. Each LED corresponds to an OpenLR road segment and is queried from TomTom both periodically and when the user requests a direction change. Please note that LED refreshes are requeried and not buffered, so repetitive changes in direction will quickly rack up API calls (slightly less than 326 per refresh)! That said, please enjoy the pretty and informative lights!

# Features
1. Real-time traffic updates of nearly 650 data points requested from TomTom (approximately 326 in each direction).
3. Over the air (OTA) updates.
4. UART configuration menu (115200 baud) stored in non-volatile memory (remembered across reboots).
5. Optional JTAG debugging circuitry (electrically isolated pinout).

# User Manual
### Quickstart Guide
**Under Construction**

### Board Anatomy

**Under Construction**


# Contribution Guide
**Under Construction**

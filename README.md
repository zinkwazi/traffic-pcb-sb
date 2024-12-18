# Traffic PCB SB
This project is a decorative and interactive display piece that shows the current traffic in the greater Los Angeles area. It uses an ESP32 to connect to [TomTom's](https://www.tomtom.com/) traffic flow API, which supplies real-time traffic data of OpenLR road segments. Each LED corresponds to an OpenLR road segment and is queried from TomTom both periodically every hour and when the user requests a direction change. Please note that LED refreshes are requeried and not buffered, so repetitive changes in direction will quickly rack up API calls (slightly less than 326 per refresh)! That said, please enjoy the pretty and informative lights!

# Features
1. Real-time traffic updates of nearly 650 data points requested from TomTom (approximately 326 in each direction).
3. Over the air (OTA) updates.
4. UART configuration menu (115200 baud) stored in non-volatile memory (remembered across reboots).
5. Optional JTAG debugging circuitry (electrically isolated pinout).

# User Manual
### Quickstart Guide
In order to start using your board, you must first configure it to connect to a wifi network and provide it with a TomTom API key to use. This is achieved through a 115200 baud UART menu that is accessed by the USB-C connector. The device will request various configuration settings and, unfortunately, you will not be able to see what you type until you press ENTER. Once configuration is complete the device will reset, then once the wifi indicator is on you may press the toggle button, which will cause an led refresh! To reaccess the configuration menu to change the API key or wifi network, hold the direction toggle button through a device reset initiated by the enable button. Note that the device does not start refreshing LEDs after a reboot until the toggle button is pressed, which is a design decision to stop excessive API queries when configuring and troubleshooting the device. Have fun!

### Board Anatomy
![fullboardv1rev1](https://github.com/user-attachments/assets/be1016b3-02a1-47e9-b4da-ed9a18e2df38)

In the lower left corner of the board is the control panel and above that is the direction panel. The three chips located throughout the board are LED matrix drivers which receive commands via I2C from the ESP32. The control panel contains the USB-C connector, which communicates with the ESP32 via UART at 115200 baud; the 'OTA' button, which initiates an over the air update; the 'EN' button, which reboots the ESP32; status LED indicators; and an optional JTAG pinout on the right. The direction panel contains the 'Toggle' button and LED indicators that show the current traffic direction being displayed — either northbound or southbound.

### Understanding Road LEDs
Each road LED on the board corresponds to two OpenLR road segments: one northbound, and one southbound. Some road segments cross the coordinates of multiple LEDs, which are all refreshed with a single API call. A green LED indicates the current segment speed is greater than or equal to 60mph, a blue (or yellow) LED indicates the segment is between 30 and 60mph, and a red LED indicates the segment is below 30mph. Note that these speed indications are based on absolute speed and not speed relative to the road segment speed limit or typical flow speed, although this may be changed in future updates. Finally, LEDs at intersections of freeways always prioritize showing the segment speed of the freeway flowing most north or south.

### LED Indicator States
The board can be in one of the following states, discussed further below:
- Initialization complete: The wifi LED is on and the traffic direction LEDs are off.
- Configuration change is requested: The error LED is on and the traffic direction LEDs are flashing.
- An OTA update is in progress: The wifi LED is on and the traffic direction LEDs are on.
- A TomTom error has occurred: The error LED is flashing.
- An unrecoverable error has occurred: The error LED is on and the traffic direction LEDs are not flashing.

### Configuration Changes
Upon a reset via the 'EN' button, the ESP32 typically connects to the wifi network and indicates it has connected by turning on the wifi status LED. When it is unable to connect to the wifi network, it requests a configuration change from the user via UART at 115200 baud through the USB-C connector. This request is indicated by the error LED turning on and the traffic direction LEDs flashing. A configuration request can be forced by clicking the 'EN' button while holding the 'Toggle' button. The 'Toggle' button should be held until the board indicates that it is requesting a configuration change.

### OTA Updates
At any point after the wifi indicator is on, an OTA update may be initiated by clicking the 'OTA' button. An OTA update is underway if the wifi LED is on and the traffic direction LEDs are all solidly on; be sure to let the device finish and reenter the 'initialization complete' state. The ESP32 requests its updates from the latest firmware release, which is stored at https://jdbaptista.github.io/Personal-Website/trafficpcb/firmware.bin. This default can be changed to another URL by configuring it in ESP-IDF's 'menuconfig', then rebuilding and manually flashing the custom firmware. Note that subsequent firmware updates will need to be built manually in the same way to reflect the new URL.

### TomTom Errors
If you use an invalid TomTom API key, or your key is out of uses, then the error led will flash. When this happens, either fix your key or change the key via a configuration change, discussed above. The device requires a manual reboot via the 'EN' pin in the case of TomTom errors (or any errors). Note that key issues are the most likely cause of this kind of error, however other things may cause them such as TomTom being down or even a wifi error. If you are certain your key is correct, then the logs may hold some guidance as to what the issue is.

### Unrecoverable Errors
In the case that the firmware encounters an unrecoverable error, such as a memory allocation error, the error LED will turn on. These errors will not automatically cause a system restart because the error logs should be checked (via UART at 115200 baud through the USB-C connector) and posted as an issue to the repo. Once the logs are retrieved, simply reboot the system via the 'EN' button and forget anything ever happened :).

### JTAG Access
To enable debugging of the ESP32, the JTAG interface can be accessed through the pinout to the right of the control panel. The board contains JTAG circuitry that handles ESP32 strapping pins and electrically isolates the pinout to the rest of the board with op-amps acting as unity gain buffers. By default, JTAG circuitry is completely disconnected from the rest of the board (even ground and power) and can be connected by soldering breadboarding wires between the connection points near the JTAG pinout.

### Versioning
In the 'releases' tab of the repository you can find various firmware binaries that can manually be flashed onto the device in case something goes wrong with OTA updates. The versioning system is built such that updates should always select the most up to date version for the particular version and board revision the firmware is running on. Releases are handled manually, so there may be an issue where the ESP32 starts selecting firmware updates for a different version of the device. In this case, manually flash the image that corresponds to V[board version]\_[revision]\_[firmware_version]. This will get OTA updates back on the correct track. Note that if the server on which early firmware versions look for OTA updates is down and has been superseded by another server, then firmware must be flashed manually.

The current version of firmware running on the board can be viewed at boot through the UART logs. The letters on the end of the version denote some build configurations:
- U: undefined, meaning the programmer forgot to specify this in the version string.
- A/F: specifies whether data is coming from the API or is generated as fake data.
- R/B/G/Y: specifies a color (R: red) (B: blue) (G: green) (Y: yellow).

Ordering of configuration in the version string is as follows: [A/F: data source][R/B/G/Y: slow led speed][R/B/G/Y: medium led speed][R/B/G/Y: fast led speed]. Example: V1_2_5ARBG denotes the 5th firmware version for board version 1 revision 2.

### Pricing
TomTom's current API price is $0.75 per 1000 requests. An LED refresh in the North direction requires 266 requests and the South direction requires 275 requests (some LEDs correspond to the same openLR road segment). Therefore, these require just about $0.20 to $0.21 cents per refresh. An LED refresh period of 5 minutes will cost roughly $57 to $60 per day, 15 minutes at $20 per day, 20 minutes at $15, 30 minutes at $10, 60 minutes at $5. The user should be aware that V1_0_1 and earlier refresh data every time the direction button is pressed, resulting in many API calls that are only bounded by the user's lack of enthusiasm for button pressing.

# Contribution Guide
**Under Construction**

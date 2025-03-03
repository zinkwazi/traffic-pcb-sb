# Traffic PCB SB
This project is a decorative and interactive display piece that shows the current traffic in the greater Los Angeles area. It uses an ESP32 to connect to [TomTom's](https://www.tomtom.com/) traffic flow API, which supplies real-time traffic data of OpenLR road segments. Each LED corresponds to an OpenLR road segment and is queried from TomTom both periodically every hour and when the user requests a direction change. Please note that LED refreshes are requeried and not buffered, so repetitive changes in direction will quickly rack up API calls (slightly less than 326 per refresh)! That said, please enjoy the pretty and informative lights!

# Features
1. Real-time traffic updates of nearly 650 data points requested from TomTom (approximately 326 in each direction).
3. Over the air (OTA) updates.
4. UART configuration menu (115200 baud) stored in non-volatile memory (remembered across reboots).
5. Operation in no-WiFi environments with the most recently collected data displayed.

# User Manual
### Quickstart Guide
In order to start using your board, you must first configure it to connect to a wifi network. This is achieved through a 115200 baud UART menu exposed via the USB-C connector. This menu is accessed by holding the "Toggle" button (until the menu is opened) and pressing the "EN" or "Restart" button. The device will request various configuration settings and, unfortunately, you will not be able to see what you type until you press ENTER. Once configuration is complete, the device will reset. Then, the device will spend a short time initializing and attempting to connect to the network with the credentials provided. If it connects successfully, or it has previously connected to a network, the LEDs will refresh! Pressing the "Toggle" button after this will cause the direction of traffic currently being displayed to switch and the LEDs to refresh. Pressing the "Update" button on version 2 of the device—or "IO0" on version 1—will initiate an OTA update, discussed further below. If something goes wrong, the red error LED will light up and you should reboot the device with the "Restart" button. Have fun!

### Board Anatomy
![fullboardv1rev1](https://github.com/user-attachments/assets/be1016b3-02a1-47e9-b4da-ed9a18e2df38)

In the lower left corner of the board is the control panel and above that is the direction panel. The three chips located throughout the board are LED matrix drivers which receive commands via I2C from the ESP32. The control panel contains the USB-C connector, which communicates with the ESP32 via UART at 115200 baud; the 'OTA' button, which initiates an over the air update; the 'EN' button, which reboots the ESP32; status LED indicators; and an optional JTAG pinout on the right. The direction panel contains the 'Toggle' button and LED indicators that show the current traffic direction being displayed — either northbound or southbound.

### Understanding Road LEDs
Each road LED on the board corresponds to two OpenLR road segments: one northbound, and one southbound. Some road segments cross the coordinates of multiple LEDs, which are all refreshed with a single API call. Version 2 of the device features configurable LED colors, where the legend of colors is located just below the direction indicator. Road segments are classified as fast (always blue or green in version 1) when the segment speed is at least 80% of its normal speed, medium (orange or yellow in version 1) when between 50% and 80%, and slow (red in version 1) when below 50% of its normal speed. Finally, LEDs at intersections of freeways always prioritize showing the segment speed of the freeway oriented most vertically.

### LED Indicator States
The board can be in one of the following states, discussed further below:
- Initialization: The device has just been powered on or rebooted, and is not yet in another state. This may take longer than expected if there is trouble connecting to the wifi network.
- Configuration change is requested: The error LED is on and the traffic direction LEDs are flashing.
- An OTA update is in progress: The wifi LED is on and the traffic direction LEDs are on. On version 1 of the device, an update may still be occurring if the user interacts with the board during an update.
- A server error has occurred: The error LED is flashing. This indicates that the data displayed is not current, however the error will go away if the device is able to collect new data from the server.
- A fatal error has occurred: The error LED is on and the traffic direction LEDs are not flashing. The device must be manually rebooted after the logs are retrieved.

### Configuration Changes
The user can change which wifi network the device attempts to connect to through a 115200 baud UART menu exposed via the USB-C connector. This menu is accessed by holding the "Toggle" button (until the menu is opened) and pressing the "EN" (on version 1) or "Restart" (on version 2) button. The menu is active when the error LED is on and the traffic LEDs are flashing. If you are having trouble accessing the menu, please be sure to hold the "Toggle" button until these indicators are active. The device will request various configuration settings and, unfortunately, you will not be able to see what you type until you press ENTER. Once configuration is complete, the device will reset. A known bug is that the device cannot connect to some networks with particular SSIDs (names); For example, it will not connect to a network named "Jaden's IPhone", however it will connect to "JadenIPhone".

### OTA Updates
At any point after the wifi indicator is on, an OTA update may be initiated by clicking the 'OTA' button. An OTA update is underway if the wifi LED is on and the traffic direction LEDs are all solidly on; be sure to let the device finish and reenter the 'initialization complete' state. The ESP32 requests its updates from the latest firmware release, stored at bearanvil.com/firmware/firmwareV[board_version]_[revision].bin. This default can be changed to another URL by configuring it in ESP-IDF's 'menuconfig', then rebuilding and manually flashing the custom firmware. Note that subsequent firmware updates will need to be built manually in the same way to reflect the new URL.

### Server Errors
Server errors indicate that live data could not be retrieved from the server and the data currently displayed is the most recently stored data. The device will always try to retrieve data from the server before an LED refresh, and in the case that it is able to, the server error will go away and the error LED will no longer flash. Server errros are overwritten by fatal errors, discussed below.

### Unrecoverable Errors
In the case that the firmware encounters an unrecoverable error, such as a memory allocation error, the error LED will turn on. These errors will not automatically cause a system restart because the error logs should be checked (via UART at 115200 baud through the USB-C connector) and posted as an issue to the repo. Once the logs are retrieved, simply reboot the system via the 'EN' button and forget anything ever happened :).

### Versioning
In the 'releases' tab of the repository you can find various firmware binaries that can manually be flashed onto the device in case something goes wrong with OTA updates. The versioning system is built such that updates should always select the most up to date version for the particular version and board revision the firmware is running on. Releases are handled manually, so there may be an issue where the ESP32 starts selecting firmware updates for a different version of the device. In this case, manually flash the image that corresponds to V[board version]\_[revision]\_[firmware_version]. This will get OTA updates back on the correct track. Note that if the server on which early firmware versions look for OTA updates is down and has been superseded by another server, then firmware must be flashed manually.

The current version of firmware running on the board can be viewed at boot through the UART logs. The letters on the end of the version denote some build configurations:
- U: undefined, meaning the programmer forgot to specify this in the version string.
- A/F: specifies whether data is coming from the API or is generated as fake data.
- R/B/G/Y: specifies a color (R: red) (B: blue) (G: green) (Y: yellow).

Ordering of configuration in the version string is as follows: [A/F: data source][R/B/G/Y: slow led speed][R/B/G/Y: medium led speed][R/B/G/Y: fast led speed]. Example: V1_2_5ARBG denotes the 5th firmware version for board version 1 revision 2.

# Contribution Guide
**Under Construction**

# Traffic PCB SB
This project is a decorative and interactive display piece that shows the current traffic in the greater Los Angeles area. It sources data from [TomTom's](https://www.tomtom.com) traffic APIs, which supplies real-time traffic data. Each LED corresponds to an OpenLR road segment and data is queried from TomTom every hour. The way data is retrieved is likely to improve in the future. The device uses an ESP32S3 to control LEDs and refreshes the display every 5 minutes, during which LED colors will update if a change in road speed occurs. Please
reference the User Guide below to understand how to configure the device and interpret indicator lights. Enjoy!

# Features
1. Real-time traffic updates of 403 LEDs and nearly 800 data points.
2. Over the air (OTA) updates with daily firmware availability checks.
3. UART configuration menu (115200 baud) stored in non-volatile memory (remembered across reboots).
4. Offline operation with the most recently collected data displayed.

# User Manual
### Quickstart Guide
In order to start using your board, you must first configure it to connect to a wifi network. This is achieved through a 115200 baud UART menu exposed via the USB-C connector. Once connecting your USB-C cable to your computer, a COM device should be discoverable and communication via UART can be opened in the Arduino IDE or another program. The menu should open automatically and prompt you for wifi credentials (note that backspace is not accepted). If it does not open automatically, the menu is accessable by holding the "Toggle" button (until the menu is opened) and pressing the "Restart" button. Once configuration is complete, the device will reset. Then, the device will spend some time loading and collecting road segment data. It will refresh the LEDs once complete, after which pressing the "Toggle" button will change the direction of traffic flow being displayed from the Southeast direction to the Northwest direction. If a firmware update is available, with newer features and bug fixes, the OTA indicator light will strobe green. Pressing the "Update" button will download the update and the light will turn solid green if successful, otherwise it will turn solid
red if unsucessful.

### Board Anatomy
![fullboardv1rev1](https://github.com/user-attachments/assets/be1016b3-02a1-47e9-b4da-ed9a18e2df38)

In the lower left corner of the board is the control panel and above that is the direction panel. The three chips located throughout the board are LED matrix drivers which receive commands via I2C from the ESP32S3. The control panel contains the USB-C connector, which communicates with the ESP32S3 via UART at 115200 baud; the 'Update' button, which initiates an over the air update; the 'Restart' button, which reboots the ESP32S3; and status LED indicators. The direction panel contains the 'Toggle' button and LED indicators that show the current traffic direction being displayed — either Southeast or Northwest.

### Understanding Road LEDs
Each road LED on the board corresponds to two OpenLR road segments: one northbound, and one southbound. Some road segments cross the coordinates of multiple LEDs, which are all refreshed with a single API call. Version 2 of the device features configurable LED colors, where the legend of colors is located just below the direction panel. Road segments are classified as fast (always blue or green in version 1) when the segment speed is at least 80% of its normal speed, medium (orange or yellow in version 1) when between 50% and 80%, and slow (red in version 1) when below 50% of its normal speed. Finally, LEDs at intersections of freeways always prioritize showing the segment speed of the freeway oriented most vertically.

### LED Indicator States
The board can be in one of the following states, discussed further below:
- Initialization: The traffic direction LEDs are strobing in a loading circle pattern. The device has just been powered on or rebooted, and is initializing the device. This may take longer than expected if there is trouble connecting to the wifi network.
- Configuration change is requested: The error LED is on and the traffic direction LEDs are flashing.
- An OTA update is available: The OTA LED is strobing green. Pressing the "Update" button will update the firmware to the latest version if connected to wifi.
- An OTA update is in progress: The OTA LED is solid white. After updating, the LED will momentarily turn either green or red to indicate update success or failure.
- A server error has occurred: The error LED is flashing. This indicates that data retrieval has failed, and the device may not be displaying live data anymore.
- A fatal error has occurred: The error LED momentarily turns red, and the device resets. This can happen for a variety of reasons. If this occurs often, an issue can be created in this repository and you will be helped in a timely manner.

### Configuration Changes
The user can change which wifi network the device attempts to connect to through a 115200 baud UART menu exposed via the USB-C connector. This menu is accessed by holding the "Toggle" button (until the menu is opened) and pressing the "Restart" button. The menu is active when the error LED is on and the traffic LEDs are flashing. If you are having trouble accessing the menu, please be sure to hold the "Toggle" button until these indicators are active. The device will request various configuration settings and, unfortunately, you will not be able to see what you type until you press ENTER. Once configuration is complete, the device will reset. A known bug is that the device cannot connect to some networks with particular SSIDs (names); For example, it will not connect to a network named "Jaden's IPhone", however it will connect to "JadenIPhone".

### Versioning
In the 'releases' tab of the repository you can find various firmware binaries that can manually be flashed onto the device in case something goes wrong with OTA updates, or you would prefer a previous device version. Please note that patch versions, v0.0.X, are mandatory and the device will automatically update to the latest known patch version if it is still available on the server. For example, if the latest available version on the server is v0.7.3 and you manually flash v0.7.1 onto the device, the device will automatically update to v0.7.3. However, if you flash v0.6.0, then the device will not update to any v0.6.X or v0.7.X manually.

Hardware versions are denoted: V[hardware_version]_[hardware_revision]. The hardware version and revision of your board is indicated just above the control panel.
Firmware versions are denoted: v[major_version].[minor_version].[patch_version]. Patch updates are mandatory, but major/minor version updates will be indicated as available.

# Contribution Guide

### Flashing Firmware Guide (preliminary)
1. download vscode
2. on the lefthand side, open extensions tab (four boxes button)
3. search for ESP-IDF from espressif and download
4. Open setup wizard (alternatively cmd+shift+p, then ">ESP-IDF: Setup extension")
5. clone repo
6. open esp-firmware folder and use setup wizard again if necessary, use v5.4.x or v5.3.x.
7. wait for download to complete, follow terminal instructions if errors. This is very likely to be a problematic step,
and unfortunately will not allow the next steps to work if it fails. If
there are issues, follow [ESP-IDF Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) instructions. Again,
this is highly likely to be very problematic and will likely require a lot of troubleshooting. Navigating to user/esp/install.sh and user/esp/export.sh and running ./install.sh might fix it. Also, running
"sudo -H ./install.sh" after restarting the terminal on MAC may be helpful. I think that the v5.3.x script requires python < v3.8, however the tool downloads v3.9 automatically? I recommend trying v5.4.x
if this happens, but really this is an issue to ask esp-idf maintainers about how to solve.
9. if everthing is done, go to commands with (cmd+shift+p).
10. Execute ">ESP-IDF: Select Project Configuration" and choose developV2_0 if flashing for hardware version 2.
11. On the bottom blue bar, if "esp32" is shown as the device target, change it to "esp32s3" for hardware version 2. This step is important, as it creates the build directory and sdkconfig file.
12. click on the wrench icon in the bottom blue bar to build the project under the chosen configuration. Alternatively,
use command "ESP-IDF: Build your project". Wait for the build to complete. If there are errors, contact Jaden or fix them.
13. Plug in board to usb port and hold "Update" through a click of "Restart". Continue holding update until a new COM port
is discovered in the device manager. Holding "Update" through a restart boots the ESP32 into USB flashing mode.
11. Click the plug symbol on the blue bar to select the correct COM port, or execute ">ESP-IDF: Select Port" and 
select the correct port for the connected board.
12. Click the star symbol, or execute ">ESP-IDF: Select flash method" and choose UART.
13. Click either the lightning bolt symbol or flame symbol to either flash the device or flash the device and then monitor
the device logs, respectively. Alternatively, use the corresponding commands: "ESP-IDF: Flash your project" and
"ESP-IDF: Build, Flash, and Monitor".
14. Monitor the output, which should display the firmware and hardware version in red at some point. If the hardware version
is not V2_0, then you have flashed V1_0 firmware to the device and need to select the develop_V2_0 configuration with
"ESP-IDF: Select project configuration". If the firmware version is not v0.5.x, then an issue exists, which is likely that
the default build configurations have not made it into the build. Contact Jaden for help.
15. If the device is asking for wifi settings, type them in carefully (you currently cannot use backspace), however don't
worry if the credentials are incorrect as you can change them later. The device will reset.
16. Give the device some time to perform initialization and grab initial data. Watch the logs for any errors, or watch
the error LED, which will blink or be solid red if something went wrong, such as the wifi credentials being incorrect.
17. To change the wifi credentials again, hold "Toggle" through a reset and be very patient because, for some yet unknown
reason, the device has a crazy boot time. You will boot into the wifi configuration menu where you can update the wifi
credentials.
18. Congrats! Things should be working. If not, contact Jaden.

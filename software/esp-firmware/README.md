This is firmware written under the ESP-IDF SDK (v5.3.1) for ESP32 microcontrollers. The easiest way to get started with ESP-IDF is to download the ESP-IDF extension in Visual Studio Code, which will be discussed below.

### Useful Resources
- [ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html#manual-installation) 
  
  (Manual installation is easiest)
- [ESP-IDF Build System Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html)
- [ESP-IDF Extension Documentation](https://github.com/espressif/vscode-esp-idf-extension/blob/HEAD/docs/ONBOARDING.md)
- [FreeRTOS Book](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book)
  
  (ESP-IDF runs on top of FreeRTOS by default)

### Helpful Tidbits
ESP-IDF projects have a nice build system that is not very friendly. Instead of having 'include' and 'src' folders, ESP-IDF has components which are linked to the project via the CMake variable `COMPONENT_DIRS`, which specifies where the build system should search for components. By default it includes `PROJECT_DIR/components` ([See More](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html#optional-project-variables)). Each component has its own CMake file, which should essentially just include `idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS ".")`. This line tells the build system that anything using the component should search the entire directory for source and header files.

ESP-IDF uses its own command line for some reason, which should have been installed with the rest of ESP-IDF, and will show up next to the terminal in VSCode (although it won't be that useful with the extension).

If things are going wrong, you may be missing environment variables. These are the environment variables I am currently using: `IDF_PATH : C:\Users\name\esp\v5.3\esp-idf` and `IDF_TOOLS_PATH : C:\Users\name\.espressif`.

To get Intellisense for the code, you will have to download the C/C++ extension and run the command (run a command with `CTRL+SHIFT+P`) `>C/C++: Edit Configurations (UI)`. This will create `.vscode\c_cpp_properties.json`, which you should make contain roughly the following:

```json
{
    "configurations": [
        {
            "name": "ESP-IDF",
            "compilerPath": "${config:idf.toolsPathWin}\\tools\\xtensa-esp-elf\\esp-13.2.0_20240530\\xtensa-esp-elf\\bin\\xtensa-esp32-elf-gcc.exe",
            "compileCommands": "${config:idf.buildPath}/compile_commands.json",
            "includePath": [
                "${config:idf.espIdfPath}/components/**",
                "${config:idf.espIdfPathWin}/components/**",
                "${workspaceFolder}/**",
                "${workspaceFolder}/main/include"
            ],
            "browse": {
                "path": [
                    "${config:idf.espIdfPath}/components",
                    "${config:idf.espIdfPathWin}/components",
                    "${workspaceFolder}"
                ],
                "limitSymbolsToIncludedHeaders": true
            }
        }
    ],
    "version": 4
}
```

I refrain from adding this to Git because I'm not entirely sure if everthing is the same on different systems/computers. If this is really not working for you, then you can use the extension to create a new project which should automatically generate the correct file ([guide](https://github.com/espressif/vscode-esp-idf-extension/blob/43bb2c09ae9a9890de9ff7e6b65cfea1e297de7f/docs/tutorial/new_project_wizard.md)). Be sure to use the `hello-world` example project as the `sample-project` does not actually come with this file for some reason.

Another helpful command is `>ESP-IDF: SDK Configuration Editor (Menuconfig)`, which allows the user to change build settings and defaults for peripherals.

Finally, use the tools in the blue bar at the bottom of the screen to build (wrench), flash (lightning), and monitor (tv screen) the output of a connected esp32. Alternatively, do all of these steps at once with the flame.
## generate_led_coordinates.py

**Assists in the generation of C arrays which describe the x, y PCB coordinates
of each LED on a version of the board.**

**Input:** A CSV file containing the CPL data of every designator on the board. This
should include at least the following headers: "Designator", "Mid X", "Mid Y".
The script will ignore all designators not of the form "LEDx", where x is a 
unique integer.

**Example Input:**

```
Designator,Val,Package,Mid X,Mid Y,Rotation,Layer
F1,3A,Fuse_1206_3216Metric,9.271,-182.499,-90,top
F2,3A,Fuse_1206_3216Metric,58,-163.5,180,top
J2,USB C,USB_C_Receptacle_HRO_TYPE-C-31-M-12,4,-177.8,-90,top
LED1,LED_ARGB,LED_RGB_0606,5.0292,-27.0256,0,top
LED2,LED_ARGB,LED_RGB_0606,39.7256,-29.972,0,top
LED6,LED_ARGB,LED_RGB_0606,197.9168,-86.5124,0,top
LED5,LED_ARGB,LED_RGB_0606,201.1172,-88.8492,0,top
LED4,LED_ARGB,LED_RGB_0606,204.8256,-90.4748,0,top
```

**Output:** A text file containing the C array describing the PCB's LED x, y coordinates,
where each index 'i' in the array corresponds to LED 'i'.. A comment is created 
alongside any indices that did not have a corresponding entry in the input file. 
Fractional portions of the input coordinates are rounded to the nearest integer.

**Example Output:**

```
/* A mapping from LED number to corresponding
x, y PCB coordinates. */
static const LEDCoord LEDNumToCoord[] = {
    {0, 0}, // there is no LED with number 0
    {5, -27}, // 1
    {40, -30}, // 2
    {0, 0}, // there is no LED with number 3
    {205, -90}, // 4
    {201, -89}, // 5
};
```

**Example Explanation:** Input designators F1, F2, and J2 are ignored and do
not affect the output. Similarly, the order of designators "LEDx" is ignored.
Index 2 of the output contains the "Mid X" and "Mid Y" respectively, rounded
to the nearest integer. There is no "LED3" designator, so index 3 of the output
is set to {0, 0} and a comment is created documenting absence.


**Usage:** 
```
python3 generate_led_coordinates.py inputCPL.csv [output.txt]
```

If no output file is provided, the script will present the generated struct
to stdout. 
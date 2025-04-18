/**
 * V1_0_led_registers.c
 * 
 * Cointains LED number to matrix register mappings for V1_0 of the hardware.
 */

#include "sdkconfig.h"
 
#if CONFIG_HARDWARE_VERSION == 1

#include "led_registers.h"

#include <stdint.h>

#include "led_types.h"

/* A mapping from LED number to corresponding 
matrix registers for each color (red, green, blue),
where index (i - 1) corresponds to LED number i. */
const LEDReg LEDNumToReg[MAX_NUM_LEDS_REG] = {
    {0x8D, 0x8F, 0x8E, MAT1_PAGE1}, // 1, start matrix 1
    {0x96, 0x98, 0x97, MAT1_PAGE1}, // 2
    {0x9F, 0xA1, 0xA0, MAT1_PAGE1}, // 3
    {0xA8, 0xAA, 0xA9, MAT1_PAGE1}, // 4
    {0x84, 0x86, 0x85, MAT1_PAGE1}, // 5
    {0x7B, 0x7D, 0x7C, MAT1_PAGE1}, // 6
    {0x72, 0x74, 0x73, MAT1_PAGE1}, // 7
    {0x69, 0x6B, 0x6A, MAT1_PAGE1}, // 8
    {0x60, 0x62, 0x61, MAT1_PAGE1}, // 9
    {0x8A, 0x8C, 0x8B, MAT1_PAGE1}, // 10
    {0x93, 0x95, 0x94, MAT1_PAGE1}, // 11
    {0x9C, 0x9E, 0x9D, MAT1_PAGE1}, // 12
    {0xA5, 0xA7, 0xA6, MAT1_PAGE1}, // 13
    {0x81, 0x83, 0x82, MAT1_PAGE1}, // 14
    {0x78, 0x7A, 0x79, MAT1_PAGE1}, // 15
    {0x6F, 0x71, 0x70, MAT1_PAGE1}, // 16
    {0x66, 0x68, 0x67, MAT1_PAGE1}, // 17
    {0x5D, 0x5F, 0x5E, MAT1_PAGE1}, // 18
    {0x87, 0x88, 0x89, MAT1_PAGE1}, // 19
    {0x90, 0x91, 0x92, MAT1_PAGE1}, // 20
    {0x99, 0x9A, 0x9B, MAT1_PAGE1}, // 21
    {0xA2, 0xA3, 0xA4, MAT1_PAGE1}, // 22
    {0x7E, 0x7F, 0x80, MAT1_PAGE1}, // 23
    {0x75, 0x76, 0x77, MAT1_PAGE1}, // 24
    {0x6C, 0x6D, 0x6E, MAT1_PAGE1}, // 25
    {0x63, 0x64, 0x65, MAT1_PAGE1}, // 26
    {0x5A, 0x5B, 0x5C, MAT1_PAGE1}, // 27
    {0xB1, 0xB2, 0xB3, MAT1_PAGE0}, // 28
    {0x1B, 0x1C, 0x1D, MAT1_PAGE1}, // 29
    {0x39, 0x3A, 0x3B, MAT1_PAGE1}, // 30
    {0x57, 0x58, 0x59, MAT1_PAGE1}, // 31
    {0x93, 0x94, 0x95, MAT1_PAGE0}, // 32
    {0x75, 0x76, 0x77, MAT1_PAGE0}, // 33
    {0x57, 0x58, 0x59, MAT1_PAGE0}, // 34
    {0x39, 0x3A, 0x3B, MAT1_PAGE0}, // 35
    {0x1B, 0x1C, 0x1D, MAT1_PAGE0}, // 36
    {0xAE, 0xAF, 0xB0, MAT1_PAGE0}, // 37
    {0x18, 0x19, 0x1A, MAT1_PAGE1}, // 38
    {0x36, 0x37, 0x38, MAT1_PAGE1}, // 39
    {0x54, 0x55, 0x56, MAT1_PAGE1}, // 40
    {0x90, 0x91, 0x92, MAT1_PAGE0}, // 41
    {0x72, 0x73, 0x74, MAT1_PAGE0}, // 42
    {0x54, 0x55, 0x56, MAT1_PAGE0}, // 43
    {0x36, 0x37, 0x38, MAT1_PAGE0}, // 44
    {0x18, 0x19, 0x1A, MAT1_PAGE0}, // 45
    {0xAB, 0xAC, 0xAD, MAT1_PAGE0}, // 46
    {0x15, 0x16, 0x17, MAT1_PAGE1}, // 47
    {0x33, 0x34, 0x35, MAT1_PAGE1}, // 48
    {0x51, 0x52, 0x53, MAT1_PAGE1}, // 49
    {0x8D, 0x8E, 0x8F, MAT1_PAGE0}, // 50
    {0x6F, 0x70, 0x71, MAT1_PAGE0}, // 51
    {0x51, 0x52, 0x53, MAT1_PAGE0}, // 52
    {0x33, 0x34, 0x35, MAT1_PAGE0}, // 53
    {0x15, 0x16, 0x17, MAT1_PAGE0}, // 54
    {0xA8, 0xAA, 0xA9, MAT1_PAGE0}, // 55
    {0x12, 0x14, 0x13, MAT1_PAGE1}, // 56
    {0x30, 0x32, 0x31, MAT1_PAGE1}, // 57
    {0x4E, 0x50, 0x4F, MAT1_PAGE1}, // 58
    {0x8A, 0x8C, 0x8B, MAT1_PAGE0}, // 59
    {0x6C, 0x6E, 0x6D, MAT1_PAGE0}, // 60
    {0x4E, 0x50, 0x4F, MAT1_PAGE0}, // 61
    {0x30, 0x32, 0x31, MAT1_PAGE0}, // 62
    {0x12, 0x14, 0x13, MAT1_PAGE0}, // 63
    {0xA2, 0xA3, 0xA4, MAT1_PAGE0}, // 64
    {0x0C, 0x0D, 0x0E, MAT1_PAGE1}, // 65
    {0x2A, 0x2B, 0x2C, MAT1_PAGE1}, // 66
    {0x48, 0x49, 0x4A, MAT1_PAGE1}, // 67
    {0x84, 0x85, 0x86, MAT1_PAGE0}, // 68
    {0x66, 0x67, 0x68, MAT1_PAGE0}, // 69
    {0x48, 0x49, 0x4A, MAT1_PAGE0}, // 70
    {0x2A, 0x2B, 0x2C, MAT1_PAGE0}, // 71
    {0x0C, 0x0D, 0x0E, MAT1_PAGE0}, // 72
    {0xA5, 0xA6, 0xA7, MAT1_PAGE0}, // 73
    {0x0F, 0x10, 0x11, MAT1_PAGE1}, // 74
    {0x2D, 0x2E, 0x2F, MAT1_PAGE1}, // 75
    {0x4B, 0x4C, 0x4D, MAT1_PAGE1}, // 76
    {0x87, 0x88, 0x89, MAT1_PAGE0}, // 77
    {0x69, 0x6A, 0x6B, MAT1_PAGE0}, // 78
    {0x4B, 0x4C, 0x4D, MAT1_PAGE0}, // 79
    {0x2D, 0x2E, 0x2F, MAT1_PAGE0}, // 80
    {0x0F, 0x10, 0x11, MAT1_PAGE0}, // 81
    {0x99, 0x9A, 0x9B, MAT1_PAGE0}, // 82
    {0x03, 0x04, 0x05, MAT1_PAGE1}, // 83
    {0x21, 0x22, 0x23, MAT1_PAGE1}, // 84
    {0x3F, 0x40, 0x41, MAT1_PAGE1}, // 85
    {0x7B, 0x7C, 0x7D, MAT1_PAGE0}, // 86
    {0x5D, 0x5E, 0x5F, MAT1_PAGE0}, // 87
    {0x3F, 0x40, 0x41, MAT1_PAGE0}, // 88
    {0x21, 0x22, 0x23, MAT1_PAGE0}, // 89
    {0x03, 0x04, 0x05, MAT1_PAGE0}, // 90
    {0x9C, 0x9D, 0x9E, MAT1_PAGE0}, // 91
    {0x06, 0x07, 0x08, MAT1_PAGE1}, // 92
    {0x24, 0x25, 0x26, MAT1_PAGE1}, // 93
    {0x42, 0x43, 0x44, MAT1_PAGE1}, // 94
    {0x7E, 0x7F, 0x80, MAT1_PAGE0}, // 95
    {0x60, 0x61, 0x62, MAT1_PAGE0}, // 96
    {0x42, 0x43, 0x44, MAT1_PAGE0}, // 97
    {0x24, 0x25, 0x26, MAT1_PAGE0}, // 98
    {0x06, 0x07, 0x08, MAT1_PAGE0}, // 99
    {0x9F, 0xA0, 0xA1, MAT1_PAGE0}, // 100
    {0x09, 0x0A, 0x0B, MAT1_PAGE1}, // 101
    {0x27, 0x28, 0x29, MAT1_PAGE1}, // 102
    {0x45, 0x46, 0x47, MAT1_PAGE1}, // 103
    {0x81, 0x82, 0x83, MAT1_PAGE0}, // 104
    {0x63, 0x64, 0x65, MAT1_PAGE0}, // 105
    {0x45, 0x46, 0x47, MAT1_PAGE0}, // 106
    {0x27, 0x28, 0x29, MAT1_PAGE0}, // 107
    {0x09, 0x0A, 0x0B, MAT1_PAGE0}, // 108
    {0x96, 0x97, 0x98, MAT1_PAGE0}, // 109
    {0x00, 0x01, 0x02, MAT1_PAGE1}, // 110
    {0x1E, 0x1F, 0x20, MAT1_PAGE1}, // 111
    {0x3C, 0x3D, 0x3E, MAT1_PAGE1}, // 112
    {0x78, 0x79, 0x7A, MAT1_PAGE0}, // 113
    {0x5A, 0x5B, 0x5C, MAT1_PAGE0}, // 114
    {0x3C, 0x3D, 0x3E, MAT1_PAGE0}, // 115
    {0x1E, 0x1F, 0x20, MAT1_PAGE0}, // 116
    {0x00, 0x01, 0x02, MAT1_PAGE0}, // 117, end matrix 1
    {0xA2, 0xA3, 0xA4, MAT2_PAGE1}, // 118, begin matrix 2
    {0x87, 0x88, 0x89, MAT2_PAGE1}, // 119
    {0x7E, 0x7F, 0x80, MAT2_PAGE1}, // 120
    {0x75, 0x76, 0x77, MAT2_PAGE1}, // 121
    {0x6C, 0x6D, 0x6E, MAT2_PAGE1}, // 122
    {0x63, 0x64, 0x65, MAT2_PAGE1}, // 123
    {0x5A, 0x5B, 0x5C, MAT2_PAGE1}, // 124
    {0x90, 0x91, 0x92, MAT2_PAGE1}, // 125
    {0x99, 0x9A, 0x9B, MAT2_PAGE1}, // 126
    {0x51, 0x52, 0x53, MAT2_PAGE1}, // 127
    {0xAB, 0xAC, 0xAD, MAT2_PAGE0}, // 128
    {0x8D, 0x8E, 0x8F, MAT2_PAGE0}, // 129
    {0x6F, 0x70, 0x71, MAT2_PAGE0}, // 130
    {0x51, 0x52, 0x53, MAT2_PAGE0}, // 131
    {0x33, 0x34, 0x35, MAT2_PAGE0}, // 132
    {0x15, 0x16, 0x17, MAT2_PAGE0}, // 133
    {0x15, 0x16, 0x17, MAT2_PAGE1}, // 134
    {0x33, 0x34, 0x35, MAT2_PAGE1}, // 135
    {0x57, 0x58, 0x59, MAT2_PAGE1}, // 136
    {0xB1, 0xB2, 0xB3, MAT2_PAGE0}, // 137
    {0x93, 0x94, 0x95, MAT2_PAGE0}, // 138
    {0x75, 0x76, 0x77, MAT2_PAGE0}, // 139
    {0x57, 0x58, 0x59, MAT2_PAGE0}, // 140
    {0x39, 0x3A, 0x3B, MAT2_PAGE0}, // 141
    {0x1B, 0x1C, 0x1D, MAT2_PAGE0}, // 142
    {0x1B, 0x1C, 0x1D, MAT2_PAGE1}, // 143
    {0x39, 0x3A, 0x3B, MAT2_PAGE1}, // 144
    {0xA8, 0xA9, 0xAA, MAT2_PAGE1}, // 145
    {0x8D, 0x8E, 0x8F, MAT2_PAGE1}, // 146
    {0x84, 0x85, 0x86, MAT2_PAGE1}, // 147
    {0x7B, 0x7C, 0x7D, MAT2_PAGE1}, // 148
    {0x72, 0x73, 0x74, MAT2_PAGE1}, // 149
    {0x69, 0x6A, 0x6B, MAT2_PAGE1}, // 150
    {0x60, 0x61, 0x62, MAT2_PAGE1}, // 151
    {0x96, 0x97, 0x98, MAT2_PAGE1}, // 152
    {0x9F, 0xA0, 0xA1, MAT2_PAGE1}, // 153
    {0xA5, 0xA6, 0xA7, MAT2_PAGE1}, // 154
    {0x8A, 0x8B, 0x8C, MAT2_PAGE1}, // 155
    {0x81, 0x82, 0x83, MAT2_PAGE1}, // 156
    {0x78, 0x79, 0x7A, MAT2_PAGE1}, // 157
    {0x6F, 0x70, 0x71, MAT2_PAGE1}, // 158
    {0x66, 0x67, 0x68, MAT2_PAGE1}, // 159
    {0x5D, 0x5E, 0x5F, MAT2_PAGE1}, // 160
    {0x93, 0x94, 0x95, MAT2_PAGE1}, // 161
    {0x9C, 0x9D, 0x9E, MAT2_PAGE1}, // 162
    {0x54, 0x55, 0x56, MAT2_PAGE1}, // 163
    {0xAE, 0xAF, 0xB0, MAT2_PAGE0}, // 164
    {0x90, 0x91, 0x92, MAT2_PAGE0}, // 165
    {0x72, 0x73, 0x74, MAT2_PAGE0}, // 166
    {0x54, 0x55, 0x56, MAT2_PAGE0}, // 167
    {0x36, 0x37, 0x38, MAT2_PAGE0}, // 168
    {0x18, 0x19, 0x1A, MAT2_PAGE0}, // 169
    {0x18, 0x19, 0x1A, MAT2_PAGE1}, // 170
    {0x36, 0x37, 0x38, MAT2_PAGE1}, // 171
    {0x4E, 0x4F, 0x50, MAT2_PAGE1}, // 172
    {0xA8, 0xA9, 0xAA, MAT2_PAGE0}, // 173
    {0x8A, 0x8B, 0x8C, MAT2_PAGE0}, // 174
    {0x6C, 0x6D, 0x6E, MAT2_PAGE0}, // 175
    {0x4E, 0x4F, 0x50, MAT2_PAGE0}, // 176
    {0x30, 0x31, 0x32, MAT2_PAGE0}, // 177
    {0x12, 0x13, 0x14, MAT2_PAGE0}, // 178
    {0x12, 0x13, 0x14, MAT2_PAGE1}, // 179
    {0x30, 0x31, 0x32, MAT2_PAGE1}, // 180
    {0x4B, 0x4C, 0x4D, MAT2_PAGE1}, // 181
    {0xA5, 0xA6, 0xA7, MAT2_PAGE0}, // 182
    {0x87, 0x88, 0x89, MAT2_PAGE0}, // 183
    {0x69, 0x6A, 0x6B, MAT2_PAGE0}, // 184
    {0x4B, 0x4C, 0x4D, MAT2_PAGE0}, // 185
    {0x2D, 0x2E, 0x2F, MAT2_PAGE0}, // 186
    {0x0F, 0x10, 0x11, MAT2_PAGE0}, // 187
    {0x0F, 0x10, 0x11, MAT2_PAGE1}, // 188
    {0x2D, 0x2E, 0x2F, MAT2_PAGE1}, // 189
    {0x45, 0x46, 0x47, MAT2_PAGE1}, // 190
    {0x9F, 0xA0, 0xA1, MAT2_PAGE0}, // 191
    {0x81, 0x82, 0x83, MAT2_PAGE0}, // 192
    {0x63, 0x64, 0x65, MAT2_PAGE0}, // 193
    {0x45, 0x46, 0x47, MAT2_PAGE0}, // 194
    {0x27, 0x28, 0x29, MAT2_PAGE0}, // 195
    {0x09, 0x0A, 0x0B, MAT2_PAGE0}, // 196
    {0x09, 0x0A, 0x0B, MAT2_PAGE1}, // 197
    {0x27, 0x28, 0x29, MAT2_PAGE1}, // 198
    {0x3F, 0x40, 0x41, MAT2_PAGE1}, // 199
    {0x99, 0x9A, 0x9B, MAT2_PAGE0}, // 200
    {0x7B, 0x7C, 0x7D, MAT2_PAGE0}, // 201
    {0x5D, 0x5E, 0x5F, MAT2_PAGE0}, // 202
    {0x3F, 0x40, 0x41, MAT2_PAGE0}, // 203
    {0x21, 0x22, 0x23, MAT2_PAGE0}, // 204
    {0x03, 0x04, 0x05, MAT2_PAGE0}, // 205
    {0x03, 0x04, 0x05, MAT2_PAGE1}, // 206
    {0x21, 0x22, 0x23, MAT2_PAGE1}, // 207
    {0x48, 0x49, 0x4A, MAT2_PAGE1}, // 208
    {0xA2, 0xA3, 0xA4, MAT2_PAGE0}, // 209
    {0x84, 0x85, 0x86, MAT2_PAGE0}, // 210
    {0x66, 0x67, 0x68, MAT2_PAGE0}, // 211
    {0x48, 0x49, 0x4A, MAT2_PAGE0}, // 212
    {0x2A, 0x2B, 0x2C, MAT2_PAGE0}, // 213
    {0x0C, 0x0D, 0x0E, MAT2_PAGE0}, // 214
    {0x0C, 0x0D, 0x0E, MAT2_PAGE1}, // 215
    {0x2A, 0x2B, 0x2C, MAT2_PAGE1}, // 216
    {0x44, 0x42, 0x43, MAT2_PAGE1}, // 217
    {0x9E, 0x9C, 0x9D, MAT2_PAGE0}, // 218
    {0x80, 0x7E, 0x7F, MAT2_PAGE0}, // 219
    {0x62, 0x60, 0x61, MAT2_PAGE0}, // 220
    {0x44, 0x42, 0x43, MAT2_PAGE0}, // 221
    {0x26, 0x24, 0x25, MAT2_PAGE0}, // 222
    {0x08, 0x06, 0x07, MAT2_PAGE0}, // 223
    {0x08, 0x06, 0x07, MAT2_PAGE1}, // 224
    {0x26, 0x24, 0x25, MAT2_PAGE1}, // 225
    {0x3C, 0x3D, 0x3E, MAT2_PAGE1}, // 226
    {0x96, 0x97, 0x98, MAT2_PAGE0}, // 227
    {0x78, 0x79, 0x7A, MAT2_PAGE0}, // 228
    {0x5A, 0x5B, 0x5C, MAT2_PAGE0}, // 229
    {0x3C, 0x3D, 0x3E, MAT2_PAGE0}, // 230
    {0x1E, 0x1F, 0x20, MAT2_PAGE0}, // 231
    {0x00, 0x01, 0x02, MAT2_PAGE0}, // 232
    {0x00, 0x01, 0x02, MAT2_PAGE1}, // 233
    {0x1E, 0x1F, 0x20, MAT2_PAGE1}, // 234, end matrix 2
    {0x3E, 0x3D, 0x3C, MAT3_PAGE1}, // 235, begin matrix 3
    {0x02, 0x01, 0x00, MAT3_PAGE0}, // 236
    {0x20, 0x1F, 0x1E, MAT3_PAGE0}, // 237
    {0x3E, 0x3D, 0x3C, MAT3_PAGE0}, // 238
    {0x5C, 0x5B, 0x5A, MAT3_PAGE0}, // 239
    {0x7A, 0x79, 0x78, MAT3_PAGE0}, // 240
    {0x98, 0x97, 0x96, MAT3_PAGE0}, // 241
    {0x02, 0x01, 0x00, MAT3_PAGE1}, // 242
    {0x20, 0x1F, 0x1E, MAT3_PAGE1}, // 243
    {0x41, 0x40, 0x3F, MAT3_PAGE1}, // 244
    {0x05, 0x04, 0x03, MAT3_PAGE0}, // 245
    {0x23, 0x22, 0x21, MAT3_PAGE0}, // 246
    {0x41, 0x40, 0x3F, MAT3_PAGE0}, // 247
    {0x5F, 0x5E, 0x5D, MAT3_PAGE0}, // 248
    {0x7D, 0x7C, 0x7B, MAT3_PAGE0}, // 249
    {0x9B, 0x9A, 0x99, MAT3_PAGE0}, // 250
    {0x05, 0x04, 0x03, MAT3_PAGE1}, // 251
    {0x23, 0x22, 0x21, MAT3_PAGE1}, // 252
    {0x44, 0x43, 0x42, MAT3_PAGE1}, // 253
    {0x08, 0x07, 0x06, MAT3_PAGE0}, // 254
    {0x26, 0x25, 0x24, MAT3_PAGE0}, // 255
    {0x44, 0x43, 0x42, MAT3_PAGE0}, // 256
    {0x62, 0x61, 0x60, MAT3_PAGE0}, // 257
    {0x80, 0x7F, 0x7E, MAT3_PAGE0}, // 258
    {0x9E, 0x9D, 0x9C, MAT3_PAGE0}, // 259
    {0x08, 0x07, 0x06, MAT3_PAGE1}, // 260
    {0x26, 0x25, 0x24, MAT3_PAGE1}, // 261
    {0x47, 0x46, 0x45, MAT3_PAGE1}, // 262
    {0x0B, 0x0A, 0x09, MAT3_PAGE0}, // 263
    {0x29, 0x28, 0x27, MAT3_PAGE0}, // 264
    {0x47, 0x46, 0x45, MAT3_PAGE0}, // 265
    {0x65, 0x64, 0x63, MAT3_PAGE0}, // 266
    {0x83, 0x82, 0x81, MAT3_PAGE0}, // 267
    {0xA1, 0xA0, 0x9F, MAT3_PAGE0}, // 268
    {0x0B, 0x0A, 0x09, MAT3_PAGE1}, // 269
    {0x29, 0x28, 0x27, MAT3_PAGE1}, // 270
    {0x4A, 0x49, 0x48, MAT3_PAGE1}, // 271
    {0x0E, 0x0D, 0x0C, MAT3_PAGE0}, // 272
    {0x2C, 0x2B, 0x2A, MAT3_PAGE0}, // 273
    {0x4A, 0x49, 0x48, MAT3_PAGE0}, // 274
    {0x68, 0x67, 0x66, MAT3_PAGE0}, // 275
    {0x86, 0x85, 0x84, MAT3_PAGE0}, // 276
    {0xA4, 0xA3, 0xA2, MAT3_PAGE0}, // 277
    {0x0E, 0x0D, 0x0C, MAT3_PAGE1}, // 278
    {0x2C, 0x2B, 0x2A, MAT3_PAGE1}, // 279
    {0x50, 0x4F, 0x4E, MAT3_PAGE1}, // 280
    {0x14, 0x13, 0x12, MAT3_PAGE0}, // 281
    {0x32, 0x31, 0x30, MAT3_PAGE0}, // 282
    {0x50, 0x4F, 0x4E, MAT3_PAGE0}, // 283
    {0x6E, 0x6D, 0x6C, MAT3_PAGE0}, // 284
    {0x8C, 0x8B, 0x8A, MAT3_PAGE0}, // 285
    {0xAA, 0xA9, 0xA8, MAT3_PAGE0}, // 286
    {0x14, 0x13, 0x12, MAT3_PAGE1}, // 287
    {0x32, 0x31, 0x30, MAT3_PAGE1}, // 288
    {0x53, 0x52, 0x51, MAT3_PAGE1}, // 289
    {0x17, 0x16, 0x15, MAT3_PAGE0}, // 290
    {0x35, 0x34, 0x33, MAT3_PAGE0}, // 291
    {0x53, 0x52, 0x51, MAT3_PAGE0}, // 292
    {0x71, 0x70, 0x6F, MAT3_PAGE0}, // 293
    {0x8F, 0x8E, 0x8D, MAT3_PAGE0}, // 294
    {0xAD, 0xAC, 0xAB, MAT3_PAGE0}, // 295
    {0x17, 0x16, 0x15, MAT3_PAGE1}, // 296
    {0x35, 0x34, 0x33, MAT3_PAGE1}, // 297
    {0x4D, 0x4C, 0x4B, MAT3_PAGE1}, // 298
    {0x11, 0x10, 0x0F, MAT3_PAGE0}, // 299
    {0x2F, 0x2E, 0x2D, MAT3_PAGE0}, // 300
    {0x4D, 0x4C, 0x4B, MAT3_PAGE0}, // 301
    {0x6B, 0x6A, 0x69, MAT3_PAGE0}, // 302
    {0x89, 0x88, 0x87, MAT3_PAGE0}, // 303
    {0xA7, 0xA6, 0xA5, MAT3_PAGE0}, // 304
    {0x11, 0x10, 0x0F, MAT3_PAGE1}, // 305
    {0x2F, 0x2E, 0x2D, MAT3_PAGE1}, // 306
    {0xA4, 0xA3, 0xA2, MAT3_PAGE1}, // 307
    {0x5C, 0x5B, 0x5A, MAT3_PAGE1}, // 308
    {0x65, 0x64, 0x63, MAT3_PAGE1}, // 309
    {0x6E, 0x6D, 0x6C, MAT3_PAGE1}, // 310
    {0x77, 0x76, 0x75, MAT3_PAGE1}, // 311
    {0x80, 0x7F, 0x7E, MAT3_PAGE1}, // 312
    {0x89, 0x88, 0x87, MAT3_PAGE1}, // 313
    {0x92, 0x91, 0x90, MAT3_PAGE1}, // 314
    {0x9B, 0x9A, 0x99, MAT3_PAGE1}, // 315
    {0x57, 0x58, 0x59, MAT3_PAGE1}, // 316
    {0x1B, 0x1C, 0x1D, MAT3_PAGE0}, // 317
    {0x39, 0x3A, 0x3B, MAT3_PAGE0}, // 318
    {0x57, 0x58, 0x59, MAT3_PAGE0}, // 319
    {0x75, 0x76, 0x77, MAT3_PAGE0}, // 320
    {0x93, 0x94, 0x95, MAT3_PAGE0}, // 321
    {0xB1, 0xB2, 0xB3, MAT3_PAGE0}, // 322
    {0x1B, 0x1C, 0x1D, MAT3_PAGE1}, // 323
    {0x39, 0x3A, 0x3B, MAT3_PAGE1}, // 324
    {0x74, 0x73, 0x72, MAT3_PAGE0}, // 329 mapped to 325
    {0x92, 0x91, 0x90, MAT3_PAGE0}, // 330 mapped to 326, end matrix 3
};

#endif /* CONFIG_HARDWARE_VERSION == 1 */
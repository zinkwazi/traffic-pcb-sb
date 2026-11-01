/**
 * refresh_types.h
 * 
 * Created On: 7/1/26
 * Author: Jaden Baptista
 * 
 * Contains types and definitions common
 * to many files in the refresh component.
 */

#ifndef INC_REFRESH_TYPES_H_
#define INC_REFRESH_TYPES_H_

/**
 * The maximum size of a frame of LEDs that can be displayed.
 * 
 * This is greater than the total number of LEDs on the board
 * so that the animation can be complex.
 */
#define MAX_FRAME_SIZE                  (512)

typedef struct LEDSpeed {
    /* The Kicad LED number this speed corresponds to */
    uint16_t ledNum; 
    /* The speed of the road segment of the LED. UINT8_MAX means 
     * that this LED is invalid and should always be skipped */
    uint8_t speed; 
} LEDSpeed;

#endif /* INC_REFRESH_TYPES_H_ */
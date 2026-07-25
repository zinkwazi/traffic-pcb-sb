/**
 * refresh_fsm.h
 * 
 * Contains a finite state machine
 * run by the refresh task to handle
 * commands and update LEDs.
 */

#ifndef REFRESH_FSM_H_
#define REFRESH_FSM_H_

#include <stdbool.h>
#include <stdint.h>

#include "main_types.h"
#include "led_types.h"

typedef struct LEDSpeed {
    uint16_t ledNum; /* The Kicad LED number this speed corresponds to */
    uint8_t speed; /* The speed of the road segment of the LED */
} LEDSpeed;

/**
 * Action the FSM instructs to take.
 * Using actions instead of raw function
 * calls in the FSM allows unit testing 
 * logic without mocking or dependency
 * injection.
 */
typedef enum {
    REFRESH_ACTION_NONE,
    /* set an LED to the provided color */
    REFRESH_ACTION_SET,
    /* clear the output of an LED */
    REFRESH_ACTION_CLEAR,
    /* clear the output of a range of LEDs */
    REFRESH_ACTION_CLEAR_RANGE,
} RefreshFSMAction;

typedef struct RefreshFSMOutput {
    /**
     * Whether the FSM is idle, indicating that
     * the FSM can stop being ticked until a new
     * command needs to be sent to it.
     */
    bool isIdle;
    /**
     * The frame index of the old frame that was
     * that should be released. UINT32_MAX if there
     * is not a frame to release.
     */
    uint32_t releaseOldFrameNdx;
    /**
     * The typical north frame index of the old typical
     * frame that should be released. UINT32_MAX if
     * there is not a frame to release.
     */
    uint32_t releaseOldTypicalNorthFrameNdx;
    /**
     * The typical south frame index of the old typical
     * frame that should be released. UINT32_MAX if
     * there is not a frame to release.
     */
    uint32_t releaseOldTypicalSouthFrameNdx;
    /**
     * The frame index of the queued frame that should
     * be released. UINT32_MAX if there is not a frame
     * to release.
     */
    uint32_t releaseQueuedFrameNdx;
    /**
     * The typical frame index of the queued typical
     * frame that should be released. UINT32_MAX if
     * there is not a frame to release.
     */
    uint32_t releaseQueuedTypicalFrameNdx;
    /**
     * The action the refresh task should take
     * based on FSM logic.
     */
    RefreshFSMAction action;
    union {
        struct {
            uint16_t ledNum;
            Color color;
        } set;
        struct {
            uint32_t ledNum;
        } clear;
        struct {
            uint32_t startLedNum;
        } clearRange;
    } actionParams;
} RefreshFSMOutput;

typedef enum {
    /**
     * Install the provided LED frame to the board
     * in the provided direction. If LEDs are off, this
     * command starts instantly. If a frame is already
     * on the board, this command slowly clears the board
     * before starting to install the new frame. If a frame
     * is already being cleared, this command quickly clears
     * the board.
     */
    REFRESH_CMD_NEW_FRAME,
    /**
     * Update the typical LED speeds for the given
     * direction used by the refresh FSM.
     */
    REFRESH_CMD_UPDATE_TYPICAL,
    /**
     * Turn on night mode. This will cause the board
     * to be cleared the the previously stored direction.
     */
    REFRESH_CMD_NIGHT_MODE_ON,
    /**
     * Turn off night mode. This will cause the most
     * recently received frame to be installed to the board.
     */
    REFRESH_CMD_NIGHT_MODE_OFF,
    /**
     * no command.
     */
    REFRESH_CMD_NONE,
} RefreshFSMCommandType;

typedef struct {
    RefreshFSMCommandType type;
    /* The ledFrames index where this command's LED frame is stored. NUM_LED_FRAMES if unused. */
    uint32_t frameNdx;
    /* The length of this command's LED frame. UINT32_MAX if unused. */
    uint32_t frameLen;
    /* The animation and display direction. NO_DIR if unused. */
    Direction dir;
} RefreshFSMCommand;

/**
 * Resources the refresh FSM needs to
 * do its job properly. These must exist
 * for the duration of use of the FSM.
 */
typedef struct {
    /* the LED num to matrix register lookup table to determine if an LED is valid */
    LEDReg *LEDNumToReg;
    /* the length of LEDNumToReg */
    uint32_t LEDNumToRegLen;
    /* the frame array where input frames will be placed for the FSM */
    LEDSpeed **LEDFrames;
    /* the length of LEDFrames */
    uint32_t LEDFramesLen;
    /* the LED color to display if an LED is considered slow */
    Color slowLEDColor;
    /* the LED color to display if an LED is considered medium */
    Color mediumLEDColor;
    /* the LED color to display if an LED is considered fast */
    Color fastLEDColor;
} RefreshFSMResources;

void refreshFSMInit(RefreshFSMResources *resources);
RefreshFSMOutput refreshFSMTick(RefreshFSMCommand *cmd);

#endif /* REFRESH_FSM_H_ */
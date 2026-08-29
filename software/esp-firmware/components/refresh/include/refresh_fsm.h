/**
 * refresh_fsm.h
 * 
 * Contains a finite state machine
 * run by the refresh task to handle
 * commands to update LEDs.
 */

#ifndef REFRESH_FSM_H_
#define REFRESH_FSM_H_

#include <stdbool.h>
#include <stdint.h>

#include "main_types.h"
#include "led_types.h"

#include "refresh_types.h"

typedef enum {
    /* Installs the provided LED frame to the board in the provided direction */
    REFRESH_CMD_NEW_FRAME,
    /* Clears then reinstalls the current LED frame, if any */
    REFRESH_CMD_REFRESH,
    /* Updates the typical LED speeds for the given direction */
    REFRESH_CMD_UPDATE_TYPICAL,
    /* Turns on night mode, during which LEDs will be turned off */
    REFRESH_CMD_NIGHT_MODE_ON,
    /* Turns off night mode, allowing the current LED frame to be displayed */
    REFRESH_CMD_NIGHT_MODE_OFF,
    REFRESH_CMD_NONE,
} RefreshFSMCommandType;

typedef struct {
    RefreshFSMCommandType type;
    /* The ledFrames index where this command's LED frame is stored. Ownership is given to the FSM */
    uint32_t frameNdx;
    /* The length of this command's LED frame stored at frameNdx in ledFrames */
    uint32_t frameLen;
    /* The animation and display direction */
    Direction dir;
} RefreshFSMCommand;

/**
 * Action the FSM instructs to take.
 * Using actions instead of raw function
 * calls in the FSM allows unit testing 
 * logic without mocking or dependency
 * injection.
 */
typedef enum {
    REFRESH_ACTION_SET, /* set an LED to the provided color */
    REFRESH_ACTION_CLEAR, /* clear the output of an LED */
    REFRESH_ACTION_CLEAR_RANGE, /* clear the output of a range of LEDs */
    REFRESH_ACTION_NONE,
} RefreshFSMActionType;

typedef struct RefreshFSMAction {
    RefreshFSMActionType type;
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
    };
} RefreshFSMAction;

/**
 * The type of frame release occurring by the refresh FSM.
 */
typedef enum {
    REFRESH_FSM_FRAME_RELEASE_STANDARD,
    REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH,
    REFRESH_FSM_FRAME_RELEASE_TYPICAL_SOUTH,
    REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD,
    REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL,
    REFRESH_FSM_FRAME_RELEASE_TYPES_COUNT,
} RefreshFSMFrameReleaseType;

/**
 * A frame being released by the refresh FSM, which returns
 * ownership to user code.
 */
typedef struct RefreshFSMFrameRelease {
    uint32_t index;
    RefreshFSMFrameReleaseType type;
} RefreshFSMFrameRelease;

typedef struct RefreshFSMFrameReleaseList {
    RefreshFSMFrameRelease list[REFRESH_FSM_FRAME_RELEASE_TYPES_COUNT];
    uint32_t len;
} RefreshFSMFrameReleaseList;

typedef struct RefreshFSMOutput {
    /* Whether user code can stop ticking the FSM until a new command needs to be processed */
    bool isIdle;
    /* The frames the FSM is releasing back to user code ownership */
    RefreshFSMFrameReleaseList framesToRelease;
    /* The action user code should take based on FSM logic */
    RefreshFSMAction action;
} RefreshFSMOutput;

/**
 * Resources the refresh FSM needs to do its job. These 
 * must exist for the duration of use of the FSM.
 */
typedef struct {
    /* the LED num to matrix register lookup table to determine if an LED is valid */
    const LEDReg *LEDNumToReg;
    /* the length of LEDNumToReg */
    uint32_t LEDNumToRegLen;
    /* the frame array where input frames will be placed for the FSM */
    LEDSpeed (*LEDFrames)[MAX_FRAME_SIZE];
    /* the length of LEDFrames */
    uint32_t LEDFramesLen;
    /* the LED color to display if an LED is considered slow */
    Color slowLEDColor;
    /* the LED color to display if an LED is considered medium */
    Color mediumLEDColor;
    /* the LED color to display if an LED is considered fast */
    Color fastLEDColor;
} RefreshFSMResources;

void refreshFSMInit(const RefreshFSMResources *resources);
RefreshFSMOutput refreshFSMTick(const RefreshFSMCommand *cmd);

#endif /* REFRESH_FSM_H_ */
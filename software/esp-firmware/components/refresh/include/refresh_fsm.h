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
    /**
     * Updates the typical LED speeds for the given direction, taking
     * effect the next time a frame is installed. Unlike NEW_FRAME, the
     * frame is a lookup table indexed by LED number, not an ordered
     * list: frame[n].ledNum must equal n and frame[n].speed is the
     * typical speed of LED n (slot 0 is unused). Every slot up to
     * MAX_NUM_LEDS_REG must be written, otherwise an assertion fails
     * when an LED with a mismatched slot is installed. A typical speed
     * of 0 is treated as 1. frameLen should be MAX_FRAME_SIZE and must
     * be nonzero. The FSM leaves WAITING_FOR_FRAMES only once both
     * directions have a typical frame.
     */
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
    /**
     * The location to set true once the command has been taken by the
     * FSM and any frame it displaced has been released to the frame pool.
     * Set by the task running the FSM, not the FSM itself, since the
     * release happens after the tick. Waiting on this before queueing
     * another command of the same type keeps reserved frames from piling
     * up in the command queue. Set to NULL if unused.
     */
    volatile bool *received;
    /**
     * The location to set true once the frame on the board when this
     * command arrived has been cleared (immediately if the board is
     * already dark). Only honored for REFRESH_CMD_NEW_FRAME, and the FSM
     * never writes false, so user code must reset it before queueing.
     * If a later NEW_FRAME supersedes this one before it is installed,
     * this is still set once the board is cleared. Set to NULL if unused.
     */
    volatile bool *cleared;
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
        /* REFRESH_ACTION_SET: set ledNum to color */
        struct set {
            uint16_t ledNum;
            Color color;
        } set;
        /* REFRESH_ACTION_CLEAR: turn off ledNum */
        struct clear {
            uint32_t ledNum;
        } clear;
        /**
         * REFRESH_ACTION_CLEAR_RANGE: turn off every LED still lit
         * on the board in the reverse direction of the current frame,
         * starting from startNdx to 0. This should complete in a
         * single tick before the frame is released if requested.
         */
        struct clearRange {
            uint32_t frameNdx;
            uint32_t startNdx;
            uint32_t typicalFrameNdx; // used to identify LEDs to skip
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
    /* The frames the FSM is releasing back to user code ownership after the requested action */
    RefreshFSMFrameReleaseList framesToRelease;
    /* The action user code should take based on FSM logic */
    RefreshFSMAction action;
    /**
     * The direction indicator state user code should display, kept in
     * sync with the frame on the board: the direction of the frame being
     * installed, shown, or cleared, or NO_DIR (indicators off) while the
     * board is dark. Only acted on when update is true, which happens
     * on the tick the displayed direction changes. User code should
     * perform this after the action, as both may be requested in the
     * same tick (e.g. a final clear of the old frame followed by the
     * start of a new frame in a different direction).
     */
    struct {
        bool update;
        Direction dir;
    } indicator;
} RefreshFSMOutput;

/**
 * Resources the refresh FSM needs to do its job. These 
 * must exist for the duration of use of the FSM.
 */
typedef struct {
    /* the LED num to matrix register lookup table to determine if an LED is valid.
     * Indexed directly by LED number: LEDNumToReg[n] describes LED n. Note this
     * differs from led_registers.h's global LEDNumToReg, where index i describes
     * LED i + 1 (refresh_task.c builds a shifted copy for the FSM). */
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
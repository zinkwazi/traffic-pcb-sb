/**
 * refresh_fsm.c
 * 
 * Contains an FSM that handles requests for LED frames
 * to be installed or cleared from the board.
 */

#include "refresh_fsm.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"

#include "app_err.h"
#include "esp_err.h"
#include "led_matrix.h"
#include "led_types.h"
#include "led_registers.h"
#include "main_types.h"
#include "refresh_task.h"

#define TAG "refresh_fsm"

typedef enum {
    /* the FSM is waiting for current frames, displaying a loading animation */
    REFRESH_FSM_WAITING_FOR_FRAMES,
    /* the FSM is installing the current frame */
    REFRESH_FSM_INSTALLING_FRAME,
    /* the FSM is clearing the previous frame */
    REFRESH_FSM_CLEARING_FRAME,
    /* the FSM is clearing the previous frame quickly (in one tick) */
    REFRESH_FSM_QUICK_CLEARING_FRAME,
    /* the board is cleared and ready to instantly install a frame */
    REFRESH_FSM_CLEARED,
    /* the board has a frame installed */
    REFRESH_FSM_FRAME_INSTALLED,
} RefreshFSMState;

typedef struct RefreshFSM {
    /* the frame index of the LED that was just updated */
    uint32_t currLEDNdx;
    /* the frame index of the current LED frame being installed. UINT32_MAX if none. */
    uint32_t currFrameNdx;
    /* the number of items in LEDFrames[currFrameNdx] */
    uint32_t currFrameLen;
    /* the frame index of the next LED frame. This frame is the one to be installed next. UINT32_MAX if none. */
    uint32_t nextFrameNdx;
    /* the number of items in LEDFrames[nextFrameNdx] */
    uint32_t nextFrameLen;
    /* the frame index of the current north typical LED speeds frame, which must be max length. UINT32_MAX if none. */
    uint32_t currTypicalNorthFrameNdx;
    /* the frame index of the next north typical LED speeds frame, which must be max length. UINT32_MAX if none. */
    uint32_t nextTypicalNorthFrameNdx;
    /* the frame index of the current south typical LED speeds frame, which must be max length. UINT32_MAX if none. */
    uint32_t currTypicalSouthFrameNdx;
    /* the frame index of the next south typical LED speeds frame, which must be max length. UINT32_MAX if none. */
    uint32_t nextTypicalSouthFrameNdx;
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
    /* the state of the FSM */
    RefreshFSMState state;
    /* the current animation and display direction. Note that the clearing direction is the opposite */
    Direction currDir;
    /* the next animation and display direction. */
    Direction nextDir;
    /* whether night mode is currently active */
    bool nightMode;
} RefreshFSM;

static RefreshFSM fsm;

static void handleCommandNewFrame(RefreshFSMOutput *out, uint32_t frameNdx, uint32_t frameLen, Direction dir);
static void handleCommandUpdateTypical(RefreshFSMOutput *out, uint32_t frameNdx, Direction dir);
static void handleCommandNightModeOff(RefreshFSMOutput *out);
static void handleCommandNightModeOn(void);
static void handleStateWaitingForFrames(RefreshFSMOutput *out);
static void handleStateInstallingFrame(RefreshFSMOutput *out);
static void handleStateClearingFrame(RefreshFSMOutput *out);
static void handleStateQuickClearingFrame(RefreshFSMOutput *out);
static bool iterateFrame(RefreshFSMOutput *out, bool setLED);
static Color ledSpeedToColor(LEDSpeed speed, Direction dir);
static char *refreshFSMStateName(RefreshFSMState state);

/**
 * Initializes the refresh FSM, which handles requests
 * for LED frames to be installed or cleared from the board.
 * 
 * @param fsm The FSM to initialize.
 * @param resources Resources that the FSM will create
 * a shallow copy of.
 * 
 * @param LEDNumToReg The lookup table the FSM should use
 * to determine if an LED is valid.
 * @param LEDNumToRegLen The length of LEDNumToReg, defining
 * the highest (potentially) valid LED number.
 * 
 * @returns ESP_OK if successful.
 */
void refreshFSMInit(RefreshFSMResources *resources)
{
    assert(NULL != resources);
    assert(NULL != resources->LEDNumToReg);
    assert(NULL != resources->LEDFrames);

    fsm.currLEDNdx = UINT32_MAX;
    fsm.currFrameNdx = UINT32_MAX;
    fsm.currFrameLen = 0;
    fsm.nextFrameNdx = UINT32_MAX;
    fsm.nextFrameLen = 0;
    fsm.currTypicalNorthFrameNdx = UINT32_MAX;
    fsm.nextTypicalNorthFrameNdx = UINT32_MAX;
    fsm.currTypicalSouthFrameNdx = UINT32_MAX;
    fsm.nextTypicalSouthFrameNdx = UINT32_MAX;
    fsm.LEDNumToReg = resources->LEDNumToReg;
    fsm.LEDNumToRegLen = resources->LEDNumToRegLen;
    fsm.LEDFrames = resources->LEDFrames;
    fsm.LEDFramesLen = resources->LEDFramesLen;
    fsm.slowLEDColor = resources->slowLEDColor;
    fsm.mediumLEDColor = resources->mediumLEDColor;
    fsm.fastLEDColor = resources->fastLEDColor;
    fsm.state = REFRESH_FSM_WAITING_FOR_FRAMES;
    fsm.currDir = NO_DIR;
    fsm.nextDir = NO_DIR;
    fsm.nightMode = false;
}

/**
 * Advances the refresh FSM by one time period.
 * 
 * @pre Run on a consistent timebase (ex. 5Hz)
 * while a frame is being processed. This can
 * be ignored when the FSM is idle.
 * 
 * @param cmd The command to be handled by the FSM.
 * 
 * @returns Upstream output for communication with
 * the task running the FSM.
 */
RefreshFSMOutput refreshFSMTick(RefreshFSMCommand *cmd)
{
    assert(NULL != cmd);

    RefreshFSMOutput out = {
        .isIdle = false,
        .releaseOldFrameNdx = UINT32_MAX,
        .releaseOldTypicalNorthFrameNdx = UINT32_MAX,
        .releaseOldTypicalSouthFrameNdx = UINT32_MAX,
        .releaseQueuedFrameNdx = UINT32_MAX,
        .releaseQueuedTypicalFrameNdx = UINT32_MAX,
    };

    /* handle command */
    if (REFRESH_CMD_NONE != cmd->type)
    {
        ESP_LOGI(TAG, "Command Received: %d", cmd->type);
    }
    RefreshFSMState prevState = fsm.state;
    switch (cmd->type)
    {
        case REFRESH_CMD_NEW_FRAME:
            handleCommandNewFrame(&out, cmd->frameNdx, cmd->frameLen, cmd->dir);
            break;
        case REFRESH_CMD_UPDATE_TYPICAL:
            handleCommandUpdateTypical(&out, cmd->frameNdx, cmd->dir);
            break;
        case REFRESH_CMD_NIGHT_MODE_OFF:
            handleCommandNightModeOff(&out);
            break;
        case REFRESH_CMD_NIGHT_MODE_ON:
            handleCommandNightModeOn();
            break;
        case REFRESH_CMD_NONE:
            break;
        default:
            assert(0 && "refresh FSM command invalid");
            break;
    }

    /* tick FSM */
    if (prevState != fsm.state)
    {
        ESP_LOGI(TAG, "Transitioned from state %s to state %s", refreshFSMStateName(prevState), refreshFSMStateName(fsm.state));
    }
    switch (fsm.state)
    {
        case REFRESH_FSM_WAITING_FOR_FRAMES:
            handleStateWaitingForFrames(&out);
            break;
        case REFRESH_FSM_INSTALLING_FRAME:
            handleStateInstallingFrame(&out);
            break;
        case REFRESH_FSM_CLEARING_FRAME:
            handleStateClearingFrame(&out);
            break;
        case REFRESH_FSM_QUICK_CLEARING_FRAME:
            handleStateQuickClearingFrame(&out);
            break;
        case REFRESH_FSM_CLEARED:
            out.isIdle = true;
            break;
        case REFRESH_FSM_FRAME_INSTALLED:
            out.isIdle = true;
            break;
        default:
            assert(0 && "refresh FSM state invalid");
            break;
    }

    return out;
}

/**
 * Transitions the FSM to the REFRESH_FSM_INSTALLING_FRAME
 * state. This is effectively an entry callback.
 */
static void transitionToInstallingFrame(RefreshFSMOutput *out)
{
    /* latch new frames */
    if (UINT32_MAX != fsm.nextFrameNdx)
    {
        ESP_LOGI(TAG, "latching frameNdx from %lu to %lu", fsm.currFrameNdx, fsm.nextFrameNdx);
        out->releaseOldFrameNdx = fsm.currFrameNdx;
        fsm.currFrameNdx = fsm.nextFrameNdx;
        fsm.currFrameLen = fsm.nextFrameLen;
        fsm.currDir = fsm.nextDir;
        fsm.nextFrameNdx = UINT32_MAX;
    }

    if (UINT32_MAX != fsm.nextTypicalNorthFrameNdx)
    {
        ESP_LOGI(TAG, "latching typicalNorthNdx from %lu to %lu", fsm.currTypicalNorthFrameNdx, fsm.nextTypicalNorthFrameNdx);
        out->releaseOldTypicalNorthFrameNdx = fsm.currTypicalNorthFrameNdx;
        fsm.currTypicalNorthFrameNdx = fsm.nextTypicalNorthFrameNdx;
        fsm.nextTypicalNorthFrameNdx = UINT32_MAX;
    }
    
    if (UINT32_MAX != fsm.nextTypicalSouthFrameNdx)
    {
        ESP_LOGI(TAG, "latching typicalSouthNdx from %lu to %lu", fsm.currTypicalSouthFrameNdx, fsm.nextTypicalSouthFrameNdx);
        out->releaseOldTypicalSouthFrameNdx = fsm.currTypicalSouthFrameNdx;
        fsm.currTypicalSouthFrameNdx = fsm.nextTypicalSouthFrameNdx;
        fsm.nextTypicalSouthFrameNdx = UINT32_MAX;
    }

    /* start installing frame */
    fsm.currLEDNdx = 0;
    fsm.state = REFRESH_FSM_INSTALLING_FRAME;
}

/**
 * Checks whether it is valid to transition from the
 * REFRESH_FSM_WAITING_FOR_FRAMES state to the
 * REFRESH_FSM_INSTALLING_FRAME state and transitions if true.
 */
static void tryTransitionFromWaitingForFrames(RefreshFSMOutput *out)
{
    const bool haveBothTypicalFrames = (fsm.nextTypicalNorthFrameNdx != UINT32_MAX) &&
                                       (fsm.nextTypicalSouthFrameNdx != UINT32_MAX);
    if (haveBothTypicalFrames && fsm.nextFrameNdx != UINT32_MAX)
    {
        transitionToInstallingFrame(out);
    }
}

/**
 * Updates the FSM state to start installing
 * the new LED frame.
 * 
 * @note If LEDs are off, the new frame starts 
 * being installed instantly. If a frame is
 * already on the board, the board will start being
 * cleared. If the board is already being cleared,
 * the board will be cleared quickly.
 * 
 * @param frameNdx The index in LEDFrames of the new
 * typical speed frame.
 * @param frameLen The length of the frame indexed
 * by frameNdx in LEDFrames.
 * @param dir The traffic direction this frame
 * corresponds to.
 */
static void handleCommandNewFrame(RefreshFSMOutput *out, uint32_t frameNdx, uint32_t frameLen, Direction dir)
{
    /* buffer latest frame */
    out->releaseQueuedFrameNdx = fsm.nextFrameNdx;
    fsm.nextFrameNdx = frameNdx;
    fsm.nextFrameLen = frameLen;
    fsm.nextDir = dir;

    /* manage FSM state */
    switch (fsm.state)
    {
        case REFRESH_FSM_WAITING_FOR_FRAMES:
            tryTransitionFromWaitingForFrames(out);
            break;
        case REFRESH_FSM_INSTALLING_FRAME:
            fsm.state = REFRESH_FSM_CLEARING_FRAME;
            break;
        case REFRESH_FSM_CLEARING_FRAME:
            /* falls through */
        case REFRESH_FSM_QUICK_CLEARING_FRAME:
            fsm.state = REFRESH_FSM_QUICK_CLEARING_FRAME;
            break;
        case REFRESH_FSM_CLEARED:
            if (!fsm.nightMode)
            {
                transitionToInstallingFrame(out);
            }
            break;
        case REFRESH_FSM_FRAME_INSTALLED:
            fsm.state = REFRESH_FSM_CLEARING_FRAME;
            break;
    }
}

/**
 * Updates the FSM state to use new typical
 * speed data the next time an LED frame is
 * installed.
 * 
 * @param[out] releaseQueuedTypicalFrameNdx The index in
 * LEDFrames of the previously queued typical speed
 * frame that must be unreserved.
 * @param frameNdx The index in LEDFrames of the new
 * typical speed frame.
 * @param dir The traffic direction this typical
 * speed frame corresponds to.
 */
static void handleCommandUpdateTypical(RefreshFSMOutput *out, uint32_t frameNdx, Direction dir)
{
    /* update buffered typical direction */
    switch (dir)
    {
        case NORTH:
            ESP_LOGI(TAG, "buffered typicalNorthNdx %lu switched to ndx %lu", fsm.nextTypicalNorthFrameNdx, frameNdx);
            out->releaseQueuedTypicalFrameNdx = fsm.nextTypicalNorthFrameNdx;
            fsm.nextTypicalNorthFrameNdx = frameNdx;
            break;
        case SOUTH:
            ESP_LOGI(TAG, "buffered typicalSouthNdx %lu switched to ndx %lu", fsm.nextTypicalSouthFrameNdx, frameNdx);
            out->releaseQueuedTypicalFrameNdx = fsm.nextTypicalSouthFrameNdx;
            fsm.nextTypicalSouthFrameNdx = frameNdx;
            break;
        default:
            assert(0 && "encountered bad direction");
    }

    /* manage FSM state */
    switch (fsm.state)
    {
        case REFRESH_FSM_WAITING_FOR_FRAMES:
            tryTransitionFromWaitingForFrames(out);
            break;
        default:
            break;
    }
}

/**
 * Updates the FSM state to turn night mode off.
 * 
 * @param fsm The refresh FSM.
 */
static void handleCommandNightModeOff(RefreshFSMOutput *out)
{
    fsm.nightMode = false;
    switch (fsm.state)
    {
        case REFRESH_FSM_CLEARED:
            transitionToInstallingFrame(out);
            break;
        default:
            break;
    }
}

/**
 * Updates the FSM state to turn night mode on.
 * 
 * @param fsm The refresh FSM.
 */
static void handleCommandNightModeOn(void)
{
    fsm.nightMode = true;
    switch (fsm.state)
    {
        case REFRESH_FSM_FRAME_INSTALLED:
            fsm.state = REFRESH_FSM_CLEARING_FRAME;
            break;
        default:
            break;
    }
}

/**
 * Waits for both a current LED frame and a current
 * typical LED frame to be provided to the FSM before
 * transitioning to the INSTALLING_FRAME state.
 * 
 * @param fsm The refresh FSM.
 * @param[out] out Upstream output for communication
 * with the task running the FSM.
 */
static void handleStateWaitingForFrames(RefreshFSMOutput *out)
{
    out->isIdle = true;
    out->action = REFRESH_ACTION_NONE;
}

/**
 * Updates the color of a single LED each tick until
 * all LEDs of the frame are installed on the board.
 * 
 * @param fsm The refresh FSM.
 * @param[out] out Upstream output for communication
 * with the task running the FSM.
 */
static void handleStateInstallingFrame(RefreshFSMOutput *out)
{
    assert(NULL != out);

    const bool setLED = true;
    bool done = iterateFrame(out, setLED);

    if (done)
    {
        out->isIdle = true;
        fsm.state = REFRESH_FSM_FRAME_INSTALLED;
    }
}

/**
 * Turns off a single LED until all LEDs on the board
 * are cleared.
 * 
 * @param fsm The refresh FSM.
 * @param[out] out Upstream output for communication
 * with the task running the FSM.
 */
static void handleStateClearingFrame(RefreshFSMOutput *out)
{
    assert(NULL != out);

    const bool setLED = false;
    bool done = iterateFrame(out, setLED);

    if (done)
    {
        if (!fsm.nightMode)
        {
            out->isIdle = false;
            transitionToInstallingFrame(out);
        } else
        {
            out->isIdle = true;
            fsm.state = REFRESH_FSM_CLEARED;
        }
        return;
    }

    out->isIdle = false;
}

/**
 * Turns off all LEDs in sequence, without delays
 * between LEDs, until all LEDs on the board are cleared.
 * 
 * @note This state clears the board in a single tick.
 * 
 * @param fsm The refresh FSM.
 * @param[out] out Upstream output for communication
 * with the task running the FSM.
 */
static void handleStateQuickClearingFrame(RefreshFSMOutput *out)
{
    assert(NULL != out);

    out->action = REFRESH_ACTION_CLEAR_RANGE;
    out->actionParams.clearRange.startLedNum = fsm.currLEDNdx;

    out->releaseOldFrameNdx = fsm.currFrameNdx;
    fsm.currFrameNdx = UINT32_MAX;

    if (!fsm.nightMode)
    {
        out->isIdle = false;
        transitionToInstallingFrame(out);
    } else
    {
        out->isIdle = true;
        fsm.state = REFRESH_FSM_CLEARED;
    }
}

/**
 * Iterates through the LEDs of the current frame, either
 * setting the LED color or clearing the LED.
 * 
 * @param fsm The refresh FSM.
 * @param[out] out Upstream output for communication
 * with the task running the FSM.
 * @param setLED Whether to set or clear the LED.
 * 
 * @returns True if iteration is complete, false otherwise.
 */
static bool iterateFrame(RefreshFSMOutput *out, bool setLED)
{
    LEDSpeed speed;
    
    assert(NULL != out);

    /* find next LED */
    bool foundValidLED = false;
    for (; fsm.currLEDNdx < fsm.currFrameLen; fsm.currLEDNdx++)
    {
        speed = fsm.LEDFrames[fsm.currFrameNdx][fsm.currLEDNdx];
        if (speed.ledNum >= fsm.LEDNumToRegLen) continue;
        if (isLEDValid(fsm.LEDNumToReg[speed.ledNum]))
        {
            foundValidLED = true;
            break;
        }
    }

    /* generate output action */
    if (foundValidLED)
    {
        fsm.currLEDNdx++;
        if (setLED)
        {
            out->action = REFRESH_ACTION_SET;
            out->actionParams.set.ledNum = speed.ledNum;
            out->actionParams.set.color = ledSpeedToColor(speed, fsm.currDir);
        } else
        {
            out->action = REFRESH_ACTION_CLEAR;
            out->actionParams.clear.ledNum = speed.ledNum;
        }
    } else
    {
        out->action = REFRESH_ACTION_NONE;
    }

    return fsm.currLEDNdx >= fsm.currFrameLen;
}

/**
 * Converts the LED speed to LED color based on
 * the typical LED speed.
 * 
 * @param fsm The refresh FSM which contains a frame
 * with the current typical speeds.
 * @param speed The LED speed.
 * @param dir The traffic direction the speed corresponds to.
 * 
 * @returns The LED color corresponding to the speed.
 */
static Color ledSpeedToColor(LEDSpeed speed, Direction dir)
{
    Color ret;
    
    assert(fsm.currTypicalNorthFrameNdx < fsm.LEDFramesLen);
    assert(fsm.currTypicalSouthFrameNdx < fsm.LEDFramesLen);

    /* retrieve typical speed */
    uint32_t typicalFrameNdx = (NORTH == dir) ? fsm.currTypicalNorthFrameNdx : fsm.currTypicalSouthFrameNdx;
    LEDSpeed typicalSpeed = fsm.LEDFrames[typicalFrameNdx][speed.ledNum];
    assert(typicalSpeed.ledNum == speed.ledNum);
    if (0 == typicalSpeed.speed) typicalSpeed.speed = 1;

    /* determine color */
    float percentFlow = speed.speed / typicalSpeed.speed;
    if (percentFlow < CONFIG_SLOW_CUTOFF_PERCENT) {
        ret = fsm.slowLEDColor;
    } else if (percentFlow < CONFIG_MEDIUM_CUTOFF_PERCENT) {
        ret = fsm.mediumLEDColor;
    } else {
        ret = fsm.fastLEDColor;
    }

    return ret;
}

/**
 * @returns The state name of the refresh FSM state.
 */
static char *refreshFSMStateName(RefreshFSMState state)
{
    switch (state)
    {
        case REFRESH_FSM_WAITING_FOR_FRAMES: return "WAITING_FOR_FRAMES";
        case REFRESH_FSM_INSTALLING_FRAME: return "INSTALLING_FRAME";
        case REFRESH_FSM_CLEARING_FRAME: return "CLEARING_FRAME";
        case REFRESH_FSM_QUICK_CLEARING_FRAME: return "QUICK_CLEARING_FRAME";
        case REFRESH_FSM_CLEARED: return "CLEARED";
        case REFRESH_FSM_FRAME_INSTALLED: return "FRAME_INSTALLED";
    }
    return "UNKNOWN";
}
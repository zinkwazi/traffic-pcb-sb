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
#include "sdkconfig.h"

#include "app_err.h"
#include "esp_err.h"
#include "led_matrix.h"
#include "led_types.h"
#include "led_registers.h"
#include "main_types.h"
#include "refresh_task.h"

#include "refresh_types.h"

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
    REFRESH_FSM_STATE_COUNT,
} RefreshFSMState;

typedef struct RefreshFSM {
    /* one past the frame index of the LED just installed */
    uint32_t currLEDNdx;
    /* the frame index of the current LED frame being installed. UINT32_MAX if none */
    uint32_t currFrameNdx;
    uint32_t currFrameLen;
    /* the frame index of the next LED frame to be installed. UINT32_MAX if none */
    uint32_t nextFrameNdx;
    uint32_t nextFrameLen;
    /* the frame index of the current north typical LED speeds frame, which must be max length. UINT32_MAX if none */
    uint32_t currTypicalNorthFrameNdx;
    /* the frame index of the next north typical LED speeds frame, which must be max length. UINT32_MAX if none */
    uint32_t nextTypicalNorthFrameNdx;
    /* the frame index of the current south typical LED speeds frame, which must be max length. UINT32_MAX if none */
    uint32_t currTypicalSouthFrameNdx;
    /* the frame index of the next south typical LED speeds frame, which must be max length. UINT32_MAX if none */
    uint32_t nextTypicalSouthFrameNdx;
    /* the LED num to matrix register lookup table to determine if an LED is valid */
    const LEDReg *LEDNumToReg;
    /* the length of LEDNumToReg */
    uint32_t LEDNumToRegLen;
    /* the frame array where input frames will be placed for the FSM */
    LEDSpeed (*LEDFrames)[MAX_FRAME_SIZE];
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
    /* the next animation and display direction */
    Direction nextDir;
    /* whether night mode is currently active */
    bool nightMode;
} RefreshFSM;

static RefreshFSM fsm;

static void handleCommandNewFrame(RefreshFSMOutput *out, uint32_t frameNdx, uint32_t frameLen, Direction dir);
static void handleCommandRefresh(RefreshFSMOutput *out, Direction dir);
static void handleCommandUpdateTypical(RefreshFSMOutput *out, uint32_t frameNdx, uint32_t frameLen, Direction dir);
static void handleCommandNightModeOff(RefreshFSMOutput *out);
static void handleCommandNightModeOn(void);
static void handleStateWaitingForFrames(RefreshFSMOutput *out);
static void handleStateInstallingFrame(RefreshFSMOutput *out);
static void handleStateClearingFrame(RefreshFSMOutput *out);
static void handleStateQuickClearingFrame(RefreshFSMOutput *out);
static bool releaseFrame(RefreshFSMFrameReleaseList *framesToRelease, uint32_t index, RefreshFSMFrameReleaseType type);
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
void refreshFSMInit(const RefreshFSMResources *resources)
{
    assert(NULL != resources);
    assert(NULL != resources->LEDNumToReg);
    assert(NULL != resources->LEDFrames);
    assert(0 != resources->LEDFramesLen);

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
RefreshFSMOutput refreshFSMTick(const RefreshFSMCommand *cmd)
{
    assert(NULL != cmd);

    RefreshFSMOutput out = {
        .isIdle = false,
        .framesToRelease.len = 0,
        .action.type = REFRESH_ACTION_NONE,
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
        case REFRESH_CMD_REFRESH:
            handleCommandRefresh(&out, cmd->dir);
            break;
        case REFRESH_CMD_UPDATE_TYPICAL:
            handleCommandUpdateTypical(&out, cmd->frameNdx, cmd->frameLen, cmd->dir);
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
 * Transitions the FSM to the REFRESH_FSM_INSTALLING_FRAME state.
 */
static void transitionToInstallingFrame(RefreshFSMOutput *out)
{
    assert(NULL != out);

    /* latch queued frames */
    if (UINT32_MAX != fsm.nextFrameNdx)
    {
        ESP_LOGI(TAG, "latching currNdx from %lu to %lu", fsm.currFrameNdx, fsm.nextFrameNdx);
        if (UINT32_MAX != fsm.currFrameNdx)
        {
            bool released = releaseFrame(&out->framesToRelease, fsm.currFrameNdx, REFRESH_FSM_FRAME_RELEASE_STANDARD);
            assert(released && "failed to release current frame");
        }
        fsm.currFrameNdx = fsm.nextFrameNdx;
        fsm.currFrameLen = fsm.nextFrameLen;
        fsm.currDir = fsm.nextDir;
        fsm.nextFrameNdx = UINT32_MAX;
        fsm.nextFrameLen = 0;
    }

    if (UINT32_MAX != fsm.nextTypicalNorthFrameNdx)
    {
        ESP_LOGI(TAG, "latching typicalNorthNdx from %lu to %lu", fsm.currTypicalNorthFrameNdx, fsm.nextTypicalNorthFrameNdx);
        if (UINT32_MAX != fsm.currTypicalNorthFrameNdx)
        {
            bool released = releaseFrame(&out->framesToRelease, fsm.currTypicalNorthFrameNdx, REFRESH_FSM_FRAME_RELEASE_TYPICAL_NORTH);
            assert(released && "failed to release current typical north frame");
        }
        fsm.currTypicalNorthFrameNdx = fsm.nextTypicalNorthFrameNdx;
        fsm.nextTypicalNorthFrameNdx = UINT32_MAX;
    }
    
    if (UINT32_MAX != fsm.nextTypicalSouthFrameNdx)
    {
        ESP_LOGI(TAG, "latching typicalSouthNdx from %lu to %lu", fsm.currTypicalSouthFrameNdx, fsm.nextTypicalSouthFrameNdx);
        if (UINT32_MAX != fsm.currTypicalSouthFrameNdx)
        {
            bool released = releaseFrame(&out->framesToRelease, fsm.currTypicalSouthFrameNdx, REFRESH_FSM_FRAME_RELEASE_TYPICAL_SOUTH);
            assert(released && "failed to release current typical south frame");
        }
        fsm.currTypicalSouthFrameNdx = fsm.nextTypicalSouthFrameNdx;
        fsm.nextTypicalSouthFrameNdx = UINT32_MAX;
    }

    /* start installing frame */
    fsm.currLEDNdx = 0;
    fsm.state = REFRESH_FSM_INSTALLING_FRAME;
}

/**
 * Attempts to transition out of the REFRESH_FSM_WAITING_FOR_FRAMES
 * state once both typical frames and a current frame are available.
 * Transitions to REFRESH_FSM_INSTALLING_FRAME, unless night mode is
 * on, in which case the FSM instead goes to REFRESH_FSM_CLEARED so
 * the board stays dark until night mode is turned off.
 */
static void tryTransitionFromWaitingForFrames(RefreshFSMOutput *out)
{
    const bool haveBothTypicalFrames = (fsm.nextTypicalNorthFrameNdx != UINT32_MAX) &&
                                       (fsm.nextTypicalSouthFrameNdx != UINT32_MAX);
    if (haveBothTypicalFrames && fsm.currFrameNdx != UINT32_MAX)
    {
        if (!fsm.nightMode)
        {
            transitionToInstallingFrame(out);
        } else
        {
            fsm.state = REFRESH_FSM_CLEARED;
        }
    }
}

/**
 * Updates the FSM state to start installing the new LED frame.
 * 
 * @note If LEDs are off, the new frame starts being installed instantly.
 * If a frame is already installed, the board will start being cleared.
 * If the board is already being cleared, the board will be cleared quickly.
 * 
 * @param frameNdx The index in LEDFrames of the new frame.
 * @param frameLen The length of the new frame.
 * @param dir The traffic direction of the new frame.
 */
static void handleCommandNewFrame(RefreshFSMOutput *out, uint32_t frameNdx, uint32_t frameLen, Direction dir)
{
    assert(NULL != out);
    assert(UINT32_MAX != frameNdx);

    /* buffer new frame */
    if (UINT32_MAX == fsm.currFrameNdx)
    {
        /* latch new frame */
        fsm.currFrameNdx = frameNdx;
        fsm.currFrameLen = frameLen;
        fsm.currDir = dir;
    } else
    {
        /* buffer new frame */
        if (UINT32_MAX != fsm.nextFrameNdx)
        {
            bool released = releaseFrame(&out->framesToRelease, fsm.nextFrameNdx, REFRESH_FSM_FRAME_RELEASE_QUEUED_STANDARD);
            assert(released && "failed to release queued standard frame");
        }
        fsm.nextFrameNdx = frameNdx;
        fsm.nextFrameLen = frameLen;
        fsm.nextDir = dir;
    }



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
        default:
            assert(0 && "encountered unknown refresh FSM state");
            break;
    }
}

static void handleCommandRefresh(RefreshFSMOutput *out, Direction dir)
{
    assert(NULL != out);
    
    switch (fsm.state)
    {
        case REFRESH_FSM_WAITING_FOR_FRAMES:
            /* falls through */
        case REFRESH_FSM_INSTALLING_FRAME:
            /* falls through */
        case REFRESH_FSM_CLEARING_FRAME:
            /* falls through */
        case REFRESH_FSM_QUICK_CLEARING_FRAME:
            /* falls through */
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
        default:
            assert(0 && "encountered unknown refresh FSM state");
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
 * @param frameLen The length of the frame.
 * @param dir The traffic direction this typical
 * speed frame corresponds to.
 */
static void handleCommandUpdateTypical(RefreshFSMOutput *out, uint32_t frameNdx, uint32_t frameLen, Direction dir)
{
    assert(NULL != out);
    assert(UINT32_MAX != frameNdx);
    assert(0 != frameLen);

    /* update buffered typical direction */
    switch (dir)
    {
        case NORTH:
            ESP_LOGI(TAG, "buffered typicalNorthNdx %lu switched to ndx %lu", fsm.nextTypicalNorthFrameNdx, frameNdx);
            if (UINT32_MAX != fsm.nextTypicalNorthFrameNdx)
            {
                bool released = releaseFrame(&out->framesToRelease, fsm.nextTypicalNorthFrameNdx, REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL);
                assert(released && "failed to release queued typical frame");
            }
            fsm.nextTypicalNorthFrameNdx = frameNdx;
            break;
        case SOUTH:
            ESP_LOGI(TAG, "buffered typicalSouthNdx %lu switched to ndx %lu", fsm.nextTypicalSouthFrameNdx, frameNdx);
            if (UINT32_MAX != fsm.nextTypicalSouthFrameNdx)
            {
                bool released = releaseFrame(&out->framesToRelease, fsm.nextTypicalSouthFrameNdx, REFRESH_FSM_FRAME_RELEASE_QUEUED_TYPICAL);
                assert(released && "failed to release queued typical frame");
            }
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
    assert(NULL != out);

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
        case REFRESH_FSM_INSTALLING_FRAME:
            fsm.state = REFRESH_FSM_CLEARING_FRAME;
            break;
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
    assert(NULL != out);

    out->isIdle = true;
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

    if (!done) return;

    if (!fsm.nightMode)
    {
        transitionToInstallingFrame(out);
    } else
    {
        out->isIdle = true;
        fsm.state = REFRESH_FSM_CLEARED;
    }
}

/**
 * Turns off every currently-lit LED of the current frame in a single
 * tick, in the same order they would be turned off one at a time by
 * REFRESH_FSM_CLEARING_FRAME (i.e. the reverse of install order).
 *
 * @note This state clears the board in a single tick.
 * @note currLEDNdx marks one past the highest frame index that could
 * still be lit, whether we arrived here mid-install (indices below it
 * were already set) or mid-clear (indices below it are not yet
 * cleared) - see iterateFrame.
 *
 * @param fsm The refresh FSM.
 * @param[out] out Upstream output for communication
 * with the task running the FSM.
 */
static void handleStateQuickClearingFrame(RefreshFSMOutput *out)
{
    assert(NULL != out);
    assert(UINT32_MAX != fsm.currFrameNdx);

    if (0 != fsm.currLEDNdx)
    {
        out->action.type = REFRESH_ACTION_CLEAR_RANGE;
        out->action.clearRange.frameNdx = fsm.currFrameNdx;
        out->action.clearRange.startNdx = fsm.currLEDNdx - 1;
    }

    if (!fsm.nightMode)
    {
        transitionToInstallingFrame(out);
    } else
    {
        fsm.currLEDNdx = 0;
        out->isIdle = true;
        fsm.state = REFRESH_FSM_CLEARED;
    }
}

/**
 * Adds the frame to the list of frames to release.
 * 
 * @param framesToRelease The list to add the frame to release to.
 * @param index The ledFrames index of the frame to release.
 * @param type The type of release.
 * 
 * @returns True if the frame was added to the list.
 * False if the list was full.
 */
static bool releaseFrame(RefreshFSMFrameReleaseList *framesToRelease, uint32_t index, RefreshFSMFrameReleaseType type)
{
    assert(NULL != framesToRelease);
    if (framesToRelease->len >= REFRESH_FSM_FRAME_RELEASE_TYPES_COUNT) return false;

    framesToRelease->list[framesToRelease->len].index = index;
    framesToRelease->list[framesToRelease->len].type = type;
    framesToRelease->len++;

    return true;
}

/**
 * Iterates through the LEDs of the current frame, either
 * setting the LED color or clearing the LED.
 * 
 * @param fsm The refresh FSM.
 * @param[out] out Upstream output for communication
 * with the task running the FSM.
 * @param setLED Whether to set or clear the LED. If clearing, the interation
 * direction is reversed.
 * 
 * @returns True if iteration is complete, false otherwise.
 */
static bool iterateFrame(RefreshFSMOutput *out, bool setLED)
{
    LEDSpeed speed;
    
    assert(NULL != out);

    /* find next LED */
    bool foundValidLED = false;
    if (setLED)
    {
        /* iterate in forward direction */
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
    } else
    {
        /* iterate in reverse direction */
        fsm.currLEDNdx--; // index is ahead by 1
        for (; fsm.currLEDNdx != UINT32_MAX; fsm.currLEDNdx--)
        {
            speed = fsm.LEDFrames[fsm.currFrameNdx][fsm.currLEDNdx];
            if (speed.ledNum >= fsm.LEDNumToRegLen) continue;
            if (isLEDValid(fsm.LEDNumToReg[speed.ledNum]))
            {
                foundValidLED = true;
                break;
            }
        }
    }


    /* generate output action */
    if (foundValidLED)
    {
        if (setLED)
        {
            fsm.currLEDNdx++;
            out->action.type = REFRESH_ACTION_SET;
            out->action.set.ledNum = speed.ledNum;
            out->action.set.color = ledSpeedToColor(speed, fsm.currDir);
        } else
        {
            out->action.type = REFRESH_ACTION_CLEAR;
            out->action.clear.ledNum = speed.ledNum;
        }
    } else
    {
        out->action.type = REFRESH_ACTION_NONE;
    }

    return fsm.currLEDNdx >= fsm.currFrameLen || fsm.currLEDNdx == 0;
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
    uint32_t percentFlow = (uint32_t) (100 * ((float) speed.speed / (float) typicalSpeed.speed));
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
        case REFRESH_FSM_STATE_COUNT: return "STATE_COUNT";
    }
    return "UNKNOWN";
}
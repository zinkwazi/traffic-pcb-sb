### Refresh Component

This component contains a task that is responsible for controlling traffic and
direction LEDs. The task retrieves the following commands from a queue:

- NEW_FRAME: Clears and installs a new 'frame' of LED colors to the board. The callee
reserves one of multiple frame buffers for their frame from a 2D array. They then
add a command to the refresh task's command queue or unreserve the frame if the queue
is full.

- NIGHT_MODE_ENABLE: Turns on night mode. When enabled, the refresh task will turn off
direction and traffic LEDs and not install new frames to the board, but will still store
the latest frame received from the queue for later use.

- NIGHT_MODE_DISABLE: Turns off night mode. The refresh task will install the previously
stored frame to the board.

When installing an LED frame to the board, the refresh task will update LEDs in the order provided by the structure of the frame. It will delay an amount of time between each LED update to create the effect of an animation on the board.

If hardware faults cause the LED update to fail, the task will retry the update for up to the length of the delay between LED updates. That is, the task should always complete a frame update in a bounded amount of time called the 'animation period'.

After a frame is installed, the refresh task should attempt to update any LEDs that could not be set in the animation period. It will continue attempting to update these LEDs until successful or the task receives a new frame to install.

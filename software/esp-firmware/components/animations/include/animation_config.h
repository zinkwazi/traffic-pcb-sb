/**
 * animation_config.h
 * 
 * Contains configuration macros for animations.
 */

#ifndef ANIMATION_CONFIG_H_
#define ANIMATION_CONFIG_H_

/* The angle of the diagonal line and curved diagonal line animations */
#define DIAG_LINE_ANGLE (M_PI / 3)




/* The parabolic factor of the curved diagonal line animation, which controls
how quickly distance values grow. This factor is necessary if values are growing
too large to be accurately represented by a double. If this value is too low,
a similar issue may occur where there is underflow in the representation or
intermediate step of the calculation. */
#define NORTH_GROWTH_FACTOR 1

/* The amount of ovaling of curved animations, which controls the 'pointiness' 
of the curve. A greater value increases the pointness, with a value of 1
corresponding to a perfect circle. */
#define NORTH_OVAL_FACTOR 3

/* The distance from the origin of the origin of curved animation ovals,
in the direction of DIAG_LINE_ANGLE. This will affect the 'pointiness' of
the curve along with OVAL_FACTOR. */
#define CURVED_NORTH_OFFSET 300

/* The tangential offset of the center of the animation. As the animation 
naturally likes to have a tangent that is a line with an angle equal to
DIAG_LINE_ANGLE through the origin, this parameter moves this point in the 
direction of the tangent. */
#define CURVED_NORTH_TANGENTIAL_OFFSET 30




/* The parabolic factor of the curved diagonal line animation, which controls
how quickly distance values grow. This factor is necessary if values are growing
too large to be accurately represented by a double. If this value is too low,
a similar issue may occur where there is underflow in the representation or
intermediate step of the calculation. */
#define SOUTH_GROWTH_FACTOR 1

/* The amount of ovaling of curved animations, which controls the 'pointiness' 
of the curve. A greater value increases the pointness, with a value of 1
corresponding to a perfect circle. */
#define SOUTH_OVAL_FACTOR 6

/* The distance from the origin of the origin of curved animation ovals,
in the direction of DIAG_LINE_ANGLE. This will affect the 'pointiness' of
the curve along with OVAL_FACTOR. */
#define CURVED_SOUTH_OFFSET 300

/* The tangential offset of the center of the animation. As the animation 
naturally likes to have a tangent that is a line with an angle equal to
DIAG_LINE_ANGLE through the origin, this parameter moves this point in the 
direction of the tangent. */
#define CURVED_SOUTH_TANGENTIAL_OFFSET 30

#endif /* ifndef ANIMATION_CONFIG_H_ */
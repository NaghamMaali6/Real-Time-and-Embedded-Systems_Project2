#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "config.h"
#include "ipc.h"
#include "traffic_light.h"

/* =====================================================
 * Traffic phases
 * ===================================================== */
typedef enum
{
    PHASE_NS_STRAIGHT_GREEN,
    PHASE_NS_STRAIGHT_YELLOW,

    PHASE_NS_LEFT_GREEN,
    PHASE_NS_LEFT_YELLOW,

    PHASE_EW_STRAIGHT_GREEN,
    PHASE_EW_STRAIGHT_YELLOW,

    PHASE_EW_LEFT_GREEN,
    PHASE_EW_LEFT_YELLOW,

    PHASE_ALL_RED,
    PHASE_PEDESTRIAN,
    PHASE_EMERGENCY

} TrafficPhase;

/* =====================================================
 * Controller state
 * ===================================================== */
typedef struct
{
    TrafficLight    lights[8];        /* 8 lights, indices match image  */
    TrafficPhase    current_phase;
    TrafficPhase    next_phase;
    time_t          phase_start;
    int             emergency_dir;    /* -1 if none                     */
    int             emergency_move ; 
    int             ped_pending[4];   /* one per Direction               */
    int             ped_direction;    /* active ped crossing dir, -1=none*/
    time_t transition_end;
    int waiting_transition;
    TrafficPhase pending_next;

} ControllerState;


/* =====================================================
 * Light index mapping (matches your image):
 *  0 = North Left   1 = North Right
 *  2 = East  Left   3 = East  Right
 *  4 = South Left   5 = South Right
 *  6 = West  Left   7 = West  Right
 * ===================================================== */
#define LIGHT_NORTH_LEFT   0
#define LIGHT_NORTH_RIGHT  1
#define LIGHT_EAST_LEFT    2
#define LIGHT_EAST_RIGHT   3
#define LIGHT_SOUTH_LEFT   4
#define LIGHT_SOUTH_RIGHT  5
#define LIGHT_WEST_LEFT    6
#define LIGHT_WEST_RIGHT   7

void controller_run(const Config* config);

#endif

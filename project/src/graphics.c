/*=============================================================
 * graphics.c
 *============================================================*/
#include "graphics.h"
#include "ipc.h"
#include "traffic_light.h"
#include "pedestrian.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef __APPLE__
  #include <GLUT/glut.h>
#else
  #include <GL/glut.h>
#endif

/*=============================================================
 * Constants
 *============================================================*/
#define WIN_W         1100
#define WIN_H         900
#define TITLE         "Traffic Light Control System"

#define ROAD_W        0.30f
#define LANE_W        0.15f
#define CX            0.58f
#define CY            0.50f

#define TIMER_MS      16      /* ~60 FPS redraw/update cadence     */

#define PED_CROSS_DIST   (ROAD_W + 0.11f)

/*=============================================================
 * Pre-computed road edges
 *============================================================*/
static float road_l, road_r, road_b, road_t;

/*=============================================================
 * Config pointer
 *============================================================*/
static const Config* g_config = NULL;

/*=============================================================
 * Phase names — order must match TrafficPhase enum
 *============================================================*/
static const char* phase_names[] =
{
    "NS STRAIGHT GREEN",   /*  0 */
    "NS STRAIGHT YELLOW",  /*  1 */
    "NS LEFT GREEN",       /*  2 */
    "NS LEFT YELLOW",      /*  3 */
    "EW STRAIGHT GREEN",   /*  4 */
    "EW STRAIGHT YELLOW",  /*  5 */
    "EW LEFT GREEN",       /*  6 */
    "EW LEFT YELLOW",      /*  7 */
    "ALL RED",             /*  8 */
    "PEDESTRIAN",          /*  9 */
    "EMERGENCY"            /* 10 */
};

/*=============================================================
 * Helpers
 *============================================================*/
static void rgb(float r, float g, float b)
{
    glColor3f(r, g, b);
}

static void fill_rect(float x, float y, float w, float h)
{
    glBegin(GL_QUADS);
        glVertex2f(x,     y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x,     y + h);
    glEnd();
}

static void fill_circle(float cx, float cy, float r)
{
    int   i;
    float angle;

    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (i = 0; i <= 32; i++)
        {
            angle = 2.0f * 3.14159f * (float)i / 32.0f;
            glVertex2f(cx + r * cosf(angle),
                       cy + r * sinf(angle));
        }
    glEnd();
}

static void draw_text(float x, float y, const char* str)
{
    const char* c;
    glRasterPos2f(x, y);
    for (c = str; *c != '\0'; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
}

static void draw_text_large(float x, float y, const char* str)
{
    const char* c;
    glRasterPos2f(x, y);
    for (c = str; *c != '\0'; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
}
/*
static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}*/

static float bezier2(float p0, float p1, float p2, float t)
{
    float u = 1.0f - t;
    return u * u * p0 + 2.0f * u * t * p1 + t * t * p2;
}

/*=============================================================
 * Zebra crossings
 *============================================================*/
static void draw_zebra(float x, float y,
                       float w, float h,
                       int horizontal)
{
    int   i, stripes = 5;
    float sw;

    if (horizontal)
    {
        sw = w / (float)(stripes * 2 - 1);
        for (i = 0; i < stripes; i++)
        {
            rgb(1, 1, 1);
            fill_rect(x + i * sw * 2.0f, y, sw, h);
        }
    }
    else
    {
        sw = h / (float)(stripes * 2 - 1);
        for (i = 0; i < stripes; i++)
        {
            rgb(1, 1, 1);
            fill_rect(x, y + i * sw * 2.0f, w, sw);
        }
    }
}

/*=============================================================
 * Lane dashes
 *============================================================*/
static void draw_dashes_h(float x, float y, float len)
{
    float xp, dash = 0.025f, gap = 0.018f;
    rgb(0.9f, 0.8f, 0.0f);
    for (xp = x; xp < x + len; xp += dash + gap)
        fill_rect(xp, y - 0.003f, dash, 0.006f);
}

static void draw_dashes_v(float x, float y, float len)
{
    float yp, dash = 0.025f, gap = 0.018f;
    rgb(0.9f, 0.8f, 0.0f);
    for (yp = y; yp < y + len; yp += dash + gap)
        fill_rect(x - 0.003f, yp, 0.006f, dash);
}

/*=============================================================
 * pedestrain light 
 *============================================================*/
void draw_ped_light(float x, float y, int state)
{
    float w = 0.015f;
    float h = 0.030f;
    float r = 0.0055f;

    // body 
    rgb(0.08f, 0.08f, 0.08f);
    fill_rect(x - w/2, y - h/2, w, h);

    // RED 
    if (state == PED_DONT_WALK)
        rgb(1.0f, 0.0f, 0.0f);
    else
        rgb(0.25f, 0.0f, 0.0f);

    fill_circle(x, y + h*0.22f, r);

    // GREEN 
    if (state == PED_WALK)
        rgb(0.0f, 1.0f, 0.0f);
    else
        rgb(0.0f, 0.25f, 0.0f);

    fill_circle(x, y - h*0.22f, r);
}
void set_pedestrian_direction(int dir)
{
    int i;

    ipc_sem_wait(sem_id, SEM_MUTEX);

    /* خلي كل الإشارات أحمر */
    for (i = 0; i < DIR_COUNT; i++)
    {
        shared_data->pedestrian_light[i] = PED_DONT_WALK;
    }

    /* الاتجاه المطلوب فقط أخضر */
    if (dir >= 0 && dir < DIR_COUNT)
    {
        shared_data->pedestrian_light[dir] = PED_WALK;
    }

    ipc_sem_signal(sem_id, SEM_MUTEX);
}
static void handle_key(unsigned char key, int x, int y)
{
    (void)x;
    (void)y;
    switch (key)
    {
        case 'n': set_pedestrian_direction(DIR_NORTH); break;
        case 's': set_pedestrian_direction(DIR_SOUTH); break;
        case 'e': set_pedestrian_direction(DIR_EAST); break;
        case 'w': set_pedestrian_direction(DIR_WEST); break;
        case 'q': exit(0); break;
    }
}

/*=============================================================
 * Intersection background
 *============================================================*/
static void draw_intersection(void)
{
    float zw   = LANE_W;
    float zlen = 0.055f;
    float zoff = 0.008f;

    /* Grass */
    rgb(0.18f, 0.42f, 0.18f);
    fill_rect(0, 0, 1, 1);

    /* Sidewalk corners */
    rgb(0.68f, 0.62f, 0.52f);
    fill_rect(0,      0,      road_l,   road_b);
    fill_rect(road_r, 0,      1-road_r, road_b);
    fill_rect(0,      road_t, road_l,   1-road_t);
    fill_rect(road_r, road_t, 1-road_r, 1-road_t);

    /* Asphalt */
    rgb(0.20f, 0.20f, 0.20f);
    fill_rect(road_l, 0,      ROAD_W, 1);
    fill_rect(0,      road_b, 1,      ROAD_W);

    /* Lane dashes */
    draw_dashes_v(CX, 0,      road_b);
    draw_dashes_v(CX, road_t, 1.0f - road_t);
    draw_dashes_h(0,      CY, road_l);
    draw_dashes_h(road_r, CY, 1.0f - road_r);

    rgb(1.0f, 1.0f, 1.0f);

    /* Vertical lane separator */
    fill_rect(CX - 0.075f, 0.0f, 0.002f, 1.0f);
    fill_rect(CX + 0.075f, 0.0f, 0.002f, 1.0f);

    /* Horizontal lane separator */
    fill_rect(0.0f, CY - 0.075f, 1.0f, 0.002f);
    fill_rect(0.0f, CY + 0.075f, 1.0f, 0.002f);

    /* Zebra crossings */
    draw_zebra(road_l,              road_t + zoff,        zw,   zlen, 0);
    draw_zebra(road_l + zw,         road_t + zoff,        zw,   zlen, 0);
    draw_zebra(road_l,              road_b - zoff - zlen, zw,   zlen, 0);
    draw_zebra(road_l + zw,         road_b - zoff - zlen, zw,   zlen, 0);
    draw_zebra(road_r + zoff,       road_b,               zlen, zw,   1);
    draw_zebra(road_r + zoff,       road_b + zw,          zlen, zw,   1);
    draw_zebra(road_l - zoff - zlen, road_b,              zlen, zw,   1);
    draw_zebra(road_l - zoff - zlen, road_b + zw,         zlen, zw,   1);

    // Pedestrian signals near crosswalks
    float off = 0.035f;   // مسافة صغيرة خارج الشارع

    // NORTH (أعلى الشارع - عند الممر الشمالي)
    draw_ped_light(
        CX - LANE_W * 1.2f,
        road_t + off,
        shared_data->pedestrian_light[DIR_NORTH]
    );

    // SOUTH (أسفل الشارع)
    draw_ped_light(
        CX + LANE_W * 1.2f,
        road_b - off,
        shared_data->pedestrian_light[DIR_SOUTH]
    );

    // EAST (يمين الشارع)
    draw_ped_light(
        road_r + off,
        CY + LANE_W * 1.2f,
        shared_data->pedestrian_light[DIR_EAST]
    );

    // WEST (يسار الشارع)
    draw_ped_light(
        road_l - off,
        CY - LANE_W * 1.2f,
        shared_data->pedestrian_light[DIR_WEST]
    );

    /* Intersection box */
    rgb(0.22f, 0.22f, 0.22f);
    fill_rect(road_l, road_b, ROAD_W, ROAD_W);

    /* Direction labels */
    rgb(1.0f, 1.0f, 1.0f);
    /* NORTH */
    draw_text_large(CX - 0.035f, 0.985f, "NORTH");

    /* SOUTH */
    draw_text_large(CX - 0.035f, 0.005f, "SOUTH");

    /* WEST */
    draw_text_large(0.215f, CY - 0.010f, "WEST");

    /* EAST */
    draw_text_large(0.905f, CY - 0.010f, "EAST");

    /* =====================================================
    * Lane arrows
    * ===================================================== 
    rgb(1.0f, 1.0f, 1.0f);
    float label_off_y = 0.12f;

    draw_text_large(CX - 0.105f, road_t + label_off_y, "STR");
    draw_text_large(CX - 0.045f, road_t + label_off_y, "LEFT");

    draw_text_large(CX + 0.055f, road_b - label_off_y, "STR");
    draw_text_large(CX - 0.010f, road_b - label_off_y, "LEFT");

    draw_text_large(road_r + 0.075f, CY + 0.085f, "STR");
    draw_text_large(road_r + 0.075f, CY + 0.140f, "LEFT");

    draw_text_large(road_l - 0.120f, CY - 0.080f, "STR");
    draw_text_large(road_l - 0.120f, CY - 0.135f, "LEFT");*/

}

/*=============================================================
 * Single traffic light
 *============================================================*/
static void draw_traffic_light(float cx, float cy,
                               LightColor active)
{
    float hw  = 0.020f;
    float hh  = 0.055f;
    float br  = 0.007f;
    float gap = 0.017f;

    rgb(0.18f, 0.18f, 0.18f);
    fill_rect(cx - hw, cy - hh, hw * 2, hh * 2);

    rgb(0.06f, 0.06f, 0.06f);
    fill_rect(cx - hw + 0.002f, cy - hh + 0.002f,
              hw * 2 - 0.004f,  hh * 2 - 0.004f);

    rgb(active == LIGHT_RED    ? 1.0f : 0.22f, 0.0f, 0.0f);
    fill_circle(cx, cy + gap, br);

    rgb(active == LIGHT_YELLOW ? 1.0f : 0.22f,
        active == LIGHT_YELLOW ? 1.0f : 0.22f,
        0.0f);
    fill_circle(cx, cy, br);

    rgb(0.0f, active == LIGHT_GREEN ? 1.0f : 0.22f, 0.0f);
    fill_circle(cx, cy - gap, br);
}

static void draw_traffic_light_horizontal(float cx, float cy,
                                          LightColor active)
{
    float hw  = 0.055f;
    float hh  = 0.020f;

    float br  = 0.007f;
    float gap = 0.017f;

    /* body */
    rgb(0.18f, 0.18f, 0.18f);
    fill_rect(cx - hw, cy - hh, hw * 2, hh * 2);

    rgb(0.06f, 0.06f, 0.06f);
    fill_rect(cx - hw + 0.002f,
              cy - hh + 0.002f,
              hw * 2 - 0.004f,
              hh * 2 - 0.004f);

    /* RED */
    rgb(active == LIGHT_RED ? 1.0f : 0.22f, 0.0f, 0.0f);
    fill_circle(cx - gap, cy, br);

    /* YELLOW */
    rgb(active == LIGHT_YELLOW ? 1.0f : 0.22f,
        active == LIGHT_YELLOW ? 1.0f : 0.22f,
        0.0f);
    fill_circle(cx, cy, br);

    /* GREEN */
    rgb(0.0f,
        active == LIGHT_GREEN ? 1.0f : 0.22f,
        0.0f);
    fill_circle(cx + gap, cy, br);
}

/*=============================================================
 * All 8 lights
 *============================================================*/
/*static void draw_all_lights(LightColor lights[8])
{
    float off = 0.026f;

    draw_traffic_light(CX - LANE_W * 0.5f, road_t + off,      lights[0]);
    draw_traffic_light(CX + LANE_W * 0.5f, road_t + off,      lights[1]);
    draw_traffic_light(road_r + off,        CY + LANE_W * 0.5f, lights[2]);
    draw_traffic_light(road_r + off,        CY - LANE_W * 0.5f, lights[3]);
    draw_traffic_light(CX + LANE_W * 0.5f, road_b - off,      lights[4]);
    draw_traffic_light(CX - LANE_W * 0.5f, road_b - off,      lights[5]);
    draw_traffic_light(road_l - off,        CY - LANE_W * 0.5f, lights[6]);
    draw_traffic_light(road_l - off,        CY + LANE_W * 0.5f, lights[7]);
}*/

static void draw_all_lights(LightColor lights[8])
{
    float off = 0.045f;

    float lane_left  = 0.03f;
    float lane_right = 0.09f;

    /* =====================================================
     * NORTH
     * ===================================================== */

    /* LEFT TURN lane */
    draw_traffic_light(
        CX - lane_left,
        road_t + off,
        lights[0]
    );

    /* STRAIGHT / RIGHT lane */
    draw_traffic_light(
        CX - lane_right,
        road_t + off,
        lights[1]
    );

    /* =====================================================
     * SOUTH
     * ===================================================== */

    /* LEFT TURN lane */
    draw_traffic_light(
        CX + lane_left,
        road_b - off,
        lights[4]
    );

    /* STRAIGHT / RIGHT lane */
    draw_traffic_light(
        CX + lane_right,
        road_b - off,
        lights[5]
    );

    /* =====================================================
     * EAST
     * ===================================================== */

    /* LEFT TURN lane */
    draw_traffic_light_horizontal(
        road_r + off,
        CY + lane_left,
        lights[2]
    );

    /* STRAIGHT / RIGHT lane */
    draw_traffic_light_horizontal(
        road_r + off,
        CY + lane_right,
        lights[3]
    );

    /* =====================================================
     * WEST
     * ===================================================== */

    /* LEFT TURN lane */
    draw_traffic_light_horizontal(
        road_l - off,
        CY - lane_left,
        lights[6]
    );

    /* STRAIGHT / RIGHT lane */
    draw_traffic_light_horizontal(
        road_l - off,
        CY - lane_right,
        lights[7]
    );
}

/*=============================================================
 * Vehicles
 *
 * Vehicle travel path:
 *   p=0  → at far edge of arm (entrance)
 *   p=0.5 → at stop line (intersection edge)
 *   p=1  → exited the other side
 *
 * Vehicles only move when their direction has green.
 * When red, they queue at the stop line (p stays at ~0.45).
 *============================================================*/
static void draw_vehicles(void)
{
    VisualVehicle v[MAX_VISUAL_VEHICLES];
    LightColor    lights[8];
    int           i;
    float vw = 0.028f;
    float vh = 0.028f;

    float lane_left  = 0.09f;
    float lane_right = 0.03f;

    float cols[8][3] =
    {
        {0.2f, 0.4f, 0.9f},
        {0.1f, 0.7f, 0.3f},
        {0.9f, 0.6f, 0.1f},
        {0.7f, 0.2f, 0.7f},
        {0.9f, 0.9f, 0.2f},
        {0.3f, 0.8f, 0.8f},
        {0.8f, 0.4f, 0.2f},
        {0.5f, 0.5f, 0.9f}
    };

    if (!shared_data) return;

    ipc_sem_wait(sem_id, SEM_MUTEX);
    for (i = 0; i < MAX_VISUAL_VEHICLES; i++)
        v[i] = shared_data->vehicles[i];
    for (i = 0; i < 8; i++)
        lights[i] = shared_data->lights[i];
    ipc_sem_signal(sem_id, SEM_MUTEX);

    int queue_count[4][2] = {{0,0},{0,0},{0,0},{0,0}};
    /*
     * queue_count[direction][lane]
     * lane 0 = LEFT lane, lane 1 = STRAIGHT/RIGHT lane
     * This keeps cars in the two lanes separated while waiting.
     */

    for (i = 0; i < MAX_VISUAL_VEHICLES; i++)
    {
        float x, y, p;
        int   is_green;

        if (!v[i].active) continue;

        p = v[i].progress;
        float stop_p;
        int q;

        if (v[i].emergency)
        {
            is_green = 1;
        }
        else if (v[i].move == MOVE_LEFT)
        {
            switch (v[i].direction)
            {
                case DIR_NORTH: is_green = (lights[0] == LIGHT_GREEN); break;
                case DIR_EAST:  is_green = (lights[2] == LIGHT_GREEN); break;
                case DIR_SOUTH: is_green = (lights[4] == LIGHT_GREEN); break;
                case DIR_WEST:  is_green = (lights[6] == LIGHT_GREEN); break;
                default:        is_green = 0; break;
            }
        }
        else
        {
            switch (v[i].direction)
            {
                case DIR_NORTH:
                    is_green = (lights[1] == LIGHT_GREEN);
                    break;
                case DIR_EAST:
                    is_green = (lights[3] == LIGHT_GREEN);
                    break;
                case DIR_SOUTH:
                    is_green = (lights[5] == LIGHT_GREEN);
                    break;
                case DIR_WEST:
                    is_green = (lights[7] == LIGHT_GREEN);
                    break;
                default:
                    is_green = 0;
                    break;
            }
        }

        /*
         * If red and vehicle has not yet reached stop line,
         * use actual progress for drawing (it's still moving).
         */
        if (!is_green && p < 0.44f)
        {
            /* Vehicle is approaching; draw at actual position */
            p = v[i].progress;
        }
        /*
         * Vehicle is stopped at or near stop line.
         * Stack queued vehicles behind each other in the SAME lane,
         * without mixing the left-turn queue with the straight/right queue.
         */
        else if (!is_green && p >= 0.44f && p < 0.55f)
        {
            int lane_index = (v[i].lane == LANE_LEFT) ? 0 : 1;
            q = queue_count[v[i].direction][lane_index];

            /* First car stops at 0.44, next cars stop behind it in the SAME lane. */
            stop_p = 0.44f - (q * 0.10f);
            if (stop_p < 0.04f) stop_p = 0.04f;

            p = stop_p;

            queue_count[v[i].direction][lane_index]++;
        }

        /*
         * IMPORTANT FIX:
         * The car ENTERS using its incoming lane, but after a turn it must
         * EXIT using the outgoing lane of the destination road.
         *
         * Vertical road:
         *   northbound outgoing lane  = CX + lane_left
         *   southbound outgoing lane  = CX - lane_left
         *
         * Horizontal road:
         *   eastbound outgoing lane   = CY - lane_left
         *   westbound outgoing lane   = CY + lane_left
         */
        const float N_IN_LEFT   = CX - lane_right;
        const float N_IN_STR    = CX - lane_left;
        const float S_IN_LEFT   = CX + lane_right;
        const float S_IN_STR    = CX + lane_left;
        const float E_IN_LEFT   = CY + lane_right;
        const float E_IN_STR    = CY + lane_left;
        const float W_IN_LEFT   = CY - lane_right;
        const float W_IN_STR    = CY - lane_left;

        const float OUT_NORTH_X = CX + lane_left;
        const float OUT_SOUTH_X = CX - lane_left;
        const float OUT_EAST_Y  = CY - lane_left;
        const float OUT_WEST_Y  = CY + lane_left;

        switch (v[i].direction)
        {
            case DIR_NORTH:   /* coming from NORTH, moving down */
                if (v[i].move == MOVE_STRAIGHT)
                {
                    x = N_IN_STR;
                    if (p < 0.5f)
                        y = 1.10f - p * 2.0f * (1.10f - road_t);
                    else
                        y = road_t - (p - 0.5f) * 2.0f * (road_t + 0.15f);
                }
                else if (v[i].move == MOVE_RIGHT)  /* N -> W */
                {
                    if (p < 0.5f)
                    {
                        x = N_IN_STR;
                        y = 1.10f - p * 2.0f * (1.10f - road_t);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(N_IN_STR, road_l - 0.02f, -0.12f, t);
                        y = bezier2(road_t,   CY + 0.08f,     OUT_WEST_Y, t);
                    }
                }
                else                                /* N -> E */
                {
                    if (p < 0.5f)
                    {
                        x = N_IN_LEFT;
                        y = 1.10f - p * 2.0f * (1.10f - road_t);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(N_IN_LEFT, CX + 0.08f, 1.12f, t);
                        y = bezier2(road_t,    CY - 0.08f, OUT_EAST_Y, t);
                    }
                }
                break;

            case DIR_SOUTH:   /* coming from SOUTH, moving up */
                if (v[i].move == MOVE_STRAIGHT)
                {
                    x = S_IN_STR;
                    if (p < 0.5f)
                        y = -0.10f + p * 2.0f * (road_b + 0.10f);
                    else
                        y = road_b + (p - 0.5f) * 2.0f * (1.15f - road_b);
                }
                else if (v[i].move == MOVE_RIGHT)  /* S -> E */
                {
                    if (p < 0.5f)
                    {
                        x = S_IN_STR;
                        y = -0.10f + p * 2.0f * (road_b + 0.10f);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(S_IN_STR, road_r + 0.02f, 1.12f, t);
                        y = bezier2(road_b,   CY - 0.08f,     OUT_EAST_Y, t);
                    }
                }
                else                                /* S -> W */
                {
                    if (p < 0.5f)
                    {
                        x = S_IN_LEFT;
                        y = -0.10f + p * 2.0f * (road_b + 0.10f);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(S_IN_LEFT, CX - 0.08f, -0.12f, t);
                        y = bezier2(road_b,    CY + 0.08f, OUT_WEST_Y, t);
                    }
                }
                break;

            case DIR_EAST:    /* coming from EAST, moving left */
                if (v[i].move == MOVE_STRAIGHT)
                {
                    y = E_IN_STR;
                    if (p < 0.5f)
                        x = 1.10f - p * 2.0f * (1.10f - road_r);
                    else
                        x = road_r - (p - 0.5f) * 2.0f * (road_r + 0.15f);
                }
                else if (v[i].move == MOVE_RIGHT)  /* E -> N */
                {
                    if (p < 0.5f)
                    {
                        y = E_IN_STR;
                        x = 1.10f - p * 2.0f * (1.10f - road_r);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(road_r,  CX + 0.08f, OUT_NORTH_X, t);
                        y = bezier2(E_IN_STR, road_t + 0.02f, 1.12f, t);
                    }
                }
                else                                /* E -> S */
                {
                    if (p < 0.5f)
                    {
                        y = E_IN_LEFT;
                        x = 1.10f - p * 2.0f * (1.10f - road_r);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(road_r,   CX - 0.08f, OUT_SOUTH_X, t);
                        y = bezier2(E_IN_LEFT, road_b - 0.02f, -0.12f, t);
                    }
                }
                break;

            case DIR_WEST:    /* coming from WEST, moving right */
                if (v[i].move == MOVE_STRAIGHT)
                {
                    y = W_IN_STR;
                    if (p < 0.5f)
                        x = -0.10f + p * 2.0f * (road_l + 0.10f);
                    else
                        x = road_l + (p - 0.5f) * 2.0f * (1.15f - road_l);
                }
                else if (v[i].move == MOVE_RIGHT)  /* W -> S */
                {
                    if (p < 0.5f)
                    {
                        y = W_IN_STR;
                        x = -0.10f + p * 2.0f * (road_l + 0.10f);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(road_l,  CX - 0.08f, OUT_SOUTH_X, t);
                        y = bezier2(W_IN_STR, road_b - 0.02f, -0.12f, t);
                    }
                }
                else                                /* W -> N */
                {
                    if (p < 0.5f)
                    {
                        y = W_IN_LEFT;
                        x = -0.10f + p * 2.0f * (road_l + 0.10f);
                    }
                    else
                    {
                        float t = (p - 0.5f) * 2.0f;
                        x = bezier2(road_l,   CX + 0.08f, OUT_NORTH_X, t);
                        y = bezier2(W_IN_LEFT, road_t + 0.02f, 1.12f, t);
                    }
                }
                break;
        }

        /* Body */
        if (v[i].emergency)
            rgb(0.95f, 0.05f, 0.05f);
        else
            rgb(cols[i % 8][0], cols[i % 8][1], cols[i % 8][2]);

        /* Draw vehicle CENTERED on the lane path, not shifted from it. */
        fill_rect(x - vw * 0.5f, y - vh * 0.5f, vw, vh);

        /* Windshield */
        rgb(0.55f, 0.82f, 1.0f);
        fill_rect(x - vw * 0.35f, y - vh * 0.35f,
                  vw * 0.65f,     vh * 0.60f);

        /* Emergency cross */
        if (v[i].emergency)
        {
            rgb(1.0f, 1.0f, 1.0f);
            fill_rect(x - vw * 0.08f, y - vh * 0.42f,
                      vw * 0.16f,     vh * 0.84f);
            fill_rect(x - vw * 0.38f, y - vh * 0.15f,
                      vw * 0.76f,     vh * 0.30f);
        }
    }
}

/*=============================================================
 * Pedestrian
 * Pedestrian crosses from one side of the road to the other.
 * Progress 0 → starting edge, 1 → other edge (full crossing).
 *============================================================*/

static void draw_pedestrians(void)
{
    VisualPedestrian peds[MAX_VISUAL_PEDESTRIANS];

    int i;

    if (!shared_data)
        return;

    /*
     * Copy shared memory locally
     * to minimize semaphore holding time
     */
    ipc_sem_wait(sem_id, SEM_MUTEX);

    for (i = 0; i < MAX_VISUAL_PEDESTRIANS; i++)
    {
        peds[i] = shared_data->pedestrians[i];
    }

    ipc_sem_signal(sem_id, SEM_MUTEX);

    /*
     * Draw every active pedestrian
     */
    for (i = 0; i < MAX_VISUAL_PEDESTRIANS; i++)
    {
        float x;
        float y;

        float r = 0.014f;

        float prog;

        if (!peds[i].active)
            continue;

        prog = peds[i].progress;

        /*
         * Compute screen position
         * based on crossing direction
         */
        switch (peds[i].direction)
        {
            case DIR_NORTH:

                /*
                 * Crosses east-west
                 * across north zebra
                 */
                x = (road_l - 0.055f)
                    + prog * (ROAD_W + 0.11f);

                y = road_t + 0.032f;

                break;

            case DIR_SOUTH:

                /*
                 * Crosses east-west
                 * across south zebra
                 */
                x = road_r
                    - prog * (ROAD_W + 0.11f);

                y = road_b - 0.032f;

                break;

            case DIR_EAST:

                /*
                 * Crosses north-south
                 * across east zebra
                 */
                x = road_r + 0.032f;

                y = (road_b - 0.055f)
                    + prog * (ROAD_W + 0.11f);

                break;

            case DIR_WEST:

                /*
                 * Crosses north-south
                 * across west zebra
                 */
                x = road_l - 0.032f;

                y = (road_b - 0.055f)
                    + prog * (ROAD_W + 0.11f);

                break;

            default:
                continue;
        }

        /*
         * Body
         */
        rgb(1.0f, 0.85f, 0.1f);

        fill_circle(x, y, r);

        /*
         * Head
         */
        rgb(0.95f, 0.73f, 0.50f);

        fill_circle(x,
                    y + r * 1.35f,
                    r * 0.48f);

        /*
         * Legs
         */
        rgb(0.15f, 0.15f, 0.75f);

        fill_rect(x - r * 0.55f,
                  y - r * 1.30f,
                  r * 0.38f,
                  r * 1.10f);

        fill_rect(x + r * 0.18f,
                  y - r * 1.30f,
                  r * 0.38f,
                  r * 1.10f);
    }
}


/*=============================================================
 * HUD panel
 *============================================================*/
static void draw_hud(void)
{
    int    phase, n, s, e, w, emg, emg_dir;
    int    ped[4];
    int    elapsed;
    char   buf[80];
    time_t phase_start, now;

    if (!shared_data) return;

    ipc_sem_wait(sem_id, SEM_MUTEX);
    phase       = shared_data->active_phase;
    n           = shared_data->north_waiting;
    s           = shared_data->south_waiting;
    e           = shared_data->east_waiting;
    w           = shared_data->west_waiting;
    emg         = shared_data->emergency_active;
    emg_dir     = shared_data->emergency_direction;
    ped[0]      = shared_data->pedestrian_request[0];
    ped[1]      = shared_data->pedestrian_request[1];
    ped[2]      = shared_data->pedestrian_request[2];
    ped[3]      = shared_data->pedestrian_request[3];
    phase_start = shared_data->phase_start_time;
    ipc_sem_signal(sem_id, SEM_MUTEX);

    /* Safe elapsed time — guard against uninitialized start */
    now = time(NULL);
    elapsed = (now >= phase_start) ? (int)(now - phase_start) : 0;

    /* Panel */
    rgb(0.05f, 0.05f, 0.10f);
    fill_rect(0.0f, 0.0f, 0.195f, 1.0f);

    rgb(0.22f, 0.22f, 0.42f);
    fill_rect(0.192f, 0.0f, 0.003f, 1.0f);

    /* Title */
    rgb(0.80f, 0.80f, 1.00f);
    draw_text_large(0.010f, 0.958f, "TRAFFIC SIM");

    rgb(0.20f, 0.20f, 0.38f);
    fill_rect(0.010f, 0.940f, 0.172f, 0.002f);

    /* Phase */
    rgb(0.55f, 0.55f, 0.68f);
    draw_text(0.010f, 0.916f, "CURRENT PHASE");

    if (phase >= 0 && phase <= 10)
    {
        if      (phase == 10)                               rgb(1.0f, 0.15f, 0.15f);
        else if (phase ==  9)                               rgb(0.2f, 0.90f, 1.00f);
        else if (phase ==  8)                               rgb(0.8f, 0.35f, 0.35f);
        else if (phase==1||phase==3||phase==5||phase==7)    rgb(1.0f, 1.00f, 0.15f);
        else                                                rgb(0.2f, 1.00f, 0.30f);

        draw_text_large(0.010f, 0.890f, phase_names[phase]);
    }

    rgb(0.45f, 0.45f, 0.55f);
    snprintf(buf, sizeof(buf), "Time in phase: %ds", elapsed);
    draw_text(0.010f, 0.863f, buf);

    rgb(0.20f, 0.20f, 0.38f);
    fill_rect(0.010f, 0.845f, 0.172f, 0.002f);

    /* Vehicle counts */
    rgb(0.65f, 0.65f, 0.85f);
    draw_text(0.010f, 0.822f, "VEHICLES WAITING");

    snprintf(buf, sizeof(buf), "North : %d", n);
    rgb(n > 5 ? 1.0f : 0.55f, n > 5 ? 0.25f : 0.85f, 0.55f);
    draw_text(0.010f, 0.800f, buf);

    snprintf(buf, sizeof(buf), "South : %d", s);
    rgb(s > 5 ? 1.0f : 0.55f, s > 5 ? 0.25f : 0.85f, 0.55f);
    draw_text(0.010f, 0.778f, buf);

    snprintf(buf, sizeof(buf), "East  : %d", e);
    rgb(e > 5 ? 1.0f : 0.55f, e > 5 ? 0.25f : 0.85f, 0.55f);
    draw_text(0.010f, 0.756f, buf);

    snprintf(buf, sizeof(buf), "West  : %d", w);
    rgb(w > 5 ? 1.0f : 0.55f, w > 5 ? 0.25f : 0.85f, 0.55f);
    draw_text(0.010f, 0.734f, buf);

    rgb(0.20f, 0.20f, 0.38f);
    fill_rect(0.010f, 0.716f, 0.172f, 0.002f);

    /* Pedestrian requests */
    rgb(0.65f, 0.65f, 0.85f);
    draw_text(0.010f, 0.695f, "PEDESTRIAN REQUESTS");

    {
        const char* dirs[4] = {"North","South","East","West"};
        int   di;
        float yp  = 0.673f;
        int   any = 0;

        for (di = 0; di < 4; di++)
        {
            if (ped[di])
            {
                rgb(0.25f, 1.0f, 0.85f);
                snprintf(buf, sizeof(buf), "  %s : WAITING", dirs[di]);
                draw_text(0.010f, yp, buf);
                yp  -= 0.022f;
                any  = 1;
            }
        }

        if (!any)
        {
            rgb(0.35f, 0.35f, 0.45f);
            draw_text(0.010f, 0.673f, "  None pending");
        }
    }

    rgb(0.20f, 0.20f, 0.38f);
    fill_rect(0.010f, 0.578f, 0.172f, 0.002f);

    /* Emergency */
    rgb(0.65f, 0.65f, 0.85f);
    draw_text(0.010f, 0.558f, "EMERGENCY STATUS");

    if (emg)
    {
        rgb(0.75f, 0.04f, 0.04f);
        fill_rect(0.010f, 0.512f, 0.172f, 0.038f);

        rgb(1.0f, 1.0f, 1.0f);
        draw_text_large(0.016f, 0.524f, "!! EMERGENCY !!");

        rgb(1.0f, 0.60f, 0.10f);
        snprintf(buf, sizeof(buf), "Direction: %s",
                 emg_dir >= 0 ? direction_to_string(emg_dir) : "?");
        draw_text(0.010f, 0.500f, buf);
    }
    else
    {
        rgb(0.25f, 0.50f, 0.25f);
        draw_text(0.010f, 0.538f, "  No emergency");
    }

    rgb(0.20f, 0.20f, 0.38f);
    fill_rect(0.010f, 0.483f, 0.172f, 0.002f);

    /* Controls */
    rgb(0.38f, 0.38f, 0.48f);
    draw_text(0.010f, 0.462f, "PEDESTRIAN KEYS");
    draw_text(0.010f, 0.442f, "  n = North");
    draw_text(0.010f, 0.422f, "  s = South");
    draw_text(0.010f, 0.402f, "  e = East");
    draw_text(0.010f, 0.382f, "  w = West");
    draw_text(0.010f, 0.362f, "  q = Quit");

    rgb(0.20f, 0.20f, 0.38f);
    fill_rect(0.010f, 0.348f, 0.172f, 0.002f);

    draw_text(0.010f, 0.328f, "Ctrl+C = shutdown");

    /* Timing config */
    if (g_config != NULL)
    {
        rgb(0.20f, 0.20f, 0.38f);
        fill_rect(0.010f, 0.310f, 0.172f, 0.002f);

        rgb(0.38f, 0.38f, 0.48f);
        draw_text(0.010f, 0.292f, "TIMING (seconds)");

        snprintf(buf, sizeof(buf), "  Green  : %d", g_config->green_time);
        draw_text(0.010f, 0.272f, buf);

        snprintf(buf, sizeof(buf), "  Yellow : %d", g_config->yellow_time);
        draw_text(0.010f, 0.252f, buf);

        snprintf(buf, sizeof(buf), "  AllRed : %d", g_config->all_red_time);
        draw_text(0.010f, 0.232f, buf);

        snprintf(buf, sizeof(buf), "  Ped    : %d", g_config->pedestrian_time);
        draw_text(0.010f, 0.212f, buf);

        snprintf(buf, sizeof(buf), "  Emerg  : %d", g_config->emergency_time);
        draw_text(0.010f, 0.192f, buf);
    }

}

/*=============================================================
 * GLUT display callback
 *============================================================*/
static void display(void)
{
    LightColor lights[8];
    int i;

    glClear(GL_COLOR_BUFFER_BIT);

    if (!shared_data) { glutSwapBuffers(); return; }

    ipc_sem_wait(sem_id, SEM_MUTEX);
    for (i = 0; i < 8; i++)
        lights[i] = shared_data->lights[i];
    ipc_sem_signal(sem_id, SEM_MUTEX);

    draw_intersection();
    draw_vehicles();
    draw_pedestrians();
    draw_all_lights(lights);
    draw_hud();

    glutSwapBuffers();
}

/*=============================================================
 * GLUT timer — fires every 500ms
 *============================================================*/
static void timer(int val)
{
    (void)val;

    if (!shared_data)
    {
        glutTimerFunc(TIMER_MS, timer, 0);
        return;
    }

    ipc_sem_wait(sem_id, SEM_MUTEX);

    if (shared_data->system_status == SYSTEM_STOPPED)
    {
        ipc_sem_signal(sem_id, SEM_MUTEX);
        exit(0);
    }

    ipc_sem_signal(sem_id, SEM_MUTEX);

    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, timer, 0);
}

static void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, 1.0, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/*=============================================================
 * Entry point
 *============================================================*/
void graphics_run(int argc, char* argv[], const Config* config)
{
    g_config = config;

    road_l = CX - ROAD_W / 2.0f;
    road_r = CX + ROAD_W / 2.0f;
    road_b = CY - ROAD_W / 2.0f;
    road_t = CY + ROAD_W / 2.0f;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow(TITLE);

    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(handle_key);
    glutTimerFunc(TIMER_MS, timer, 0);

    printf("[GRAPHICS] OpenGL window started.\n");
    glutMainLoop();
    printf("[GRAPHICS] Graphics process exiting.\n");
}



#ifndef simunits_h
#define simunits_h
/*
 * Copyright 2010 Nathanael C. Nerode
 * & Simutrans Contributors
 * This file is part of the Simutrans project and is available under the
 * Artistic License -- see license.txt
 */

/*
 * This file is designed to contain the unit conversion routines
 * which were previously scattered all over the Simutrans code.
 *
 * Some are still scattered all over the code.  So this file also documents
 * where the other conversion routines are.
 *
 * I have tried to leave out stuff which should be internal to the display
 * and have no gameplay consequences.
 *
 * DISTANCE units:
 * 1 -- tiles
 * 2 -- "internal pixels" -- 16 per tile.  Old steps per tile.
		-- OBJECT_OFFSET_STEPS, located in simconst.h
		-- important to the internal rendering engine
		-- used for locating images such as trees within a tile
		-- same value as carunits, but conceptually different....
 * 3 -- "carunits" or vehicle length units -- same as old steps per tile
        -- Length of trains & other vehicles (as specified in paks)
        -- is measured in this unit.  Currently this is very hard to alter.
 * 4 -- vehicle steps
        -- trains can be on one of 2^8 = 256 locations along a tile horizontally (not
           diagonally).
 * 5 -- "yards" -- tiny distance units used internally
        -- 2^12 per vehicle steps, by definition
		-- 2^16 per vehicle steps in older code
		-- chosen to maximize precision of certain interfaces
 * 6 -- km -- 1/tile in standard; variable in Experimental with the meters_per_tile setting
 *
 * TIME units:
 * 1 -- ticks -- referred to as ms or milliseconds in old code
 * 2 -- months -- determined by karte_t::ticks_per_world_month
 *      -- you may also use karte_t::ticks_per_world_month_shift
 * 3 -- days -- derived from months
 * 4 -- years -- derived from months
 * 5 -- hours & minutes -- NOT derived from months, implied by vehicle speed 
 *      -- (express for journey, waiting, reversing, loading and spacing times in Experimental)
 *      -- 100 km/h = (100 << 10) / 80 "yards"/tick = 1280 "yards"/tick. (see macro kmh_to_speed below)
 *      -- Assuming 1000 meters per tile, 1h = 104857600/1280 = 81920 ticks; 
 *      -- 3min = 81920/20 = 4096 ticks
 *      -- 30 * ( tenth of minute ) = 4096 ticks
 *      -- tenth of minute = 4096/30 ticks
 * 6 -- actual real-time milliseconds -- only for game speed setting
 *      -- no meaning in-game
 *
 * SPEED units:
 * 1 -- "internal" speed -- yards per tick.
 *      -- this is multiplied by delta_t, which is in ticks (in convoi_t::sync_step)
 *      -- to get distance travelled in yards (passed to vehikel_t:fahre_basis)
 * 2 -- km/h -- core setting here is VEHICLE_SPEED_FACTOR
 * 3 -- tiles per tick
 * 4 -- steps per tick
 *
 * ELECTRICITY units:
 * 1 -- "internal" units
 * 2 -- megawatts
 * This conversion still lives in dings/leitung2.h, not here
 */

#include "utils/float32e8_t.h"

/*
 * Distance units: conversion between "vehicle steps" and "yards"
 * In bitshift form
 */
#define YARDS_PER_VEHICLE_STEP_SHIFT (12)

/*
 * Mask, applied to yards, to eliminate anything smaller than a vehicle step
 * Assumes yards are in uint32 variables.... derivative of YARDS_PER_VEHICLE_STEP_SHIFT
 * #define YARDS_VEHICLE_STEP_MASK (0xFFFFF000)
 */
#define YARDS_VEHICLE_STEP_MASK ~((1<<YARDS_PER_VEHICLE_STEP_SHIFT)-1)

/*
 * Distance units: vehicle steps per tile
 * A vehicle travelling across a tile horizontally can be in this many
 * distinct locations along the tile
 * This is 256, and making it larger would require changing datatypes within
 * vehikel_t.
 */
#define VEHICLE_STEPS_PER_TILE (256)
#define VEHICLE_STEPS_PER_TILE_SHIFT 8

/*
 * Shift from yards to tiles, derived quantity
 */
#define YARDS_PER_TILE_SHIFT (VEHICLE_STEPS_PER_TILE_SHIFT + YARDS_PER_VEHICLE_STEP_SHIFT)

/*
 * Distance units: "carunits" per tile
 * Forced by history of pak files to 16
 */
#define CARUNITS_PER_TILE (16)

/*
 * Distance units: steps per carunit
 * Derived from the above two....
 */
#define VEHICLE_STEPS_PER_CARUNIT (VEHICLE_STEPS_PER_TILE/CARUNITS_PER_TILE)

/*
 * "Unlimited" speed depends on size of "speed" unit (sint32)
 */
#define SPEED_UNLIMITED (2147483647)    // == SINT32_MAX

/*
 * Global vehicle speed conversion factor between Simutrans speed
 * and km/h
 * @author Hj. Malthaner
 */
#define VEHICLE_SPEED_FACTOR  (80)

/**
 * Converts speed value to km/h
 * @author Hj. Matthaner
 * this is speed * VEHICLE_SPEED_FACTOR / 2^10 rounded to nearest
 */
#define speed_to_kmh(speed) (((speed)*VEHICLE_SPEED_FACTOR+511) >> 10)

/**
 * Converts km/h value to speed
 * @author Hj. Matthaner
 * this is speed * 2^10 /  VEHICLE_SPEED_FACTOR
 */
#define kmh_to_speed(speed) (((speed) << 10) / VEHICLE_SPEED_FACTOR)


// GEAR_FACTOR: a gear of 1.0 is stored as 64
#define GEAR_FACTOR 64

#define DT_TIME_FACTOR 64

#define WEIGHT_UNLIMITED ((std::numeric_limits<sint32>::max)())

// anything greater than 2097151 will give us overflow in kmh_to_speed. 
#define KMH_SPEED_UNLIMITED (300000)

/**
 * Conversion between km/h and m/s
 */

// scale to convert between km/h and m/s
static const float32e8_t kmh2ms((uint32) 10, (uint32) 36);
static const float32e8_t ms2kmh((uint32) 36, (uint32) 10);

/**
 * Conversion between simutrans speed and m/s
 */

// scale to convert between simutrans speed and m/s
const float32e8_t simspeed2ms((uint32) 10 * VEHICLE_SPEED_FACTOR, (uint32) 36 * 1024);
const float32e8_t ms2simspeed((uint32) 36 * 1024, (uint32) 10 * VEHICLE_SPEED_FACTOR);

inline float32e8_t speed_to_v(const sint32 speed)
{
	return simspeed2ms * speed;
}

inline sint32 v_to_speed(const float32e8_t &v)
{
	return (sint32)(ms2simspeed * v + float32e8_t::half);
}

/**
 * Conversion between simutrans steps and meters
 */

// scale to convert between simutrans speed and m/s
const float32e8_t yards2m((uint32) 10 * VEHICLE_SPEED_FACTOR, (uint32) 36 * 1024 * DT_TIME_FACTOR);
const float32e8_t m2yards((uint32) 36 * 1024 * DT_TIME_FACTOR, (uint32) 10 * VEHICLE_SPEED_FACTOR);
const float32e8_t steps2yards((uint32)1 << YARDS_PER_VEHICLE_STEP_SHIFT);

inline float32e8_t yards_to_x(const sint32 yards)
{
	return yards2m * yards;
}

inline sint32 x_to_yards(const float32e8_t &x)
{
	return (sint32)(m2yards * x + float32e8_t::half);
}

inline float32e8_t steps_to_x(const float32e8_t &simtime_factor, const sint32 steps)
{
	return steps2yards * yards2m * float32e8_t(steps) * simtime_factor;
}

inline sint32 x_to_steps(const float32e8_t &simtime_factor, const float32e8_t x)
{
	return x_to_yards(x / simtime_factor) >> YARDS_PER_VEHICLE_STEP_SHIFT;
}


#define KMH_MIN 4
static const sint32 SPEED_MIN = kmh_to_speed(KMH_MIN);
static const float32e8_t V_MIN = kmh2ms * KMH_MIN;

/*
 * Converts speed (yards per tick) into tiles per month
 */
// Done in simworld.h: speed_to_tiles_per_month

#endif /* simunits.h */

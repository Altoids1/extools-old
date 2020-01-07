#pragma once
#include "../core/core.h"
#include <cmath>


//DEFINES
#define MINIMUM_TRIT_OXYBURN_ENERGY 		2000000
#define TRITIUM_MINIMUM_RADIATION_ENERGY	0.1
#define TRITIUM_BURN_OXY_FACTOR				100
#define TRITIUM_BURN_TRIT_FACTOR			10
#define TRITIUM_BURN_RADIOACTIVITY_FACTOR	50000
//
#define FIRE_MINIMUM_TEMPERATURE_TO_EXIST	373.15
#define FIRE_HYDROGEN_ENERGY_RELEASED		560000
//
#define MINIMUM_HEAT_CAPACITY				0.0003
#define MINIMUM_MOLE_COUNT					0.01
//
#define CELL_VOLUME				2500	//liters in a cell
#define REACTING 1
#define NO_REACTION 0

//NAMESPACE STUFF
namespace altmos
{
	extern std::unordered_map<std::string, Value> AtmosTypes; // Holds the Value associated with the string variety of the type, for use in accessing gases[] mostly
	bool initialize();
}

//LISTMOS
//indices of values in gas lists.
/*
You might ask yourself: "Why are these enums decremented from the values they have originally in the DM code?"
That's because the current implementation of Container.at() increments integer inputs, trying to convert 0-indexed array usage into 1-indexed.
This does not affect iteration much, but accessing of specific values by specific indices, like thru these enums, breaks as a result.

This is the dumbfix while that gets sorted.
*/
enum Listmos : unsigned int {
	MOLES = 0,
	ARCHIVE = 1,
	GAS_META = 2,
	META_GAS_SPECIFIC_HEAT = 0,
	META_GAS_NAME = 1,
	META_GAS_MOLES_VISIBLE = 2,
	META_GAS_OVERLAY = 3,
	META_GAS_DANGER = 4,
	META_GAS_ID = 5,
	META_GAS_FUSION_POWER = 6
};

trvh TritiumReact(unsigned int n_args, Value* args, Value src); // Hook of /datum/gas_reaction/tritfire/react(datum/gas_mixture/air, datum/holder)
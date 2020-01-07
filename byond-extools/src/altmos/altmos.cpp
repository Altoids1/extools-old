#include "altmos.h"

namespace altmos
{
    std::unordered_map<std::string, Value> AtmosTypes;
    bool initialize()
    {
        //Set up gas types map
        std::vector<Value> nullvector = { Value(0.0f) };
        Container gas_types = Core::get_proc("/proc/gas_types").call(nullvector);
        int gaslen = gas_types.length();
        for (int i = 0; i < gaslen; ++i)
        {
            AtmosTypes[Core::stringify(gas_types.at(i))] = gas_types.at(i);
        }
        //Set up hooks
        Core::get_proc("/datum/gas_reaction/tritfire/proc/react", 0).hook(TritiumReact);
        return true;
    }

}

float heat_capacity(Value gas_mixture) // Cousin of /datum/gas_mixture/proc/heat_capacity(data = MOLES) 
{
    Container cached_gases = gas_mixture.get("gases");
    float Dot = 0;
    int cached_gases_size = cached_gases.length();
    for (int i = 0; i < cached_gases_size; i++)
    {
        Container gas_data = cached_gases.at(cached_gases.at(i));
        Container gas_meta = gas_data[Listmos::GAS_META];
        Dot += Value(gas_data[Listmos::MOLES]).valuef * Value(gas_meta[Listmos::META_GAS_SPECIFIC_HEAT]).valuef;
    }
    return Dot;
}
trvh TritiumReact(unsigned int n_args, Value* args, Value src) // Hook of /datum/gas_reaction/tritfire/react(datum/gas_mixture/air, datum/holder)
{
	trvh Dot;
    Value air = args[0];
    Value holder = args[1];
    //
    if (Core::stringify(air.get("type")).find("/datum/gas_mixture") == std::string::npos)
    {
        Core::Alert("Weird type for air: " + Core::stringify(air.get("type")));
    }
    //
	float energy_released = 0;
    float old_heat_capacity = heat_capacity(air);
    Container cached_gases = air.get("gases"); //Caching speeds things up, still, I think?
    float temperature = air.get("temperature").valuef;
    Value location = (holder.type == 0x01) ? holder : Value::Null(); // Basically isturf() ? holder : null
    Container cached_results = air.get("reaction_results");
    cached_results["fire"] = Value(0.0f);
    float burned_fuel = 0;
    Container cached_trit = cached_gases[altmos::AtmosTypes["/datum/gas/tritium"]];
    Container cached_oxy = cached_gases[altmos::AtmosTypes["/datum/gas/oxygen"]];
    float initial_trit = Value(cached_trit[Listmos::MOLES]).valuef;
    if (Value(cached_oxy[Listmos::MOLES]).valuef < initial_trit || MINIMUM_TRIT_OXYBURN_ENERGY > (temperature * old_heat_capacity))
    {
        burned_fuel = Value(cached_oxy[Listmos::MOLES]).valuef / TRITIUM_BURN_OXY_FACTOR;
        if (burned_fuel > initial_trit) burned_fuel = initial_trit; // Prevents negative moles of Tritium
        cached_trit[Listmos::MOLES] = Value(cached_trit[Listmos::MOLES]).valuef - burned_fuel;
    }
    else
    {
        burned_fuel = initial_trit;
        cached_trit[Listmos::MOLES] = Value(cached_trit[Listmos::MOLES]).valuef * (1 - 1 / TRITIUM_BURN_TRIT_FACTOR);
        cached_oxy[Listmos::MOLES] = Value(cached_oxy[Listmos::MOLES]).valuef - Value(cached_trit[Listmos::MOLES]).valuef;
        energy_released += (FIRE_HYDROGEN_ENERGY_RELEASED * burned_fuel * (TRITIUM_BURN_TRIT_FACTOR - 1));
    }
    if (burned_fuel != 0)
    {
        energy_released += (FIRE_HYDROGEN_ENERGY_RELEASED * burned_fuel);
        if ((location != Value::Null()) && (rand() % 10 == 0) && burned_fuel > TRITIUM_MINIMUM_RADIATION_ENERGY)
        {
            Core::Proc radiation_pulse = Core::get_proc("/proc/radiation_pulse",0);
            std::vector<Value> radargs = {location, energy_released/TRITIUM_BURN_RADIOACTIVITY_FACTOR};
            radiation_pulse.call(radargs, Value::Null(), src);
        }
        //ASSERT_GAS(/datum/gas/water_vapor, air) //oxygen+more-or-less hydrogen=H2O
        //cached_gases[/datum/gas/water_vapor][MOLES] += burned_fuel // Yogs -- Conservation of Mass
        cached_results["fire"] = burned_fuel;
    }
    if (energy_released > 0)
    {
        float new_heat_capacity = heat_capacity(air);
        if (new_heat_capacity > MINIMUM_HEAT_CAPACITY)
        {
            air.set("temperature", (temperature * old_heat_capacity + energy_released) / new_heat_capacity);
        }
    }
    if (location != Value::Null())
    {
        temperature = air.get("temperature");
        if (temperature > FIRE_MINIMUM_TEMPERATURE_TO_EXIST)
        {
            std::vector<Value> nargs = { temperature, CELL_VOLUME };
            Core::get_proc("/turf/open/proc/hotspot_expose", 0).call(nargs,Value::Null(),location);
            nargs = { air, temperature, CELL_VOLUME };
            Container contents = Core::get_proc("/proc/safe_contents").call({location},Value::Null(),src);
            int container_size = contents.length();
            for (int i = 0; i < container_size; ++i)
            {
                Core::get_proc(Core::stringify(Value(contents[i]).get("type")) + "/proc/temperature_expose", 0).call(nargs, Value::Null(), contents[i]); // Mac save me please
            }
            Core::get_proc(Core::stringify(Value(location).get("type")) + "/proc/temperature_expose", 0).call(nargs, Value::Null(), location); // Mac save me please
        }
    }
    Dot.type = 0x2A; // Number
    Dot.valuef = (Value(cached_results["fire"]).valuef != 0) ? REACTING : NO_REACTION;
    return Dot;
}
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

void assert_gas(Value gas_id, ManagedValue gases_val, Value src) // cousin of #define ASSERT_GAS()
{
    Container gases = Container(gases_val);
    if (!Value(gases[gas_id]))
    {
        IncRefCount(gases_val.type, gases_val.value);
        Core::get_proc("/proc/add_gas").call({ gas_id, gases_val });
    }
}

float heat_capacity(Container cached_gases) // Cousin of /datum/gas_mixture/proc/heat_capacity(data = MOLES) 
{
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
    Dot.type = 0x2A; // Number
    if (!n_args)
    {
        Dot.valuef = NO_REACTION;
        return Dot;
    }
    ManagedValue air = args[0];
    ManagedValue holder = (n_args > 1) ? args[1] : Value::Null();
    /*
    if (Core::stringify(air.get("type")).find("/datum/gas_mixture") == std::string::npos)
    {
        Core::Alert("Weird type for air: " + Core::stringify(air.get("type")));
    }
    */
	float energy_released = 0;
    
    ManagedValue cached_gases_val = air.get("gases");
    Container cached_gases = Container(cached_gases_val); //Caching speeds things up, still, I think?
    float old_heat_capacity = heat_capacity(cached_gases);
    float temperature = air.get("temperature").valuef;
    ManagedValue location = (holder.type == 0x01) ? holder : Value::Null(); // Basically isturf() ? holder : null
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
        cached_trit[Listmos::MOLES] = Value(cached_trit[Listmos::MOLES]).valuef * (1 - 1.0f / TRITIUM_BURN_TRIT_FACTOR);
        cached_oxy[Listmos::MOLES] = Value(cached_oxy[Listmos::MOLES]).valuef - Value(cached_trit[Listmos::MOLES]).valuef;
        energy_released += (FIRE_HYDROGEN_ENERGY_RELEASED * burned_fuel * (TRITIUM_BURN_TRIT_FACTOR - 1));
    }
    if (burned_fuel != 0)
    {
        energy_released += (FIRE_HYDROGEN_ENERGY_RELEASED * burned_fuel);
        if (!ISNULL(location) && (rand() % 10 == 0) && burned_fuel > TRITIUM_MINIMUM_RADIATION_ENERGY)
        {
            std::vector<Value> radargs = {location, energy_released/TRITIUM_BURN_RADIOACTIVITY_FACTOR};
            Core::get_proc("/proc/radiation_pulse").call(radargs);
        }
        assert_gas(altmos::AtmosTypes["/datum/gas/water_vapor"], cached_gases_val, src);
        Container cached_vapor = cached_gases[altmos::AtmosTypes["/datum/gas/water_vapor"]];
        cached_vapor[Listmos::MOLES] = float(Value(cached_vapor[Listmos::MOLES])) + burned_fuel;
        cached_results["fire"] = burned_fuel;
    }
    if (energy_released > 0)
    {
        float new_heat_capacity = heat_capacity(cached_gases);
        if (new_heat_capacity > MINIMUM_HEAT_CAPACITY)
        {
            temperature = (temperature * old_heat_capacity + energy_released) / new_heat_capacity;
            air.set("temperature", Value(temperature));
        }
    }
    if (!ISNULL(location))
    {
        if (temperature > FIRE_MINIMUM_TEMPERATURE_TO_EXIST)
        {
            location.invoke("hotspot_expose", { temperature, CELL_VOLUME });
           
            Container contents = Core::get_proc("/proc/safe_contents").call({location});
            int container_size = contents.length();
            for (int i = 0; i < container_size; ++i)
            {
                ManagedValue x = contents[i];
                IncRefCount(air.type, air.value);
                x.invoke("temperature_expose", { air, temperature, CELL_VOLUME });
                //DecRefCount(air.type, air.value);
            }
            location.invoke("temperature_expose", {temperature, CELL_VOLUME});
        }
    }
    Dot.valuef = (Value(cached_results["fire"]).valuef != 0) ? REACTING : NO_REACTION;
    //IncRefCount(air.type, air.value);
    return Dot;
}

#undef ISNULL
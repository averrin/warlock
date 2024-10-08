return {
    name = "Core",
    description = "Computation core",
    attributes = {
        type = {
            title = "Type",
            description = "",
            type = AttributeType.STRING,
            value = "Core"
        },
        code = {
            title = "Code",
            description = "Execution code",
            type = AttributeType.STRING,
            value = [[
return {
  start = function()
    charger = frame:getComponentByType("Charger");
    bat = frame:getComponentByType("Battery");
    bat:activate();
    charger.api.setTarget(charger, bat);
  end,
  update = function()
    meter = frame:getComponentByType("Power Meter");
    thermo = frame:getComponentByType("Temperature Sensor");
    info = meter.api.getPowerInfo(meter);
    charger = frame:getComponentByType("Charger");
    bat = frame:getComponentByType("Battery");
    if bat.api.getCharge(bat) <= bat.api.getCapacity(bat)
      and info.production - info.consumption > charger.api.getConsumption(charger)
      and thermo.api.getComponentTemperature(bat) < 80 then
      charger:activate();
    else
      charger:deactivate();
    end
  end,
  idle = function()
  end,
  stop = function()
  end,
}
              ]]
        },
        consumption = {
            title = "Consumption",
            description = "Power consumption",
            type = AttributeType.FLOAT,
            value = 5.0,
        },
    },
    state = ComponentState.DEACTIVATED,
    size = ComponentSize.S,
  api = {},
}

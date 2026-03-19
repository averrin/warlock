return {
	name = "Smart Battery",
	size = FrameSize.M,
	components = {
		"Core",
		"Battery",
		"Charger",
		"Power Meter",
		"Temperature Sensor",
		"Power Wire Connector",
	},
	code = [[
return {
  start = function()
    print("start");
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
              ]],
}

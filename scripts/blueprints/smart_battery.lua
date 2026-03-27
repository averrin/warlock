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
    charger = locator(frame, ".Charger");
    bat = locator(frame, ".Battery");
    bat.activate();
    charger.api.setTarget(charger, bat);
  end,
  update = function()
    meter = locator(frame, ".Power Meter");
    thermo = locator(frame, ".Temperature Sensor");
    info = meter.api.getPowerInfo(meter);
    charger = locator(frame, ".Charger");
    bat = locator(frame, ".Battery");
    if bat.api.getCharge(bat) < bat.api.getCapacity(bat)
      and info.production - info.consumption > charger.api.getConsumption(charger)
      and thermo.api.getComponentTemperature(bat) < 80 then
      charger.activate();
    else
      charger.deactivate();
    end
  end,
  idle = function()
  end,
  stop = function()
  end,
}
              ]],
}

return {
	name = "Thermal Management",
	description = "Enables active temperature regulation to prevent overheating and freezing.",
	icon = "thermometer-hot.png",
	cost = { ["Electronic Parts"] = 30 },
	requires = { "Basic Power" },
	unlocks = {
		"Cooler", "Heater", "Large Copper Heat Sink",
		"Temperature Sensor", "Life Support",
		"External AO Cooler", "External AO Heater",
	},
	unlocks_recipes = {},
}

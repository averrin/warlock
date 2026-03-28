return {
	name = "Wireless Systems",
	description = "Enables wireless power and data transmission between frames.",
	icon = "radio-tower.png",
	cost = { ["Electronic Parts"] = 60, ["Advanced Chips"] = 15 },
	requires = { "Power Generation", "Data Networking" },
	unlocks = {
		"Power Wireless Emitter", "Power Wireless Receiver",
		"Data Wireless Emitter", "Data Wireless Receiver",
		"Near Field Communicator",
	},
	unlocks_recipes = {},
}

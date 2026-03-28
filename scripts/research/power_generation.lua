return {
	name = "Power Generation",
	description = "Unlocks active and passive energy generation systems.",
	icon = "lightning-bolt.png",
	cost = { ["Electronic Parts"] = 50, ["Advanced Chips"] = 10 },
	requires = { "Basic Power" },
	unlocks = { "Generator", "Solar Panel" },
	unlocks_recipes = { "Spark Ore", "Consume Spark Ore", "Consume Spark Stone" },
}

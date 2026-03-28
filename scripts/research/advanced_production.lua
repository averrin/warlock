return {
	name = "Advanced Production",
	description = "Enables resource extraction, refining, large-scale storage, and item packing.",
	icon = "factory.png",
	cost = { ["Electronic Parts"] = 50, ["Advanced Chips"] = 20 },
	requires = { "Basic Power", "Data Networking" },
	unlocks = { "Miner", "Refinery", "Big Storage", "Packer" },
	unlocks_recipes = {
		"Spark Stone",
		"Iron Ore", "Copper Ore",
		"Enriched Iron Ore", "Enriched Iron Ore (Catalytic)",
		"Iron Ingot", "Copper Ingot",
		"Frame Parts", "Electronic Parts",
		"Compacted Carbon Dust", "UnCompacted Carbon Dust",
	},
}

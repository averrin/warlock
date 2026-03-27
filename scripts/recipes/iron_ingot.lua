return {
	name = "Iron Ingot",
	description = "Smelt Enriched Iron Ore",
	inputs = {
		{
			name = "Enriched Iron Ore",
			amount = 2,
		},
	},
	outputs = {
		{
			name = "Iron Ingot",
			amount = 1,
		},
		{
			name = "Iron Slag",
			amount = 1,
		},
	},
	timeCost = 4.0,
	powerCost = 350.0,
	available = { "Refinery" },
}

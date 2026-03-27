return {
	name = "Enriched Iron Ore (Catalytic)",
	description = "Catalytic enrichment using slag",
	inputs = {
		{
			name = "Iron Ore",
			amount = 2,
		},
		{
			name = "Iron Slag",
			amount = 1,
		},
	},
	outputs = {
		{
			name = "Enriched Iron Ore",
			amount = 2,
		},
	},
	timeCost = 2.0,
	powerCost = 250.0,
	available = { "Refinery" },
}

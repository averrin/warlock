return {
	name = "Mass Production",
	description = "Accumulate 500 Electronic Parts through mining and refining.",
	icon = "microchip.png",
	category = "production",
	prerequisites = { "first_power" },
	conditions = {
		{ type = "spendable_gte", name = "Electronic Parts", amount = 500 },
	},
	rewards = {
		{ type = "spendable", name = "Advanced Chips", amount = 50 },
		{ type = "toast", message = "Objective complete: Mass Production!" },
	},
}

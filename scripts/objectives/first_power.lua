return {
	name = "Power Up",
	description = "Unlock Basic Power research to enable energy storage and consumption management.",
	icon = "battery-100.png",
	category = "tutorial",
	prerequisites = {},
	conditions = {
		{ type = "research_unlocked", name = "Basic Power" },
	},
	rewards = {
		{ type = "spendable", name = "Electronic Parts", amount = 20 },
		{ type = "toast", message = "Objective complete: Power Up!" },
	},
}

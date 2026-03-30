return {
	name = "Network Architect",
	description = "Unlock Data Networking research and establish data communication between frames.",
	icon = "data-relay.png",
	category = "tutorial",
	prerequisites = {},
	conditions = {
		{ type = "research_unlocked", name = "Data Networking" },
	},
	rewards = {
		{ type = "spendable", name = "Electronic Parts", amount = 30 },
		{ type = "toast", message = "Objective complete: Network Architect!" },
	},
}

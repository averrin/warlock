return {
	name = "Expansion",
	description = "Have at least 3 frames in operation.",
	icon = "frames.png",
	category = "expansion",
	prerequisites = {},
	conditions = {
		function()
			-- Lua function conditions have full access to the game API.
			-- This checks whether there are at least 3 Frame entities.
			local count = 0
			-- Use frame_count_gte declarative condition for simple checks,
			-- or a function like this for complex logic.
			return false -- placeholder: replace with actual game state query
		end,
	},
	rewards = {
		{ type = "spendable", name = "Frame Parts", amount = 50 },
		{ type = "toast", message = "Objective complete: Expansion!" },
	},
}

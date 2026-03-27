return {
	name = "Advanced Core",
	category = "Core",
	description = "Hardened computation core with persistent memory and library support",
	icon = "microchip.png",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Core",
		},
		code = {
			title = "Code",
			description = "Execution code",
			type = AttributeType.STRING,
			inspector = { widget = "code" },
			value = [[
return {
  start = function()
  end,
  update = function()
  end,
  onStateChange = function(comp_id, comp_name, prev, new_state, reason)
  end,
  idle = function()
  end,
  stop = function()
  end,
}
              ]],
		},
		memory = {
			title = "Memory",
			description = "Persistent key-value storage (survives save/load)",
			type = AttributeType.STRING,
			inspector = { widget = "memory" },
			value = "{}",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 15.0,
		},
		stable = {
			title = "Stable",
			description = "If browned out, automatically starts again when the grid can supply it",
			type = AttributeType.BOOL,
			value = true,
		},
		temp_proof = {
			title = "Temp Proof",
			description = "Does not overheat or freeze",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.M,
	spendable_cost = { ["Electronic Parts"] = 25 },
	require = { "Power Wire Connector" },
	api = {},
}

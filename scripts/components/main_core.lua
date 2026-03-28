return {
	name = "Main Core",
	category = "Unique",
	description = "Computation core",
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
  idle = function()
  end,
  stop = function()
  end,
}
              ]],
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 5.0,
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
	state = ComponentState.ACTIVATED,
	size = ComponentSize.S,
	spendable_cost = { ["Electronic Parts"] = 10 },
	api = {},
}

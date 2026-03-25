return {
	name = "Core",
	category = "Core",
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
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.S,
	api = {},
}

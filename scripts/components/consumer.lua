return {
    name = "Consumer",
    description = "Generic consumer",
    attributes = {
        type = {
            title = "Type",
            description = "",
            type = AttributeType.STRING,
            value = "Consumer"
        },
        efficiency = {
            title = "Efficiency",
            description = "Component efficiency",
            type = AttributeType.FLOAT,
            value = 1.0,
        },
        consumption = {
            title = "Consumption",
            description = "Power consumption",
            type = AttributeType.FLOAT,
            value = 940.0,
            easing = {
                type = AttributeEasingType.SIN,
                range = 0.1,
                period = 650.0
            }
        },
    },
    state = ComponentState.DEACTIVATED,
    size = ComponentSize.M,
  api = {},
}

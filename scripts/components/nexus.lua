return {
    name = "Nexus",
    description = "Starting component",
    attributes = {
        type = {
            title = "Type",
            description = "",
            type = AttributeType.STRING,
            value = "Nexus"
        },
        stable = {
            title = "Stable",
            description = "Does continue work after power loss",
            type = AttributeType.BOOL,
            value = true
        },
        production = {
            title = "Production",
            description = "Power production",
            type = AttributeType.FLOAT,
            value = 1000.0,
            easing = {
                type = AttributeEasingType.JITTER,
                range = 0.05,
                period = 1000.0
            }
        },
    },
    state = ComponentState.ACTIVE,
    size = ComponentSize.L,
  api = {},
}

return {
    name = "Large Copper Heat Sink",
    category = "Thermal",
    description = "Large Copper Heat Sink",
    attributes = {
        type = {
            title = "Type",
            description = "",
            type = AttributeType.STRING,
            value = "Heat Sink"
        },
        passive = {
            title = "Passive",
            description = "Cannot change state",
            type = AttributeType.BOOL,
            value = true
        },
    },
    state = ComponentState.ACTIVE,
    size = ComponentSize.L,
    material = ComponentMaterial.COPPER,
    api = {
    },
}


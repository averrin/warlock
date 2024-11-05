#include <IconsFontAwesome6.h>
#include <editors/helpers.hpp>

const char *AttributeEasingTypeToString(AttributeEasingType type) {
  switch (type) {
  case AttributeEasingType::NONE:
    return "None";
  case AttributeEasingType::JITTER:
    return "Jitter";
  case AttributeEasingType::SAW:
    return "Saw";
  case AttributeEasingType::SIN:
    return "Sin";
  case AttributeEasingType::RANDOM_STEP:
    return "Random Step";
  default:
    return "Unknown";
  }
}

const char *ComponentMaterialToString(ComponentMaterial size) {
  switch (size) {
  case ComponentMaterial::ALUMINIUM:
    return "Aluminium";
  case ComponentMaterial::COPPER:
    return "Copper";
  case ComponentMaterial::STEEL:
    return "Steel";
  case ComponentMaterial::TITANIUM:
    return "Titanium";
  case ComponentMaterial::PLASTIC:
    return "Plastic";
  case ComponentMaterial::GLASS:
    return "Glass";
  default:
    return "Unknown";
  }
}

const char *ComponentSizeToString(ComponentSize size) {
  switch (size) {
  case ComponentSize::S:
    return "Small";
  case ComponentSize::M:
    return "Medium";
  case ComponentSize::L:
    return "Large";
  default:
    return "Unknown";
  }
}

const char *FrameSizeToString(FrameSize size) {
  switch (size) {
  case FrameSize::S:
    return "Small";
  case FrameSize::M:
    return "Medium";
  case FrameSize::L:
    return "Large";
  case FrameSize::G:
    return "Giant";
  default:
    return "Unknown";
  }
}

const char *ComponentStateToString(ComponentState state) {
  switch (state) {
  case ComponentState::DEACTIVATED:
    return "Deactivated";
  case ComponentState::ACTIVATING:
    return "Activating";
  case ComponentState::ACTIVE:
    return "Active";
  case ComponentState::DEACTIVATING:
    return "Deactivating";
  case ComponentState::ERROR:
    return "Error";
  case ComponentState::DESTROYED:
    return "Destroyed";
  case ComponentState::BLOCKED:
    return "Blocked";
  case ComponentState::BROKEN:
    return "Broken";
  default:
    return "Unknown";
  }
}

const char *ComponentStateToIcon(ComponentState state) {
  switch (state) {
  case ComponentState::DEACTIVATED:
    return ICON_FA_STOP;
  case ComponentState::ACTIVATING:
    return ICON_FA_GEARS;
  case ComponentState::ACTIVE:
    return ICON_FA_CHECK;
  case ComponentState::DEACTIVATING:
    return ICON_FA_GEARS;
  case ComponentState::ERROR:
    return ICON_FA_TRIANGLE_EXCLAMATION;
  case ComponentState::DESTROYED:
    return ICON_FA_SKULL_CROSSBONES;
  case ComponentState::BROKEN:
    return ICON_FA_SKULL_CROSSBONES;
  case ComponentState::BLOCKED:
    return ICON_FA_BAN;
  default:
    return ICON_FA_QUESTION;
  }
}

#pragma once

#include <string>
#include <utils/entt.hpp>

/** Removes connections where this frame is an endpoint and the connection requires the given connector `type` attribute. */
void destroyConnectionsUsingConnectorType(entt::registry &registry, int frameDataId,
                                           const std::string &componentTypeAttr);

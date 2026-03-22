#include <game/connection_cleanup.hpp>
#include <game/components/frame.hpp>

#include <optional>
#include <string>
#include <vector>

static std::optional<std::string> expectedConnectorTypeAttr(const Connection &conn) {
  if (conn.medium == ConnectionMedium::WIRELESS) {
    return std::nullopt; // Wireless connections are handled by the auto-connection system
  }
  switch (conn.type) {
  case ConnectionType::POWER:
    return std::string("Power Wire Connector");
  case ConnectionType::DATA:
    return std::string("Data Connector");
  case ConnectionType::CONVEYOR:
    return std::string("Conveyor Connector");
  default:
    return std::nullopt;
  }
}

void destroyConnectionsUsingConnectorType(entt::registry &registry, int frameDataId,
                                          const std::string &componentTypeAttr) {
  if (componentTypeAttr.empty())
    return;

  std::vector<entt::entity> toDestroy;
  auto view = registry.view<Connection>();
  for (auto e : view) {
    auto &c = view.get<Connection>(e);
    if (c.source != frameDataId && c.target != frameDataId)
      continue;
    auto expected = expectedConnectorTypeAttr(c);
    if (expected && *expected == componentTypeAttr)
      toDestroy.push_back(e);
  }
  for (auto e : toDestroy)
    registry.destroy(e);
}

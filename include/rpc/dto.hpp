#pragma once
#include <nlohmann/json.hpp>
#include <game/components/frame.hpp>
#include <game/systems/power.hpp>
#include <utils/entt_draw.hpp>
#include <entt/entt.hpp>

namespace rpc {

nlohmann::json serializeAttributeValue(const AttributeValue& val);
nlohmann::json serializeAttribute(const std::string& key, const Attribute& attr);
nlohmann::json serializeMetadata(const Metadata& data);
nlohmann::json serializeComponent(const Component& comp);
nlohmann::json serializeConnection(const Connection& conn);
nlohmann::json serializeConnection(entt::entity entity, const Connection& conn);
nlohmann::json serializeFrame(const Frame& frame);
nlohmann::json serializeFrame(entt::registry& registry, entt::entity entity, const Frame& frame);
nlohmann::json serializeFrameSummary(entt::entity entity, const Frame& frame);
nlohmann::json serializeCanvasBadges(const Frame& frame, const PowerInfo* powerInfo);
nlohmann::json serializeEnvironment(const Environment& env);

} // namespace rpc

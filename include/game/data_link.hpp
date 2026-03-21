#pragma once

#include <game/components/frame.hpp>
#include <cstddef>
#include <memory>
#include <string>

inline constexpr std::size_t kMaxDataQueue = 256;

/// Recompute Data Connector counterpart pairing from DATA connections (stable ordering).
void recompute_data_link_counterparts();

std::shared_ptr<Component> find_component_by_id(int component_id);

void data_link_send_raw(std::shared_ptr<Component> from, std::string payload);

void data_link_send_packet(std::shared_ptr<Component> from, DataPacket packet);

/// Push into a connector's local inbox (same-frame switching / hub flood); does not use wire.
void data_link_inject_raw(std::shared_ptr<Component> target, std::string payload);

void data_link_inject_packet(std::shared_ptr<Component> target, DataPacket packet);

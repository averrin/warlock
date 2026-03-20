#include <game/data_link.hpp>
#include <game/state.hpp>

#include <algorithm>
#include <deque>
#include <map>
#include <vector>

namespace {

constexpr std::size_t kMaxDataQueue = 256;

bool is_data_connector(const std::shared_ptr<Component> &c) {
  if (!c)
    return false;
  return c->data.get<std::string>("type") == "Data Connector";
}

bool is_data_relay(const std::shared_ptr<Component> &c) {
  return c && c->data.name == "Data Relay";
}

void link_one_raw(Component *x, int peer_comp_id) {
  if (!x)
    return;
  if (x->data.name == "Data Relay") {
    if (x->counterpart_id < 0)
      x->counterpart_id = peer_comp_id;
    else
      x->counterpart_id_alt = peer_comp_id;
  } else {
    x->counterpart_id = peer_comp_id;
  }
}

void link_pair(const std::shared_ptr<Component> &a, const std::shared_ptr<Component> &b) {
  if (!a || !b)
    return;
  link_one_raw(a.get(), b->data.id);
  link_one_raw(b.get(), a->data.id);
}

} // namespace

std::shared_ptr<Component> find_component_by_id(int component_id) {
  auto &current_state = entt::locator<State>::value();
  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    for (auto &c : frame.components) {
      if (c->data.id == component_id)
        return c;
    }
  }
  return nullptr;
}

void recompute_data_link_counterparts() {
  auto &current_state = entt::locator<State>::value();
  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    for (auto &c : frame.components) {
      if (is_data_connector(c)) {
        c->counterpart_id = -1;
        c->counterpart_id_alt = -1;
      }
    }
  }

  std::vector<Connection> conns;
  for (auto &e : current_state.registry.view<Connection>()) {
    auto &c = current_state.registry.get<Connection>(e);
    if (c.type != ConnectionType::DATA)
      continue;
    conns.push_back(c);
  }
  std::sort(conns.begin(), conns.end(),
            [](const Connection &a, const Connection &b) {
              return a.data.id < b.data.id;
            });

  std::map<int, std::deque<std::shared_ptr<Component>>> free_by_frame;
  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    std::vector<std::shared_ptr<Component>> ports;
    for (auto &c : frame.components) {
      if (is_data_connector(c))
        ports.push_back(c);
    }
    std::sort(ports.begin(), ports.end(),
              [](const std::shared_ptr<Component> &a,
                 const std::shared_ptr<Component> &b) {
                return a->data.id < b->data.id;
              });
    for (auto &p : ports) {
      free_by_frame[frame.data.id].push_back(p);
      if (is_data_relay(p))
        free_by_frame[frame.data.id].push_back(p);
    }
  }

  for (const auto &conn : conns) {
    auto &a = free_by_frame[conn.source];
    auto &b = free_by_frame[conn.target];
    if (a.empty() || b.empty())
      continue;
    auto ca = a.front();
    auto cb = b.front();
    a.pop_front();
    b.pop_front();
    link_pair(ca, cb);
  }
}

static std::shared_ptr<Component>
resolve_data_hop(const std::shared_ptr<Component> &from,
                 const std::shared_ptr<Component> &peer) {
  if (!from || !peer || !is_data_connector(peer))
    return nullptr;
  if (!is_data_relay(peer))
    return peer;
  int dest_id = -1;
  if (from->data.id == peer->counterpart_id && peer->counterpart_id_alt >= 0)
    dest_id = peer->counterpart_id_alt;
  else if (from->data.id == peer->counterpart_id_alt && peer->counterpart_id >= 0)
    dest_id = peer->counterpart_id;
  else if (peer->counterpart_id_alt >= 0)
    dest_id = peer->counterpart_id_alt;
  else
    dest_id = peer->counterpart_id;
  return find_component_by_id(dest_id);
}

void data_link_send_raw(std::shared_ptr<Component> from, std::string payload) {
  if (!from || !is_data_connector(from))
    return;
  if (from->counterpart_id < 0)
    return;
  auto peer = find_component_by_id(from->counterpart_id);
  auto dest = resolve_data_hop(from, peer);
  if (!dest || !is_data_connector(dest))
    return;
  if (dest->data_raw_inbox.size() >= kMaxDataQueue)
    return;
  dest->data_raw_inbox.push_back(std::move(payload));
}

void data_link_send_packet(std::shared_ptr<Component> from, DataPacket packet) {
  if (!from || !is_data_connector(from))
    return;
  int target_id = packet.destination;
  if (target_id < 0)
    target_id = from->counterpart_id;
  if (target_id < 0)
    return;
  auto target = find_component_by_id(target_id);
  if (!target || !is_data_connector(target))
    return;
  auto from_for_hop = from;
  auto dest = resolve_data_hop(from_for_hop, target);
  if (!dest || !is_data_connector(dest))
    return;
  if (dest->data_packet_inbox.size() >= kMaxDataQueue)
    return;
  if (packet.source < 0)
    packet.source = from->data.id;
  dest->data_packet_inbox.push_back(std::move(packet));
}

void data_link_inject_raw(std::shared_ptr<Component> target, std::string payload) {
  if (!target || !is_data_connector(target))
    return;
  if (target->data_raw_inbox.size() >= kMaxDataQueue)
    return;
  target->data_raw_inbox.push_back(std::move(payload));
}

void data_link_inject_packet(std::shared_ptr<Component> target, DataPacket packet) {
  if (!target || !is_data_connector(target))
    return;
  if (target->data_packet_inbox.size() >= kMaxDataQueue)
    return;
  target->data_packet_inbox.push_back(std::move(packet));
}

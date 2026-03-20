# Data network (Option A) — design

## Scope

- Hub flooding between ports on a **Data Switch** frame using `injectRaw` / `injectPacket` (same-frame switching).
- Wire egress uses `sendRaw` / `send` to the **counterpart** connector (paired from `DATA` connections).
- Optional **tap** role: set the **Tap (mirror only)** attribute on a `Data Wire Connector`; switch cores treat it as mirror egress only (no ingress flooding).
- **Out of scope:** learning bridges, routers, firewalls, VLAN enforcement in stock blueprints (players can implement in Lua).

## Wire vs local inject

| API | Behavior |
|-----|----------|
| `sendRaw(str)` | Enqueues raw string on the **counterpart** connector’s inbox (one hop on the wire). |
| `send{ source, destination?, headers, body }` | Enqueues structured packet on **destination** inbox if set and valid; else on **counterpart**. |
| `injectRaw(str)` | Pushes to **this** connector’s local inbox (used by switch core to flood sibling ports on the same frame). |
| `injectPacket{ ... }` | Same for structured packets. |
| `readRaw()` / `read()` | Pop from local inbox (nil when empty). |

Counterpart ids are recomputed each tick before Core `update` from all `ConnectionType::DATA` links, ordered by connection id; connectors on each frame are sorted by component id and paired greedily per connection.

## Optional header conventions (player / future routers)

Suggested string keys in `headers` (not interpreted by stock hub):

| Key | Role |
|-----|------|
| `src` | Sender id / logical address |
| `dst` | Destination id / broadcast marker |
| `type` | Payload / protocol label (EtherType-like) |
| `ttl` | Hop limit for future routers |
| `vlan` | Optional segmentation tag |

## Counters (stock switch cores)

Per **port** index (sorted data connectors with `tap` false): `counters.rx[i]`, `counters.tx[i]`, `max_queue_seen[i]`. Drain cap `DRAIN_CAP = 64` items per port per tick (raw tries first, then packet queue).

## Blueprints

- **Data Switch** — Core + 4× Data Wire Connector. Enable **Tap (mirror only)** on any wire connector (add more connectors if needed) for one or more mirror ports.

## Limits

- Per-inbox cap: 256 items (raw + packet queues separate).
- Inbox contents are **not** persisted in save files (cleared on load); counterpart is recomputed after load.

## Possible improvements

- Connection metadata binding specific connector instances when multiple `DATA` edges exist between the same two frames.
- Persist queues or expose counters via Core attributes for UI.
- Per-tick fairness between raw and packet queues.

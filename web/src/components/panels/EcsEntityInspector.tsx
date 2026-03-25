import { useCallback, useEffect, useMemo, useState, type MouseEvent } from "react";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO, EcsEntityDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { useConnectionStore } from "../../stores/connection";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { CollapsibleSection, NumberField, TextField, smallBtnStyle } from "../ui";

const ECS_REGISTRY_COMPONENT_OPTIONS = [
  "meta",
  "ineditor",
  "tags",
  "player",
  "obstacle",
  "creature",
  "script",
  "Environment",
  "transform",
  "relation",
  "proto",
] as const;

const NON_REMOVABLE_ECS = new Set(["Frame", "Connection", "ResourcePatch", "Environment"]);

type FieldEditorProps = {
  entityId: number;
  component: string;
  field: string;
  value: unknown;
  rpcClient: RpcClient;
};

function FieldEditor({ entityId, component, field, value, rpcClient }: FieldEditorProps) {
  const setEntityField = useGameStore((s) => s.setEntityField);
  const handleChange = useCallback(
    (v: unknown) => void setEntityField(rpcClient, entityId, component, field, v),
    [rpcClient, entityId, component, field, setEntityField],
  );

  if (typeof value === "boolean") {
    return (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
        <span style={{ minWidth: 90, color: "#9ca3af", fontSize: 11 }}>{field}:</span>
        <input
          type="checkbox"
          checked={value}
          onChange={(e) => handleChange(e.target.checked)}
        />
      </div>
    );
  }

  if (typeof value === "number") {
    return (
      <NumberField
        label={field}
        value={value}
        onChange={(v) => handleChange(v)}
      />
    );
  }

  if (typeof value === "string") {
    return (
      <TextField
        label={field}
        value={value}
        onChange={(v) => handleChange(v)}
      />
    );
  }

  return (
    <div style={{ display: "flex", alignItems: "flex-start", gap: 8, padding: "1px 0" }}>
      <span style={{ minWidth: 90, color: "#9ca3af", fontSize: 11 }}>{field}:</span>
      <span style={{ color: "#6b7280", fontSize: 11, fontFamily: "ui-monospace, monospace", wordBreak: "break-all" }}>
        {JSON.stringify(value)}
      </span>
    </div>
  );
}

function EcsComponentEditor({
  entityId,
  componentName,
  data,
  rpcClient,
}: {
  entityId: number;
  componentName: string;
  data: Record<string, unknown> | boolean;
  rpcClient: RpcClient;
}) {
  const claimed = useConnectionStore((s) => s.claimed);
  const removeRegistryComponent = useGameStore((s) => s.removeRegistryComponent);
  const canRemoveEcs = claimed && !NON_REMOVABLE_ECS.has(componentName);

  const handleRemove = (ev: MouseEvent) => {
    ev.stopPropagation();
    void removeRegistryComponent(rpcClient, entityId, componentName);
  };

  if (typeof data === "boolean" || Object.keys(data as object).length === 0) {
    return (
      <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "2px 0", fontSize: 11, color: "#6b7280" }}>
        <span style={{ flex: 1 }}>
          <span style={{ color: "#a78bfa", fontWeight: 500 }}>{componentName}</span>
          {" — tag"}
        </span>
        {canRemoveEcs && (
          <button type="button" style={{ ...smallBtnStyle, color: "#ef4444", flexShrink: 0 }} onClick={handleRemove}>
            Remove
          </button>
        )}
      </div>
    );
  }

  const entries = Object.entries(data as Record<string, unknown>);

  return (
    <div style={{ marginBottom: 4 }}>
      <div style={{ display: "flex", alignItems: "flex-start", gap: 8 }}>
        <div style={{ flex: 1, minWidth: 0 }}>
          <CollapsibleSection title={componentName} defaultOpen={false}>
            <div style={{ display: "grid", gap: 2, paddingLeft: 4 }}>
              {entries.map(([field, value]) => (
                <FieldEditor
                  key={field}
                  entityId={entityId}
                  component={componentName}
                  field={field}
                  value={value}
                  rpcClient={rpcClient}
                />
              ))}
            </div>
          </CollapsibleSection>
        </div>
        {canRemoveEcs && (
          <button type="button" style={{ ...smallBtnStyle, color: "#ef4444", flexShrink: 0, marginTop: 2 }} onClick={handleRemove}>
            Remove
          </button>
        )}
      </div>
    </div>
  );
}

function parseRelationLoose(comp: unknown): { parent: number; childIds: number[] } | null {
  if (typeof comp !== "object" || comp === null) return null;
  const r = comp as Record<string, unknown>;
  if (typeof r.parent !== "number") return null;
  const ch = r.children;
  const childIds = Array.isArray(ch) ? ch.filter((x): x is number => typeof x === "number") : [];
  return { parent: r.parent, childIds };
}

function entityMatchesFilter(e: EcsEntityDTO, lower: string): boolean {
  if (!lower) return true;
  return (
    e.label.toLowerCase().includes(lower) ||
    String(e.entity_id).includes(lower) ||
    Object.keys(e.components).some((c) => c.toLowerCase().includes(lower))
  );
}

function computeVisibleEntityIds(entities: EcsEntityDTO[], filterLower: string): Set<number> {
  if (!filterLower) return new Set(entities.map((x) => x.entity_id));
  const byId = new Map(entities.map((e) => [e.entity_id, e]));
  const parents = new Map<number, number>();
  for (const e of entities) {
    const rel = parseRelationLoose(e.components.relation);
    const p = rel?.parent ?? -1;
    if (p >= 0 && byId.has(p)) parents.set(e.entity_id, p);
  }
  const childMap = buildChildLists(entities);
  const childLists = new Map<number, number[]>();
  for (const [pid, ch] of childMap) {
    childLists.set(
      pid,
      ch.map((c) => c.entity_id),
    );
  }
  const seed = new Set<number>();
  for (const e of entities) {
    if (entityMatchesFilter(e, filterLower)) seed.add(e.entity_id);
  }
  const visible = new Set<number>(seed);
  for (const id of [...seed]) {
    let cur = id;
    while (parents.has(cur)) {
      cur = parents.get(cur)!;
      visible.add(cur);
    }
  }
  const stack = [...visible];
  while (stack.length) {
    const id = stack.pop()!;
    for (const ch of childLists.get(id) ?? []) {
      if (!visible.has(ch)) {
        visible.add(ch);
        stack.push(ch);
      }
    }
  }
  return visible;
}

function buildChildLists(entities: EcsEntityDTO[]): Map<number, EcsEntityDTO[]> {
  const byId = new Map(entities.map((e) => [e.entity_id, e]));
  const lists = new Map<number, EcsEntityDTO[]>();
  for (const e of entities) {
    const rel = parseRelationLoose(e.components.relation);
    if (!rel) continue;
    const p = rel.parent;
    if (p < 0 || !byId.has(p) || e.entity_id === p) continue;
    if (!lists.has(p)) lists.set(p, []);
    lists.get(p)!.push(e);
  }
  for (const [, ch] of lists) {
    ch.sort((a, b) => a.entity_id - b.entity_id);
  }
  return lists;
}

function getFrameDataId(entity: EcsEntityDTO): number | null {
  const fd = entity.components.Frame;
  if (typeof fd !== "object" || fd === null || Array.isArray(fd)) return null;
  const id = (fd as Record<string, unknown>).id;
  return typeof id === "number" ? id : null;
}

function getIneditorIcon(entity: EcsEntityDTO): string {
  const raw = entity.components.ineditor;
  if (typeof raw !== "object" || raw === null) return "";
  const icon = (raw as Record<string, unknown>).icon;
  return typeof icon === "string" ? icon.trim() : "";
}

function AddRegistryComponentRow({
  entityId,
  existingNames,
  rpcClient,
}: {
  entityId: number;
  existingNames: string[];
  rpcClient: RpcClient;
}) {
  const claimed = useConnectionStore((s) => s.claimed);
  const addRegistryComponent = useGameStore((s) => s.addRegistryComponent);
  const [pick, setPick] = useState("");
  const existing = new Set(existingNames);
  const options = ECS_REGISTRY_COMPONENT_OPTIONS.filter((n) => !existing.has(n));
  if (!claimed || options.length === 0) return null;
  return (
    <div style={{ display: "flex", flexWrap: "wrap", gap: 6, marginTop: 8, alignItems: "center" }}>
      <select
        value={pick}
        onChange={(e) => setPick(e.target.value)}
        style={{
          fontSize: 11,
          borderRadius: 3,
          border: "1px solid #374151",
          background: "#0b1220",
          color: "#e5e7eb",
          padding: "2px 6px",
          minWidth: 160,
        }}
      >
        <option value="">Add ECS component…</option>
        {options.map((n) => (
          <option key={n} value={n}>
            {n}
          </option>
        ))}
      </select>
      <button
        type="button"
        style={smallBtnStyle}
        disabled={!pick}
        onClick={() => {
          const c = pick;
          setPick("");
          void addRegistryComponent(rpcClient, entityId, c);
        }}
      >
        Add
      </button>
    </div>
  );
}

function GameFrameComponentsSection({
  entity,
  frameDataId,
  rpcClient,
  fetchEntities,
}: {
  entity: EcsEntityDTO;
  frameDataId: number;
  rpcClient: RpcClient;
  fetchEntities: (c: RpcClient) => Promise<void>;
}) {
  const frames = useGameStore((s) => s.frames);
  const addComponent = useGameStore((s) => s.addComponent);
  const removeComponent = useGameStore((s) => s.removeComponent);
  const isFrameControllable = useGameStore((s) => s.isFrameControllable);
  const claimed = useConnectionStore((s) => s.claimed);
  const componentAddSupported = isFeatureSupported(capabilityMethods.componentPalette);
  const [availableComponents, setAvailableComponents] = useState<string[]>([]);
  const [pick, setPick] = useState("");
  const [busy, setBusy] = useState(false);

  const frame = useMemo(
    () =>
      frames.find((f) => f.entity_id === entity.entity_id || f.id === frameDataId) ?? null,
    [frames, entity.entity_id, frameDataId],
  );
  const gameComponents: ComponentDTO[] = frame?.components ?? [];
  const canMutate = claimed && isFrameControllable(frameDataId);

  useEffect(() => {
    if (!componentAddSupported) return;
    void rpcClient
      .call<{ sources: Record<string, string> }>("code.sources")
      .then((data) => setAvailableComponents(Object.keys(data.sources ?? {}).sort()))
      .catch(() => setAvailableComponents([]));
  }, [rpcClient, componentAddSupported]);

  const handleAdd = async () => {
    const name = pick.trim();
    if (!name || busy) return;
    setBusy(true);
    try {
      await addComponent(rpcClient, frameDataId, name);
      await fetchEntities(rpcClient);
    } finally {
      setBusy(false);
    }
  };

  const handleRemove = async (componentId: number) => {
    if (busy) return;
    setBusy(true);
    try {
      await removeComponent(rpcClient, frameDataId, componentId);
      await fetchEntities(rpcClient);
    } finally {
      setBusy(false);
    }
  };

  return (
    <CollapsibleSection title="Game components (Lua)" defaultOpen={false}>
      {!frame && (
        <div style={{ fontSize: 10, color: "#6b7280", marginBottom: 4 }}>
          Frame snapshot not synced yet; list updates on the next state tick.
        </div>
      )}
      <div style={{ display: "grid", gap: 4 }}>
        {gameComponents.map((c) => {
          const iconFile = (c.metadata?.icon || c.icon || "").trim();
          return (
            <div
              key={c.id}
              style={{
                display: "flex",
                alignItems: "center",
                gap: 6,
                fontSize: 11,
                color: "#e5e7eb",
              }}
            >
              {iconFile ? (
                <img src={`/icons/${iconFile}`} alt="" style={{ width: 14, height: 14, objectFit: "contain", flexShrink: 0 }} />
              ) : (
                <span style={{ width: 14, flexShrink: 0 }} />
              )}
              <span style={{ flex: 1, minWidth: 0 }}>
                <span style={{ color: "#a78bfa" }}>{c.name}</span>
                <span style={{ color: "#6b7280", marginLeft: 6 }}>#{c.id}</span>
                {c.state ? <span style={{ color: "#6b7280", marginLeft: 6 }}>{c.state}</span> : null}
              </span>
              {canMutate && (
                <button
                  type="button"
                  style={{ ...smallBtnStyle, color: "#ef4444", flexShrink: 0 }}
                  disabled={busy}
                  onClick={() => void handleRemove(c.id)}
                >
                  Remove
                </button>
              )}
            </div>
          );
        })}
      </div>
      {componentAddSupported && canMutate && (
        <div style={{ display: "flex", flexWrap: "wrap", gap: 6, marginTop: 8, alignItems: "center" }}>
          <select
            value={pick}
            onChange={(e) => setPick(e.target.value)}
            style={{
              fontSize: 11,
              borderRadius: 3,
              border: "1px solid #374151",
              background: "#0b1220",
              color: "#e5e7eb",
              padding: "2px 6px",
              minWidth: 140,
            }}
          >
            <option value="">Add component…</option>
            {availableComponents.map((n) => (
              <option key={n} value={n}>
                {n}
              </option>
            ))}
          </select>
          <button type="button" style={smallBtnStyle} disabled={busy || !pick.trim()} onClick={() => void handleAdd()}>
            Add
          </button>
        </div>
      )}
      {!claimed && (
        <div style={{ fontSize: 10, color: "#6b7280", marginTop: 6 }}>Claim session to add or remove components.</div>
      )}
      {claimed && !isFrameControllable(frameDataId) && (
        <div style={{ fontSize: 10, color: "#6b7280", marginTop: 6 }}>Frame outside control zone — remove disabled.</div>
      )}
    </CollapsibleSection>
  );
}

function EntityRow({
  entity,
  rpcClient,
  depth,
  childLists,
  visibleIds,
  fetchEntities,
  filterSignature,
}: {
  entity: EcsEntityDTO;
  rpcClient: RpcClient;
  depth: number;
  childLists: Map<number, EcsEntityDTO[]>;
  visibleIds: Set<number>;
  fetchEntities: (c: RpcClient) => Promise<void>;
  filterSignature: string;
}) {
  const [expanded, setExpanded] = useState(false);
  const claimed = useConnectionStore((s) => s.claimed);
  const createEmptyEntity = useGameStore((s) => s.createEmptyEntity);
  const destroyEntityByEid = useGameStore((s) => s.destroyEntityByEid);

  useEffect(() => {
    if (filterSignature.length > 0) setExpanded(true);
  }, [filterSignature]);
  const componentNames = Object.keys(entity.components);
  const frameDataId = getFrameDataId(entity);
  const entityColor = entity.color || undefined;
  const iconFile = getIneditorIcon(entity);

  const children = (childLists.get(entity.entity_id) ?? []).filter((c) => visibleIds.has(c.entity_id));
  const canDeleteEntity = claimed && entity.label !== "Environment";

  return (
    <div style={{ borderBottom: "1px solid #1f2937" }}>
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 8,
          padding: "4px 4px",
          paddingLeft: 4 + depth * 14,
          fontSize: 11,
          borderLeft: entityColor ? `3px solid ${entityColor}` : "3px solid transparent",
          background: expanded && entityColor ? `${entityColor}10` : undefined,
        }}
      >
        <div
          style={{
            flex: 1,
            display: "flex",
            alignItems: "center",
            gap: 8,
            cursor: "pointer",
            minWidth: 0,
          }}
          onClick={() => setExpanded((v) => !v)}
        >
          <span style={{ color: "#6b7280", fontSize: 10 }}>{expanded ? "\u25BC" : "\u25B6"}</span>
          {iconFile ? (
            <img
              src={`/icons/${iconFile}`}
              alt=""
              style={{ width: 16, height: 16, objectFit: "contain", flexShrink: 0 }}
            />
          ) : entityColor ? (
            <span
              style={{
                width: 8,
                height: 8,
                borderRadius: "50%",
                background: entityColor,
                flexShrink: 0,
              }}
            />
          ) : null}
          <span style={{ color: "#60a5fa", fontFamily: "ui-monospace, monospace" }}>
            {entity.entity_id}
          </span>
          <span style={{ color: entityColor || "#e5e7eb", fontWeight: 500 }}>
            {entity.label || "(unnamed)"}
          </span>
          <span style={{ color: "#6b7280", fontSize: 10 }}>
            [{componentNames.length} components{frameDataId != null ? ` · game frame #${frameDataId}` : ""}]
          </span>
        </div>
        {claimed && (
          <div style={{ display: "flex", gap: 4, flexShrink: 0 }} onClick={(e) => e.stopPropagation()}>
            <button
              type="button"
              style={smallBtnStyle}
              onClick={() => void createEmptyEntity(rpcClient, { parent_entity_id: entity.entity_id, name: "Child" })}
            >
              + Child
            </button>
            {canDeleteEntity && (
              <button
                type="button"
                style={{ ...smallBtnStyle, color: "#ef4444" }}
                onClick={() => void destroyEntityByEid(rpcClient, entity.entity_id)}
              >
                Delete
              </button>
            )}
          </div>
        )}
      </div>
      {expanded && (
        <>
          <div style={{ paddingLeft: 16 + depth * 14, paddingBottom: 6, display: "grid", gap: 2 }}>
            {frameDataId != null && (
              <GameFrameComponentsSection
                entity={entity}
                frameDataId={frameDataId}
                rpcClient={rpcClient}
                fetchEntities={fetchEntities}
              />
            )}
            {componentNames.map((name) => (
              <EcsComponentEditor
                key={name}
                entityId={entity.entity_id}
                componentName={name}
                data={entity.components[name]!}
                rpcClient={rpcClient}
              />
            ))}
            <AddRegistryComponentRow entityId={entity.entity_id} existingNames={componentNames} rpcClient={rpcClient} />
          </div>
          {children.map((ch) => (
            <EntityRow
              key={ch.entity_id}
              entity={ch}
              rpcClient={rpcClient}
              depth={depth + 1}
              childLists={childLists}
              visibleIds={visibleIds}
              fetchEntities={fetchEntities}
              filterSignature={filterSignature}
            />
          ))}
        </>
      )}
    </div>
  );
}

type Props = {
  rpcClient: RpcClient;
  filter: string;
};

export function EntitiesSection({ rpcClient, filter }: Props) {
  const ecsEntities = useGameStore((s) => s.ecsEntities);
  const fetchEntities = useGameStore((s) => s.fetchEntities);
  const createEmptyEntity = useGameStore((s) => s.createEmptyEntity);
  const claimed = useConnectionStore((s) => s.claimed);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    void fetchEntities(rpcClient);
  }, [rpcClient, fetchEntities]);

  const filterLower = filter.trim().toLowerCase();

  const { roots, childLists, visibleIds } = useMemo(() => {
    const visible = computeVisibleEntityIds(ecsEntities, filterLower);
    const lists = buildChildLists(ecsEntities);
    const asChild = new Set<number>();
    for (const [, ch] of lists) {
      for (const c of ch) asChild.add(c.entity_id);
    }
    const rootEntities = ecsEntities.filter((e) => !asChild.has(e.entity_id));
    rootEntities.sort((a, b) => a.entity_id - b.entity_id);
    const rootsFiltered = rootEntities.filter((e) => visible.has(e.entity_id));
    return { roots: rootsFiltered, childLists: lists, visibleIds: visible };
  }, [ecsEntities, filterLower]);

  const handleRefresh = async () => {
    setLoading(true);
    try {
      await fetchEntities(rpcClient);
    } finally {
      setLoading(false);
    }
  };

  const shownCount = useMemo(() => {
    if (!filterLower) return ecsEntities.length;
    return [...visibleIds].length;
  }, [ecsEntities.length, filterLower, visibleIds]);

  return (
    <CollapsibleSection title={`\uD83E\uDDE9 All Entities (${ecsEntities.length}${filterLower ? ` · ${shownCount} shown` : ""})`}>
      <div style={{ display: "flex", gap: 6, marginBottom: 6, alignItems: "center", flexWrap: "wrap" }}>
        <button
          type="button"
          onClick={() => void handleRefresh()}
          disabled={loading}
          style={{
            fontSize: 11,
            padding: "2px 8px",
            borderRadius: 3,
            border: "1px solid #374151",
            background: "#0b1220",
            color: "#e5e7eb",
            cursor: loading ? "not-allowed" : "pointer",
          }}
        >
          {loading ? "Loading\u2026" : "Refresh"}
        </button>
        <span style={{ fontSize: 10, color: "#6b7280" }}>Live sync ~400ms after state / env updates</span>
        {claimed && (
          <button
            type="button"
            style={smallBtnStyle}
            onClick={() => void createEmptyEntity(rpcClient, { name: "Entity" })}
          >
            + Empty root entity
          </button>
        )}
      </div>
      {ecsEntities.length === 0 && !loading ? (
        <div style={{ color: "#6b7280", fontSize: 11 }}>Loading entities…</div>
      ) : roots.length === 0 ? (
        <div style={{ color: "#6b7280", fontSize: 11 }}>No matches</div>
      ) : (
        <div style={{ display: "grid", gap: 0 }}>
          {roots.map((entity) => (
            <EntityRow
              key={entity.entity_id}
              entity={entity}
              rpcClient={rpcClient}
              depth={0}
              childLists={childLists}
              visibleIds={visibleIds}
              fetchEntities={fetchEntities}
              filterSignature={filterLower}
            />
          ))}
        </div>
      )}
    </CollapsibleSection>
  );
}

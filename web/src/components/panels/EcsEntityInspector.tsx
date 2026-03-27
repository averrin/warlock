import { useCallback, useEffect, useMemo, useState, type MouseEvent } from "react";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO, EcsEntityDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { useConnectionStore } from "../../stores/connection";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import {
  CollapsibleSection,
  DictEditor,
  NumberField,
  TextField,
  smallBtnStyle,
  Badge,
  IconButton,
  CardExpandControl,
  fmtId,
  copyToClipboard,
} from "../ui";

function isFolderEntity(entity: EcsEntityDTO): boolean {
  const meta = entity.components.meta;
  if (typeof meta !== "object" || meta === null) return false;
  return (meta as Record<string, unknown>).id === "FOLDER";
}

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

const NON_REMOVABLE_ECS = new Set([
  "Frame",
  "Connection",
  "ResourcePatch",
  "Environment",
  "SpendablePool",
]);

type FieldEditorProps = {
  entityId: number;
  component: string;
  field: string;
  value: unknown;
  rpcClient: RpcClient;
  rpcPrefix?: string;
};

function FieldEditor({ entityId, component, field, value, rpcClient, rpcPrefix = "entities" }: FieldEditorProps) {
  const setEntityField = useGameStore((s) => s.setEntityField);
  const handleChange = useCallback(
    (v: unknown) => {
      if (rpcPrefix === "entities") {
        void setEntityField(rpcClient, entityId, component, field, v);
      } else {
        void rpcClient.call(`${rpcPrefix}.set_field`, { entity_id: entityId, component, field, value: v });
      }
    },
    [rpcClient, rpcPrefix, entityId, component, field, setEntityField],
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

function SpendablePoolEditor({
  entityId,
  data,
  rpcClient,
  rpcPrefix = "entities",
}: {
  entityId: number;
  data: Record<string, unknown>;
  rpcClient: RpcClient;
  rpcPrefix?: string;
}) {
  const setEntityField = useGameStore((s) => s.setEntityField);
  const raw = data.amounts;
  const amounts: Record<string, number> =
    typeof raw === "object" && raw !== null && !Array.isArray(raw)
      ? Object.fromEntries(
          Object.entries(raw as Record<string, unknown>).map(([k, v]) => [
            k,
            typeof v === "number" ? v : Number(v) || 0,
          ]),
        )
      : {};

  const handleChange = (key: string, value: unknown) => {
    if (rpcPrefix === "entities") {
      void setEntityField(rpcClient, entityId, "SpendablePool", key, value);
    } else {
      void rpcClient.call(`${rpcPrefix}.set_field`, { entity_id: entityId, component: "SpendablePool", field: key, value });
    }
  };

  return (
    <CollapsibleSection title="SpendablePool" defaultOpen={false}>
      <DictEditor
        data={amounts}
        onChangeEntry={handleChange}
      />
    </CollapsibleSection>
  );
}

function EcsComponentEditor({
  entityId,
  componentName,
  data,
  rpcClient,
  rpcPrefix = "entities",
}: {
  entityId: number;
  componentName: string;
  data: Record<string, unknown> | boolean;
  rpcClient: RpcClient;
  rpcPrefix?: string;
}) {
  const claimed = useConnectionStore((s) => s.claimed);
  const removeRegistryComponent = useGameStore((s) => s.removeRegistryComponent);
  const canRemoveEcs = claimed && !NON_REMOVABLE_ECS.has(componentName) && rpcPrefix === "entities";

  const handleRemove = (ev: MouseEvent) => {
    ev.stopPropagation();
    if (rpcPrefix === "entities") {
      void removeRegistryComponent(rpcClient, entityId, componentName);
    } else {
      void rpcClient.call(`${rpcPrefix}.component.remove`, { entity_id: entityId, component: componentName });
    }
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

  if (componentName === "SpendablePool" && typeof data === "object" && data !== null && !Array.isArray(data)) {
    return (
      <div style={{ marginBottom: 4 }}>
        <SpendablePoolEditor entityId={entityId} data={data as Record<string, unknown>} rpcClient={rpcClient} rpcPrefix={rpcPrefix} />
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
                  rpcPrefix={rpcPrefix}
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

function parseOptionalEntityId(v: unknown): number | null {
  if (v === null || v === undefined) return null;
  if (typeof v === "number" && Number.isFinite(v)) return Math.trunc(v);
  if (typeof v === "string" && /^-?\d+$/.test(v.trim())) return parseInt(v.trim(), 10);
  return null;
}

function parseRelationLoose(comp: unknown): { parent: number; childIds: number[] } | null {
  if (typeof comp !== "object" || comp === null) return null;
  const r = comp as Record<string, unknown>;
  if (!("parent" in r) && !("children" in r)) return null;
  const parent = parseOptionalEntityId(r.parent) ?? -1;
  const ch = r.children;
  const childIds = Array.isArray(ch)
    ? ch.map(parseOptionalEntityId).filter((x): x is number => x !== null)
    : [];
  return { parent, childIds };
}

function buildParentOfChildMap(entities: EcsEntityDTO[]): Map<number, number> {
  const byId = new Map<number, EcsEntityDTO>();
  for (const e of entities) {
    const id = parseOptionalEntityId(e.entity_id);
    if (id !== null) byId.set(id, e);
  }
  const childToParent = new Map<number, number>();
  for (const e of entities) {
    const eid = parseOptionalEntityId(e.entity_id);
    if (eid === null) continue;
    const rel = parseRelationLoose(e.components.relation);
    if (!rel) continue;
    const p = rel.parent;
    if (p >= 0 && byId.has(p) && eid !== p) childToParent.set(eid, p);
  }
  for (const e of entities) {
    const pid = parseOptionalEntityId(e.entity_id);
    if (pid === null) continue;
    const rel = parseRelationLoose(e.components.relation);
    if (!rel) continue;
    for (const cid of rel.childIds) {
      if (!byId.has(cid) || cid === pid) continue;
      if (!childToParent.has(cid)) childToParent.set(cid, pid);
    }
  }
  return childToParent;
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
  const parents = buildParentOfChildMap(entities);
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
  const byId = new Map<number, EcsEntityDTO>();
  for (const e of entities) {
    const id = parseOptionalEntityId(e.entity_id);
    if (id !== null) byId.set(id, e);
  }
  const lists = new Map<number, EcsEntityDTO[]>();
  for (const [childId, parentId] of buildParentOfChildMap(entities)) {
    const child = byId.get(childId);
    if (!child) continue;
    if (!lists.has(parentId)) lists.set(parentId, []);
    lists.get(parentId)!.push(child);
  }
  for (const [, ch] of lists) {
    ch.sort((a, b) => {
      const ai = parseOptionalEntityId(a.entity_id) ?? 0;
      const bi = parseOptionalEntityId(b.entity_id) ?? 0;
      return ai - bi;
    });
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
  rpcPrefix = "entities",
}: {
  entityId: number;
  existingNames: string[];
  rpcClient: RpcClient;
  rpcPrefix?: string;
}) {
  const claimed = useConnectionStore((s) => s.claimed);
  const addRegistryComponent = useGameStore((s) => s.addRegistryComponent);
  const [pick, setPick] = useState("");
  const existing = new Set(existingNames);
  const options = ECS_REGISTRY_COMPONENT_OPTIONS.filter((n) => !existing.has(n));
  if (!claimed || options.length === 0 || rpcPrefix !== "entities") return null;
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
              <span style={{ flex: 1, minWidth: 0, display: "flex", alignItems: "center", gap: 4 }}>
                <span style={{ color: "#a78bfa" }}>{c.name}</span>
                <Badge label={fmtId(c.id)} variant="id" title={`Component ID ${c.id} — click to copy`} onClick={() => copyToClipboard(`#${c.id}`)} />
                {c.state ? <span style={{ color: "#6b7280" }}>{c.state}</span> : null}
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
  rpcPrefix = "entities",
}: {
  entity: EcsEntityDTO;
  rpcClient: RpcClient;
  depth: number;
  childLists: Map<number, EcsEntityDTO[]>;
  visibleIds: Set<number>;
  fetchEntities: (c: RpcClient) => Promise<void>;
  filterSignature: string;
  rpcPrefix?: string;
}) {
  const [expanded, setExpanded] = useState(false);
  const claimed = useConnectionStore((s) => s.claimed);
  const createEmptyEntity = useGameStore((s) => s.createEmptyEntity);
  const destroyEntityByEid = useGameStore((s) => s.destroyEntityByEid);

  const handleCreateChild = async () => {
    if (rpcPrefix === "entities") {
      await createEmptyEntity(rpcClient, { parent_entity_id: entity.entity_id, name: "Child" });
    } else {
      await rpcClient.call(`${rpcPrefix}.create`, { name: "Child", parent_entity_id: entity.entity_id });
      await fetchEntities(rpcClient);
    }
  };

  const handleDestroy = async () => {
    if (rpcPrefix === "entities") {
      await destroyEntityByEid(rpcClient, entity.entity_id);
    } else {
      await rpcClient.call(`${rpcPrefix}.destroy`, { entity_id: entity.entity_id });
      await fetchEntities(rpcClient);
    }
  };

  useEffect(() => {
    if (filterSignature.length > 0) setExpanded(true);
  }, [filterSignature]);
  const componentNames = Object.keys(entity.components);
  const frameDataId = getFrameDataId(entity);
  const entityColor = entity.color || undefined;
  const iconFile = getIneditorIcon(entity);

  const children = (childLists.get(entity.entity_id) ?? []).filter((c) => visibleIds.has(c.entity_id));
  const canDeleteEntity = claimed && entity.label !== "Environment";
  const isFolder = isFolderEntity(entity);

  return (
    <div style={{ borderBottom: "1px solid #1f2937" }}>
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 6,
          padding: "4px 4px",
          paddingLeft: 4 + depth * 14,
          fontSize: 11,
          cursor: "pointer",
          borderLeft: entityColor ? `3px solid ${entityColor}` : "3px solid transparent",
          background: expanded && entityColor ? `${entityColor}10` : undefined,
        }}
        onClick={() => setExpanded((v) => !v)}
      >
        {/* Click target: everything left of the action buttons */}
        <div
          style={{ flex: 1, display: "flex", alignItems: "center", gap: 6, minWidth: 0 }}
        >
          {iconFile ? (
            <img src={`/icons/${iconFile}`} alt="" style={{ width: 16, height: 16, objectFit: "contain", flexShrink: 0 }} />
          ) : entityColor ? (
            <span style={{ width: 8, height: 8, borderRadius: "50%", background: entityColor, flexShrink: 0 }} />
          ) : null}
          <span style={{ color: entityColor || "#e5e7eb", fontWeight: 500, minWidth: 0 }}>
            {entity.label || "(unnamed)"}
          </span>
          <Badge label={fmtId(entity.entity_id)} variant="id" title={`Entity ID ${entity.entity_id} — click to copy`} onClick={() => copyToClipboard(`#${entity.entity_id}`)} />
          {isFolder ? (
            <Badge label="folder" variant="custom" />
          ) : frameDataId != null ? (
            <Badge label={`frame ${fmtId(frameDataId)}`} variant="id" title={`Frame ID ${frameDataId} — click to copy`} onClick={() => copyToClipboard(`#${frameDataId}`)} />
          ) : null}
        </div>
        {/* Action buttons — stop propagation so clicks don't toggle expand */}
        {claimed && (
          <div style={{ display: "flex", gap: 4, flexShrink: 0 }} onClick={(e) => e.stopPropagation()}>
            <IconButton
              icon="⊕"
              size="sm"
              title="Add child entity"
              onClick={() => void handleCreateChild()}
            />
            {canDeleteEntity && (
              <IconButton
                icon="✕"
                size="sm"
                variant="danger"
                title="Delete entity"
                onClick={() => void handleDestroy()}
              />
            )}
          </div>
        )}
        <CardExpandControl expanded={expanded} onToggle={() => setExpanded((v) => !v)} />
      </div>
      {expanded && (
        <>
          {!isFolder && (componentNames.length > 0 || frameDataId != null || claimed) && (
            <div
              style={{
                border: "1px solid #334155",
                borderRadius: 6,
                padding: "6px 8px",
                margin: `4px 4px 4px ${4 + depth * 14}px`,
                display: "grid",
                gap: 2,
              }}
            >
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
                  rpcPrefix={rpcPrefix}
                />
              ))}
              <AddRegistryComponentRow
                entityId={entity.entity_id}
                existingNames={componentNames}
                rpcClient={rpcClient}
                rpcPrefix={rpcPrefix}
              />
            </div>
          )}
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
              rpcPrefix={rpcPrefix}
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
  rpcPrefix?: string;
};

export function EntitiesSection({ rpcClient, filter, rpcPrefix = "entities" }: Props) {
  const ecsEntitiesGame = useGameStore((s) => s.ecsEntities);
  const fetchEntitiesGame = useGameStore((s) => s.fetchEntities);
  const createEmptyEntity = useGameStore((s) => s.createEmptyEntity);
  const claimed = useConnectionStore((s) => s.claimed);
  const [loading, setLoading] = useState(false);
  const [localEntities, setLocalEntities] = useState<EcsEntityDTO[]>([]);

  const isGameEntities = rpcPrefix === "entities";
  const ecsEntities = isGameEntities ? ecsEntitiesGame : localEntities;

  const fetchEntities = isGameEntities
    ? fetchEntitiesGame
    : async (c: RpcClient) => {
        const res = await c.call(`${rpcPrefix}.list`, {}) as { entities: EcsEntityDTO[] };
        setLocalEntities(res.entities ?? []);
      };

  useEffect(() => {
    void fetchEntities(rpcClient);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [rpcClient, rpcPrefix]);

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
        {isGameEntities && (
          <span style={{ fontSize: 10, color: "#6b7280" }}>Live sync ~400ms after state / env updates</span>
        )}
        {claimed && (
          <button
            type="button"
            style={smallBtnStyle}
            onClick={() =>
              isGameEntities
                ? void createEmptyEntity(rpcClient, { name: "Entity" })
                : void rpcClient.call(`${rpcPrefix}.create`, { name: "Entity" }).then(() => fetchEntities(rpcClient))
            }
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
              rpcPrefix={rpcPrefix}
            />
          ))}
        </div>
      )}
    </CollapsibleSection>
  );
}

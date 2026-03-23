import { useCallback, useMemo, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import type { EcsEntityDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { CollapsibleSection, NumberField, TextField } from "../ui";

// ─── Field‐level editors ────────────────────────────────────────────────────

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

  // Arrays and objects: read‐only JSON display
  return (
    <div style={{ display: "flex", alignItems: "flex-start", gap: 8, padding: "1px 0" }}>
      <span style={{ minWidth: 90, color: "#9ca3af", fontSize: 11 }}>{field}:</span>
      <span style={{ color: "#6b7280", fontSize: 11, fontFamily: "ui-monospace, monospace", wordBreak: "break-all" }}>
        {JSON.stringify(value)}
      </span>
    </div>
  );
}

// ─── Component editor (one ECS component on one entity) ─────────────────────

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
  // Tag components (proto, item) or empty marker components
  if (typeof data === "boolean" || Object.keys(data).length === 0) {
    return (
      <div style={{ padding: "2px 0", fontSize: 11, color: "#6b7280" }}>
        <span style={{ color: "#a78bfa", fontWeight: 500 }}>{componentName}</span>
        {" — tag"}
      </div>
    );
  }

  const entries = Object.entries(data);

  return (
    <CollapsibleSection
      title={componentName}
      defaultOpen={false}
    >
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
  );
}

// ─── Single entity row ──────────────────────────────────────────────────────

function EntityRow({
  entity,
  rpcClient,
}: {
  entity: EcsEntityDTO;
  rpcClient: RpcClient;
}) {
  const [expanded, setExpanded] = useState(false);
  const componentNames = Object.keys(entity.components);

  return (
    <div style={{ borderBottom: "1px solid #1f2937" }}>
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 8,
          padding: "4px 0",
          cursor: "pointer",
          fontSize: 11,
        }}
        onClick={() => setExpanded((v) => !v)}
      >
        <span style={{ color: "#6b7280", fontSize: 10 }}>{expanded ? "\u25BC" : "\u25B6"}</span>
        <span style={{ color: "#60a5fa", fontFamily: "ui-monospace, monospace" }}>
          {entity.entity_id}
        </span>
        <span style={{ color: "#e5e7eb", fontWeight: 500 }}>
          {entity.label || "(unnamed)"}
        </span>
        <span style={{ color: "#6b7280", fontSize: 10 }}>
          [{componentNames.length} components]
        </span>
      </div>
      {expanded && (
        <div style={{ paddingLeft: 16, paddingBottom: 6, display: "grid", gap: 2 }}>
          {componentNames.map((name) => (
            <EcsComponentEditor
              key={name}
              entityId={entity.entity_id}
              componentName={name}
              data={entity.components[name]!}
              rpcClient={rpcClient}
            />
          ))}
        </div>
      )}
    </div>
  );
}

// ─── Main entities section ──────────────────────────────────────────────────

type Props = {
  rpcClient: RpcClient;
  filter: string;
};

export function EntitiesSection({ rpcClient, filter }: Props) {
  const ecsEntities = useGameStore((s) => s.ecsEntities);
  const fetchEntities = useGameStore((s) => s.fetchEntities);
  const [loading, setLoading] = useState(false);

  const filtered = useMemo(() => {
    if (!filter) return ecsEntities;
    const lower = filter.toLowerCase();
    return ecsEntities.filter(
      (e) =>
        e.label.toLowerCase().includes(lower) ||
        String(e.entity_id).includes(lower) ||
        Object.keys(e.components).some((c) => c.toLowerCase().includes(lower)),
    );
  }, [ecsEntities, filter]);

  const handleRefresh = async () => {
    setLoading(true);
    try {
      await fetchEntities(rpcClient);
    } finally {
      setLoading(false);
    }
  };

  return (
    <CollapsibleSection title={`\uD83E\uDDE9 All Entities (${ecsEntities.length})`}>
      <div style={{ display: "flex", gap: 6, marginBottom: 6 }}>
        <button
          type="button"
          onClick={handleRefresh}
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
      </div>
      {filtered.length === 0 ? (
        <div style={{ color: "#6b7280", fontSize: 11 }}>
          {ecsEntities.length === 0 ? "Click Refresh to load entities" : "No matches"}
        </div>
      ) : (
        <div style={{ display: "grid", gap: 0 }}>
          {filtered.map((entity) => (
            <EntityRow key={entity.entity_id} entity={entity} rpcClient={rpcClient} />
          ))}
        </div>
      )}
    </CollapsibleSection>
  );
}

import { usePatchStore, Patch } from "../../stores/patches";
import type { RpcClient } from "../../rpc/client";
import "./PatchMiniInspector.css";

interface Props {
  patch: Patch;
  x: number;
  y: number;
  rpcClient: RpcClient;
  onClose: () => void;
}

export function PatchMiniInspector({ patch, x, y, rpcClient, onClose }: Props) {
  const removePatch = usePatchStore((s) => s.removePatch);
  
  const handleDelete = async () => {
    try {
      await rpcClient.call("patches.delete", { id: patch.id });
      removePatch(patch.id);
      onClose();
    } catch (e) {
      console.error("Failed to delete patch:", e);
    }
  };
  
  return (
    <div 
      className="patch-mini-inspector"
      style={{ left: x, top: y }}
      data-context-menu
    >
      <div className="header">
        <span className="name">{patch.name}</span>
        <button className="close" onClick={onClose}>×</button>
      </div>
      <div className="content">
        <div className="row">
          <span className="label">Type:</span>
          <span>{patch.type}</span>
        </div>
        <div className="row">
          <span className="label">Item:</span>
          <span>{patch.item}</span>
        </div>
        <div className="row">
          <span className="label">Cells:</span>
          <span>{patch.cells.length}</span>
        </div>
        <div className="row">
          <span className="label">Size:</span>
          <span>{patch.bounds.w} × {patch.bounds.h}</span>
        </div>
      </div>
      <div className="actions">
        <button className="delete" onClick={handleDelete}>Delete</button>
      </div>
    </div>
  );
}

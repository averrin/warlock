import type { FrameDTO } from "../../rpc/types";
import { STATE_COLORS } from "../ui";

type Props = {
  frame: FrameDTO;
};

export function ErrorList({ frame }: Props) {
  const messages = [
    ...(frame.components ?? [])
      .map((c) => {
        const msg = c.error?.trim();
        if (!msg) return null;
        return `${c.name}: ${msg}`;
      })
      .filter((e): e is string => !!e),
    frame.canvas_badges?.has_error && !frame.canvas_badges.health ? "Frame has errors" : null,
  ].filter((e): e is string => !!e);

  if (messages.length === 0) return null;

  return (
    <div style={{ display: "flex", flexWrap: "wrap", gap: 4 }}>
      {messages.map((msg, idx) => (
        <span
          key={`${msg}-${idx}`}
          style={{
            borderRadius: 999,
            padding: "2px 6px",
            background: STATE_COLORS.ERROR,
            color: "#fee2e2",
            fontSize: 10,
            maxWidth: 200,
            overflow: "hidden",
            textOverflow: "ellipsis",
            whiteSpace: "nowrap",
          }}
          title={msg}
        >
          {msg}
        </span>
      ))}
    </div>
  );
}

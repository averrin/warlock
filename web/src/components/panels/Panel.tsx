import { PropsWithChildren } from "react";

type Props = PropsWithChildren<{
  title: string;
  unsupported?: string;
}>;

export function Panel({ title, unsupported, children }: Props) {
  return (
    <section
      style={{
        border: "1px solid #1f2937",
        borderRadius: 8,
        padding: 10,
        background: "#111827",
      }}
    >
      <header
        style={{
          display: "flex",
          justifyContent: "space-between",
          alignItems: "center",
          marginBottom: 8,
          fontSize: 13,
          fontWeight: 600,
        }}
      >
        <span>{title}</span>
        {unsupported ? (
          <span style={{ color: "#fca5a5", fontWeight: 500, fontSize: 11 }}>
            Not Yet Supported: {unsupported}
          </span>
        ) : null}
      </header>
      {children}
    </section>
  );
}

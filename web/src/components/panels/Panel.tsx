import { PropsWithChildren, ReactNode } from "react";

type Props = PropsWithChildren<{
  title: string;
  unsupported?: string;
  headerContent?: ReactNode;
}>;

export function Panel({ title, unsupported, headerContent, children }: Props) {
  return (
    <section
      style={{
        border: "1px solid #1f2937",
        borderRadius: 8,
        background: "#111827",
        padding: 10,
        height: "100%",
        display: "flex",
        flexDirection: "column",
        minHeight: 0,
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
        {headerContent ?? <span>{title}</span>}
        {unsupported ? (
          <span style={{ color: "#fca5a5", fontWeight: 500, fontSize: 11 }}>
            Not Yet Supported: {unsupported}
          </span>
        ) : null}
      </header>
      <div
        style={{
          flex: 1,
          minHeight: 0,
          overflow: "auto",
        }}
      >
        {children}
      </div>
    </section>
  );
}

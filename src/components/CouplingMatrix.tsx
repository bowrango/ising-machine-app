import { CODE_MAX, SPIN_COUNT, SPIN_LABELS, type Config, type State } from "../../protocol/messages";

interface Props {
  config: Config | null;
  state: State | null;
}

/** Spins as black/white boxes over an upper-triangular grid of coupler codes. */
export function CouplingMatrix({ config, state }: Props) {
  const codes = new Map<string, number | null>();
  for (const c of config?.couplers ?? []) codes.set(`${c.i},${c.j}`, c.code);

  const idx = [...Array(SPIN_COUNT).keys()];

  return (
    <div className="matrix" style={{ gridTemplateColumns: `auto repeat(${SPIN_COUNT}, var(--cell))` }}>
      {/* Spin row */}
      <div className="label" />
      {idx.map((j) => {
        const spin = state?.spins[j];
        const agreement = state?.agreement[j];
        return (
          <div
            key={`s${j}`}
            className={`spin ${spin === 1 ? "up" : spin === -1 ? "down" : "unknown"}`}
            title={spin === undefined ? "No data" : `${SPIN_LABELS[j]}: ${spin > 0 ? "+1" : "−1"} (${agreement}% agreement)`}
          />
        );
      })}

      {/* Column labels */}
      <div className="label" />
      {idx.map((j) => (
        <div key={`c${j}`} className="label">{SPIN_LABELS[j]}</div>
      ))}

      {/* Upper-triangular couplings (the last row would be empty) */}
      {idx.slice(0, -1).map((i) => (
        <Row key={`r${i}`} i={i} idx={idx} codes={codes} hasConfig={!!config} />
      ))}
    </div>
  );
}

function Row({ i, idx, codes, hasConfig }: { i: number; idx: number[]; codes: Map<string, number | null>; hasConfig: boolean }) {
  return (
    <>
      <div className="label">{SPIN_LABELS[i]}</div>
      {idx.map((j) => {
        if (j <= i) return <div key={j} className="cell empty" />;
        const code = codes.get(`${i},${j}`);
        if (!hasConfig || code === undefined) return <div key={j} className="cell" />;
        if (code === null) {
          return <div key={j} className="cell off" title={`${SPIN_LABELS[i]}–${SPIN_LABELS[j]}: off`}>·</div>;
        }
        // Lower code = lower resistance = stronger coupling = more saturated.
        const strength = 1 - code / CODE_MAX;
        return (
          <div
            key={j}
            className="cell on"
            style={{ "--strength": strength } as React.CSSProperties}
            title={`${SPIN_LABELS[i]}–${SPIN_LABELS[j]}: code ${code}`}
          >
            {code}
          </div>
        );
      })}
    </>
  );
}

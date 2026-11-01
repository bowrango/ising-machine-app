import { SPIN_COUNT, SPIN_LABELS, type Published } from "../../protocol/messages";

interface Props {
  result: Published | null;
}

/** A max-cut result: an optimal state and each spin's correlation over an upper-triangular grid of
 * problem weights. */
export function ResultView({ result }: Props) {
  const weights = new Map<string, number>();
  for (const [edge, w] of Object.entries(result?.problem ?? {})) {
    const [i, j] = [SPIN_LABELS.indexOf(edge[0] as never), SPIN_LABELS.indexOf(edge[1] as never)].sort();
    weights.set(`${i},${j}`, w);
  }
  const wMax = Math.max(1, ...weights.values());

  const idx = [...Array(SPIN_COUNT).keys()];

  return (
    <div className="matrix" style={{ gridTemplateColumns: `auto repeat(${SPIN_COUNT}, var(--cell))` }}>
      {/* Optimal state: white +, black - */}
      <div className="label row-label">{result ? `best (cut=${result.max.cut})` : "best"}</div>
      {idx.map((j) => {
        const phase = result?.max.state[SPIN_LABELS[j]];
        if (phase === undefined) return <div key={`m${j}`} className="spin" />;
        return (
          <div
            key={`m${j}`}
            className={`spin best ${phase === "+" ? "up" : "down"}`}
            style={{ "--corr": phase === "+" ? 1 : -1 } as React.CSSProperties}
            title={`${SPIN_LABELS[j]}: ${phase} in an optimal cut`}
          >
            {phase === "+" ? "+" : "−"}
          </div>
        );
      })}

      {/* Correlation with the reference, white (+1) to black (-1); its sign is the found state */}
      <div className="label row-label">{result ? `found (cut=${result.found.cut})` : "found"}</div>
      {idx.map((j) => {
        const corr = result?.corr[SPIN_LABELS[j]];
        if (corr === undefined) return <div key={`s${j}`} className="spin" />;
        return (
          <div
            key={`s${j}`}
            className={`spin ${corr >= 0 ? "up" : "down"}`}
            style={{ "--corr": corr } as React.CSSProperties}
            title={`${SPIN_LABELS[j]}: corr ${corr.toFixed(2)}`}
          >
            {corr >= 0 ? "+" : "−"}
            {Math.abs(corr).toFixed(2)}
          </div>
        );
      })}

      {/* Column labels */}
      <div className="label" />
      {idx.map((j) => (
        <div key={`c${j}`} className="label">{SPIN_LABELS[j]}</div>
      ))}

      {/* Upper-triangular problem weights (the last row would be empty) */}
      {idx.slice(0, -1).map((i) => (
        <Row key={`r${i}`} i={i} idx={idx} weights={weights} wMax={wMax} />
      ))}
    </div>
  );
}

function Row({ i, idx, weights, wMax }: { i: number; idx: number[]; weights: Map<string, number>; wMax: number }) {
  return (
    <>
      <div className="label row-key">{SPIN_LABELS[i]}</div>
      {idx.map((j) => {
        if (j <= i) return <div key={j} className="cell empty" />;
        const w = weights.get(`${i},${j}`);
        if (!w) return <div key={j} className="cell off">·</div>;
        return (
          <div
            key={j}
            className="cell on"
            style={{ "--strength": w / wMax } as React.CSSProperties}
            title={`${SPIN_LABELS[i]}–${SPIN_LABELS[j]}: weight ${w}`}
          >
            {w}
          </div>
        );
      })}
    </>
  );
}

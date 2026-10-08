import { CouplingMatrix } from "./components/CouplingMatrix";
import { StatusBar } from "./components/StatusBar";
import { useDeviceState } from "./hooks/useDeviceState";

export default function App() {
  const { presence, hello, config, state, stats } = useDeviceState();

  return (
    <main>
      <h1>Ising Machine</h1>
      <StatusBar presence={presence} hello={hello} stats={stats} seq={state?.seq} />
      <CouplingMatrix config={config} state={state} />
      {config?.preset && <p className="muted">Preset: {config.preset}</p>}
    </main>
  );
}

import { ResultView } from "./components/ResultView";
import { StatusBar } from "./components/StatusBar";
import { useResult } from "./hooks/useResult";

export default function App() {
  const { connected, result } = useResult();

  return (
    <main>
      <h1>Ising Machine</h1>
      <StatusBar connected={connected} result={result} />
      {result && <h2>{result.name}</h2>}
      <ResultView result={result} />
    </main>
  );
}

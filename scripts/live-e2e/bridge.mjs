// A short-lived connection to the bridge page of the instance, through the e2e harness the
// suites use (CDP port from FB2K_CDP_PORT, 9222 by default).
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const harness = await import(pathToFileURL(path.join(repoRoot, 'mcp', 'tests', 'lib', 'e2e-harness.mjs')).href);

export const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
export const step = (text) => console.log(`${new Date().toISOString().slice(11, 19)} ${text}`);

/**
 * Connects, checks that the bridge answers, runs `fn(inv, tryInv)` and disconnects. `inv`
 * throws on a failed call; `tryInv` returns `{ thrown }` instead.
 */
export async function connect(fn) {
  const { client } = await harness.connectBridgePage({ port: harness.resolvePort() });
  try {
    const bridge = harness.createBridge(client.Runtime, { invokeTimeoutMs: 8000 });
    await harness.requireResponsiveBridge(bridge, { probeId: 'live-e2e' });
    const inv = async (method, params) => {
      const r = await bridge.invokeRaw(method, params);
      if (r?.kind !== 'result') throw new Error(`${method}: ${JSON.stringify(r)}`);
      if (r.value && r.value.success === false) throw new Error(`${method}: ${JSON.stringify(r.value)}`);
      return r.value;
    };
    const tryInv = async (method, params) => {
      try {
        return await inv(method, params);
      } catch (e) {
        return { thrown: String(e.message || e) };
      }
    };
    return await fn(inv, tryInv);
  } finally {
    await harness.closeClient(client);
  }
}

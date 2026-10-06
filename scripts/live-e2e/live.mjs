#!/usr/bin/env node
// Live e2e runs against a real foobar2000 instance: deploy the freshly built DLL, run the e2e
// suites of mcp/tests, and leave the instance as it was found. See README.md next to this file.
//
//   node scripts/live-e2e/live.mjs run [suite ...] [--no-deploy] [--dll <file>] [--instance <folder>]
//   node scripts/live-e2e/live.mjs deploy [--dll <file>] [--instance <folder>]
//   node scripts/live-e2e/live.mjs snapshot <file> | quiet <file> | restore <file> | show
//
// `run` saves the playback state, stops playback at half the volume, deploys (unless
// --no-deploy), runs the suites through mcp/tests/run-e2e.mjs (all of them when none is named)
// and restores the state even when a suite fails. Exit code: that of run-e2e.mjs, or 1 when a
// step before it failed.
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

import { step } from './bridge.mjs';
import { deploy, resolveInstance } from './instance.mjs';
import { quiet, restore, show, snapshot } from './state.mjs';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const DEFAULT_DLL = path.join(repoRoot, 'bin', 'Release_x64', 'foo_ui_webview2.dll');

function parse(argv) {
  const opts = { positional: [], deploy: true, dll: DEFAULT_DLL, instance: undefined };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--no-deploy') opts.deploy = false;
    else if (a === '--dll') opts.dll = path.resolve(argv[++i]);
    else if (a === '--instance') opts.instance = argv[++i];
    else if (a.startsWith('--')) throw new Error(`unknown option ${a}`);
    else opts.positional.push(a);
  }
  return opts;
}

async function run(opts) {
  const inst = resolveInstance(opts.instance);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'fb2k-live-'));
  const snapFile = path.join(dir, 'snapshot.json');
  step(`run folder ${dir}`);
  await snapshot(snapFile);
  let code = 1;
  try {
    await quiet(snapFile);
    if (opts.deploy) await deploy(inst, opts.dll);
    const result = spawnSync(process.execPath, [path.join(repoRoot, 'mcp', 'tests', 'run-e2e.mjs'), ...opts.positional], {
      cwd: repoRoot,
      stdio: 'inherit',
      env: { ...process.env, FB2K_E2E_RESULTS_DIR: path.join(dir, 'results') },
    });
    code = typeof result.status === 'number' ? result.status : 1;
    step(`run-e2e exit ${code}; suite results in ${path.join(dir, 'results')}`);
  } finally {
    await restore(snapFile);
  }
  return code;
}

async function main() {
  const [command, ...rest] = process.argv.slice(2);
  const opts = parse(rest);
  const file = opts.positional[0];
  switch (command) {
    case 'run':
      return run(opts);
    case 'deploy':
      await deploy(resolveInstance(opts.instance), opts.dll);
      return 0;
    case 'snapshot':
    case 'quiet':
    case 'restore':
      if (!file) throw new Error(`${command} needs a file`);
      await { snapshot, quiet, restore }[command](file);
      return 0;
    case 'show':
      await show();
      return 0;
    default:
      console.error('usage: node scripts/live-e2e/live.mjs run [suite ...] [--no-deploy] [--dll <file>] [--instance <folder>]');
      console.error('       node scripts/live-e2e/live.mjs deploy [--dll <file>] [--instance <folder>]');
      console.error('       node scripts/live-e2e/live.mjs snapshot|quiet|restore <file> | show');
      return 2;
  }
}

main().then(
  (code) => process.exit(code),
  (e) => {
    console.error(`live-e2e: ${e.message || e}`);
    process.exit(1);
  },
);

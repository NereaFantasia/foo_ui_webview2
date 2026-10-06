// The foobar2000 instance a live run deploys into: where it is, which processes belong to it,
// and how to stop it, swap the component DLL and start it again.
//
// The instance is the folder that holds foobar2000.exe, from --instance or FB2K_LIVE_DIR. The
// component folder defaults to the portable layout, profile/user-components-x64/foo_ui_webview2;
// FB2K_LIVE_COMPONENT_DIR overrides it for an installed foobar2000.
import { execFileSync, spawn } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';

import { connect, sleep, step } from './bridge.mjs';

export function resolveInstance(instance) {
  const dir = instance ?? process.env.FB2K_LIVE_DIR;
  if (!dir) {
    throw new Error('no instance: pass --instance <folder> or set FB2K_LIVE_DIR to the foobar2000 folder a live run may restart');
  }
  const exe = path.join(dir, 'foobar2000.exe');
  if (!fs.existsSync(exe)) throw new Error(`no foobar2000.exe in ${dir}`);
  const componentDir = process.env.FB2K_LIVE_COMPONENT_DIR ?? path.join(dir, 'profile', 'user-components-x64', 'foo_ui_webview2');
  return {
    dir,
    exe,
    dll: path.join(componentDir, 'foo_ui_webview2.dll'),
    // Left behind by a killed instance; foobar2000 then opens a crash dialog on the next start
    // and the page never loads.
    runningMarker: path.join(dir, 'profile', 'running'),
  };
}

export const md5 = (file) => crypto.createHash('md5').update(fs.readFileSync(file)).digest('hex');

// PowerShell 5.1 started from a PowerShell 7 environment inherits its module path and then
// cannot load its own modules, so the variable is dropped for the child.
function powershell(command) {
  const env = { ...process.env };
  delete env.PSModulePath;
  return execFileSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', command], { encoding: 'utf8', env });
}

/** Process ids of foobar2000.exe started from this instance's folder; other instances are left alone. */
export function instancePids(inst) {
  const out = powershell("Get-CimInstance Win32_Process -Filter \"Name='foobar2000.exe'\" | ForEach-Object { \"$($_.ProcessId)`t$($_.ExecutablePath)\" }");
  const want = path.resolve(inst.exe).toLowerCase();
  return out
    .split(/\r?\n/)
    .map((line) => line.split('\t'))
    .filter(([pid, exe]) => pid && exe && path.resolve(exe).toLowerCase() === want)
    .map(([pid]) => Number(pid));
}

/** Asks the instance to exit through misc.exit and waits; kills it after 30 seconds. */
export async function stopInstance(inst) {
  if (!instancePids(inst).length) return;
  try {
    await connect(async (_inv, tryInv) => {
      await tryInv('misc.exit');
    });
  } catch (e) {
    step(`misc.exit not reachable: ${String(e.message || e).slice(0, 160)}`);
  }
  for (let i = 0; i < 60 && instancePids(inst).length; i++) await sleep(500);
  const left = instancePids(inst);
  if (left.length) {
    step(`still running after 30 s; killing ${left.join(', ')}`);
    for (const pid of left) execFileSync('taskkill', ['/F', '/PID', String(pid)]);
    await sleep(1500);
    if (fs.existsSync(inst.runningMarker)) {
      fs.unlinkSync(inst.runningMarker);
      step('removed the running marker');
    }
  }
  step('instance stopped');
}

/** Starts the instance through explorer, so it does not live in this shell's process tree, and waits for a responsive bridge. */
export async function startInstance(inst) {
  spawn('explorer.exe', [inst.exe], { detached: true, stdio: 'ignore' }).unref();
  step('started through explorer');
  for (let i = 0; i < 120; i++) {
    await sleep(1000);
    try {
      await connect(async () => {});
      step('bridge responsive');
      return;
    } catch (e) {
      if (i % 10 === 9) step(`waiting for the bridge: ${String(e.message || e).slice(0, 120)}`);
    }
  }
  throw new Error('the bridge did not answer within two minutes of the start');
}

/**
 * Stops the instance, backs up its DLL next to it, copies `dll` in, starts the instance again
 * and checks that the file in place is the one copied.
 */
export async function deploy(inst, dll) {
  if (!fs.existsSync(dll)) throw new Error(`no DLL at ${dll}; build first`);
  const want = md5(dll);
  step(`deploying ${dll} (md5 ${want})`);
  await stopInstance(inst);
  if (fs.existsSync(inst.dll)) {
    const stamp = new Date().toISOString().replace(/[-:]/g, '').replace(/\..*/, '').replace('T', '-');
    const backup = `${inst.dll}.bak-${stamp}`;
    fs.copyFileSync(inst.dll, backup);
    step(`backed up the previous DLL as ${path.basename(backup)} (md5 ${md5(backup)})`);
  }
  fs.copyFileSync(dll, inst.dll);
  const got = md5(inst.dll);
  if (got !== want) throw new Error(`copied DLL has md5 ${got}, expected ${want}`);
  await startInstance(inst);
  return want;
}

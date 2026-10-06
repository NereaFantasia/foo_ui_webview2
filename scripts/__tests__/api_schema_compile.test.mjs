// scripts/__tests__/api_schema_compile.test.mjs
//
// Compiles what the C++ emitter produces for every shape the schema accepts (extends,
// Partial, recursion in its three forms, a struct reached only through a reference, shared
// types, maps, a struct reached from a result first and from params later, event payloads
// and their descriptors, a shared type as a payload, a namespace that takes a trailing
// underscore in C++, the registry of all events) with the compiler the product uses, so
// a shape the emitter mishandles fails here instead of in the build of the namespace that
// first uses it. Skipped where Visual Studio with the C++ toolset is not installed; the
// harness proves on every run that it does compile, by also compiling a probe that must
// fail, and that warnings count as errors, by compiling one that only warns.

import { test, describe, after } from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseNamespace } from '../api-schema/schema.mjs';
import { cppHeaderPath, emitCppHeader, emitEventRegistry, EVENT_REGISTRY_PATH } from '../api-schema/emit-cpp.mjs';

const REPO = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const COMMON_FILE = 'src/api/schema/common.ts';
const DEMO_FILE = 'src/api/schema/demo.json';

function findVcvars() {
  if (process.platform !== 'win32') return null;
  const vswhere = path.join(process.env['ProgramFiles(x86)'] ?? 'C:\\Program Files (x86)', 'Microsoft Visual Studio', 'Installer', 'vswhere.exe');
  if (!fs.existsSync(vswhere)) return null;
  let vs = '';
  try {
    vs = execFileSync(vswhere, ['-latest', '-products', '*', '-prerelease', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], { encoding: 'utf8' }).trim();
  } catch {
    return null;
  }
  const vcvars = vs && path.join(vs, 'VC', 'Auxiliary', 'Build', 'vcvars64.bat');
  return vcvars && fs.existsSync(vcvars) ? vcvars : null;
}

// Syntax-checks one translation unit with the options the product and test projects share:
// C++20, /utf-8, /permissive-, warning level 3 with warnings as errors. The preprocessor is
// the compiler's default, which is what the CI toolset uses as well.
function compile(vcvars, includeDir, probe) {
  const includes = [includeDir, path.join(REPO, 'tests'), path.join(REPO, 'src'), path.join(REPO, 'lib', 'json', 'include')];
  const cl = ['cl', '/nologo', '/Zs', '/std:c++20', '/utf-8', '/EHsc', '/permissive-', '/W3', '/WX', ...includes.map((d) => `/I"${d}"`), `"${probe}"`].join(' ');
  const line = `""${vcvars}" >nul 2>&1 && ${cl}"`;
  const run = spawnSync('cmd.exe', ['/s', '/c', line], { encoding: 'utf8', windowsVerbatimArguments: true, timeout: 180000 });
  return { status: run.status, output: `${run.stdout ?? ''}${run.stderr ?? ''}` };
}

const str = (description) => ({ type: 'string', description });
const int = (description) => ({ type: 'integer', description });
const arr = (items, description) => ({ type: 'array', items, description });
const named = (name, properties, required, extra = {}) => ({ type: 'object', 'x-name': name, additionalProperties: false, required, properties, ...extra });
const ref = (name, common = false) => ({ type: 'object', 'x-ref': name, ...(common ? { 'x-common': true } : {}) });
const top = (properties, required) => ({ type: 'object', additionalProperties: false, required, properties });
const method = (params, result) => ({ description: 'Demo.', ...(params ? { params } : {}), ...(result ? { result } : {}) });

const TRACK = named('Track', { path: str('Path.'), title: str('Title.') }, ['path', 'title'], { 'x-common': true });
const TRACK_PARTIAL = named('TrackPartial', { path: str('Path.'), title: str('Title.') }, [], { 'x-common': true, 'x-partial-of': 'Track' });
const NULLABLE = named('Nullable', { note: { type: 'string', nullable: true, description: 'Note.' } }, ['note'], { 'x-common': true });
const MENU_ITEM = named('MenuItem', { label: str('Label.'), submenu: arr(ref('MenuItem', true), 'Submenu.') }, ['label'], { 'x-common': true });

const BASE = named('Base', { id: int('Id.') }, ['id']);
const ROW = named('Row', { note: str('Note.') }, ['note'], { 'x-extends': 'Base' });
const ROW_PARTIAL = named('RowPartial', { id: int('Id.'), note: str('Note.') }, [], { 'x-partial-of': 'Row' });
const PLAYLIST_ROW = named('PlaylistRow', { index: int('Row number.') }, ['index'], { 'x-extends': 'Track', 'x-extends-common': true });
const PLAYLIST_ROW_COLUMNS = { type: 'object', 'x-name': 'PlaylistRowColumns', 'x-extends': 'PlaylistRow', additionalProperties: false };
// A struct with an anonymous nested object, reached from a result before params.
const ITEM = named('Item', { meta: { type: 'object', additionalProperties: false, required: ['n'], properties: { n: int('N.') }, description: 'Meta.' } }, ['meta']);
// Recursion through an anonymous object, and mutual recursion through arrays.
const NODE = named('Node', { label: str('Label.'), child: { type: 'object', additionalProperties: false, required: ['items'], properties: { items: arr(ref('Node'), 'Items.') }, description: 'Child.' } }, ['label']);
const B = named('B', { as: arr(ref('A'), 'As.') }, ['as']);
const A = named('A', { bs: arr(B, 'Bs.') }, ['bs']);
// Derived from a result-side base, itself used in params only.
const DERIVED = named('Derived', { extra: str('Extra.') }, ['extra'], { 'x-extends': 'Base' });
// C is reached only through D's reference: from params in one method and from a result in
// another, so C needs a parser and a writer it gets from nowhere else. C also derives from
// Base, which params use directly: without C's own parser the compiler would silently pick
// Base's for a C, and D's parser would read a C as a Base.
const D = named('D', { cs: arr(ref('C'), 'Cs.') }, ['cs']);
const C = named('C', { ds: arr(D, 'Ds.') }, ['ds'], { 'x-extends': 'Base' });

function fixture() {
  const common = parseNamespace({ namespace: 'common', methods: {}, types: { Track: TRACK, TrackPartial: TRACK_PARTIAL, Nullable: NULLABLE, MenuItem: MENU_ITEM } }, COMMON_FILE);
  const demo = parseNamespace({
    namespace: 'demo',
    methods: {
      list: method(undefined, top({
        rows: arr(PLAYLIST_ROW, 'Rows.'),
        tracks: arr({ ...TRACK, description: 'Tracks.' }, 'Tracks.'),
        partial: arr(TRACK_PARTIAL, 'Partial rows.'),
        menu: arr(MENU_ITEM, 'Menu.'),
        nullable: arr(NULLABLE, 'Nullable.'),
        extra: arr({ ...PLAYLIST_ROW_COLUMNS, additionalProperties: { type: 'string' } }, 'Rows with columns.'),
        items: arr(ITEM, 'Items.'),
        tree: arr(NODE, 'Tree.'),
        pair: arr(A, 'Pair.'),
        sparse: arr(ROW_PARTIAL, 'Sparse rows.'),
        local: arr(ROW, 'Local rows.'),
        byId: { type: 'object', additionalProperties: ITEM, description: 'By id.' },
        viaRef: arr(C, 'Reached through D.'),
      }, ['rows', 'tracks', 'partial', 'menu', 'nullable', 'extra', 'items', 'tree', 'pair', 'sparse', 'local', 'byId', 'viaRef'])),
      add: method(top({
        items: arr(ITEM, 'Items.'),
        tree: arr(NODE, 'Tree.'),
        pair: arr(A, 'Pair.'),
        rows: arr(ROW, 'Rows.'),
        derived: arr(DERIVED, 'Derived.'),
        // An array of objects whose `path` member the bridge checks before the handler.
        entries: { ...arr(named('Entry', { path: str('Path.'), note: str('Note.') }, ['path']), 'Entries.'), 'x-security': 'MediaRead', 'x-path-key': 'path' },
        // An optional array with a floor: the reader needs the std::optional overload of MinItems.
        tags: { ...arr({ type: 'string' }, 'Tags.'), minItems: 1 },
        // An array of objects whose two members the bridge checks at different levels.
        moves: { ...arr(named('Move', { source: str('Source.'), destination: str('Destination.') }, ['source', 'destination']), 'Moves.'), 'x-path-key': { source: 'FileWrite', destination: 'Read' } },
        byId: { type: 'object', additionalProperties: { type: 'object', additionalProperties: false, required: ['n'], properties: { n: int('N.') } }, description: 'By id.' },
        sort: { type: 'string', enum: ['path', 'title'], default: 'path', description: 'Sort key.' },
        // A key C++ cannot name: the member is `default_`.
        default: str('Fallback.'),
        // Any JSON per key, and an object member that may hold any JSON.
        fields: { type: 'object', additionalProperties: { type: 'json' }, description: 'Fields.' },
        patches: arr(named('Patch', { path: str('Path.'), value: { type: 'json', description: 'Value.' } }, ['path']), 'Patches.'),
      }, ['items', 'tree', 'pair', 'rows', 'derived', 'byId', 'fields']), top({ count: int('Count.'), json: str('The same as text.') }, ['count', 'json'])),
      link: method(top({ d: arr(D, 'D.') }, ['d']), top({ d: arr(D, 'D.') }, ['d'])),
      ping: method(),
    },
    // Payloads reuse the result machinery: shared and derived rows, a nullable field, none at all.
    events: {
      changed: { description: 'Changed.', 'x-delivery': 'broadcast', payload: top({ rows: arr(PLAYLIST_ROW, 'Rows.'), tracks: arr({ ...TRACK, description: 'Tracks.' }, 'Tracks.'), note: { type: 'string', nullable: true, description: 'Note.' } }, ['rows', 'note']) },
      reset: { description: 'Reset.', 'x-delivery': 'caller' },
      frame: { description: 'Frame.', 'x-delivery': 'owner', 'x-custom-name': true, payload: top({ success: { type: 'boolean', description: 'Worked.' } }, ['success']) },
      // A shared type of common.ts as the whole payload.
      moved: { description: 'Moved.', 'x-delivery': 'window', 'x-payload-type': 'Track' },
    },
    types: { Base: BASE, Row: ROW, RowPartial: ROW_PARTIAL, PlaylistRow: PLAYLIST_ROW, PlaylistRowColumns: PLAYLIST_ROW_COLUMNS, Item: ITEM, Node: NODE, A: A, B: B, C: C, D: D, Derived: DERIVED, Track: TRACK, TrackPartial: TRACK_PARTIAL, Nullable: NULLABLE, MenuItem: MENU_ITEM },
  }, DEMO_FILE);
  // A namespace named like one the generated code uses: its C++ namespace is api::api_, and the
  // code inside it still reaches api::results and api::common.
  const apiNs = parseNamespace({
    namespace: 'api',
    events: { registered: { description: 'Registered.', 'x-delivery': 'broadcast', payload: top({ name: str('Name.'), tracks: arr({ ...TRACK, description: 'Tracks.' }, 'Tracks.') }, ['name', 'tracks']) } },
    types: { Track: TRACK },
  }, 'src/api/schema/api.json');
  return { common, demo, apiNs, namespaces: [common, demo, apiNs] };
}

const PROBE = `#include "compat/fb2k_types.h"
#include "api/generated/CommonSchema.h"
#include "api/generated/DemoSchema.h"
#include "api/generated/ApiSchema.h"
#include "api/generated/EventRegistry.h"

int main() {
    api::demo::AddParams params;
    std::string error;
    const bool ok = FromJson(nlohmann::json::object(), params, error);
    api::demo::ListResult result;
    result.menu.push_back(api::common::MenuItem{});
    const nlohmann::json j = ToJson(result);
    // A C is parsed and written as a C, not as the Base it derives from.
    api::demo::C c;
    static_assert(std::is_same_v<decltype(FromJson(nlohmann::json::object(), c, error)), bool>, "C has its own parser");
    const nlohmann::json cj = ToJson(c);
    static_assert(api::demo::Derived::kFields.size() == 2, "a derived struct lists its own keys");
    static_assert(api::demo::AddParams::kPathParams.size() == 3 && api::demo::AddParams::kPathParams[0].nestedKey != nullptr, "the object array names its path member");
    static_assert(api::demo::AddParams::kPathParams[1].access == api::params::PathAccess::FileWrite && api::demo::AddParams::kPathParams[2].access == api::params::PathAccess::Read && api::demo::AddParams::kPathParams[2].nestedKey != nullptr, "per-member levels expand in declaration order");
    static_assert(std::is_same_v<decltype(params.fields), std::map<std::string, nlohmann::json>>, "a Record of Json reads as a map of json");
    static_assert(std::is_same_v<decltype(api::demo::Patch::value), std::optional<nlohmann::json>>, "an optional Json member reads as an optional json");
    // An event descriptor names its payload struct; the payload is written like a result.
    static_assert(std::is_same_v<api::demo::events::Changed::Payload, api::demo::ChangedPayload>, "the descriptor names its payload");
    static_assert(!api::demo::events::Changed::kCustomName && api::demo::events::Frame::kCustomName, "the custom-name flag is carried");
    static_assert(api::demo::ChangedPayload::kFields.size() == 3 && api::demo::ResetPayload::kFields.empty(), "payloads list their keys");
    static_assert(std::is_same_v<std::tuple_element_t<2, api::events::All>, api::demo::events::Frame> && std::tuple_size_v<api::events::All> == 5, "the registry lists every event");
    static_assert(std::is_same_v<api::demo::events::Moved::Payload, api::common::Track>, "a shared payload is the common struct itself");
    static_assert(std::is_same_v<std::tuple_element_t<4, api::events::All>, api::api_::events::Registered>, "the api namespace is api::api_");
    const nlohmann::json mj = ToJson(api::demo::events::Moved::Payload{});
    api::api_::RegisteredPayload registered;
    registered.tracks.push_back(api::common::Track{});
    const nlohmann::json aj = ToJson(registered);
    api::demo::ChangedPayload changed;
    changed.rows.push_back(api::demo::PlaylistRow{});
    const nlohmann::json ej = ToJson(changed);
    const nlohmann::json rj = ToJson(api::demo::ResetPayload{});
    const std::string_view name = api::demo::events::Frame::kName;
    return ok && j.is_object() && cj.is_object() && ej.is_object() && rj.is_object() && mj.is_object() && aj.is_object() && name == "demo:frame" ? 0 : 1;
}
`;

describe('api-schema · the generated C++ compiles', () => {
  const vcvars = findVcvars();
  const skip = vcvars ? false : 'Visual Studio with the C++ toolset is not installed';
  let root = null;
  const scratch = () => {
    if (!root) {
      root = fs.mkdtempSync(path.join(os.tmpdir(), 'api-schema-compile-'));
      fs.mkdirSync(path.join(root, 'api', 'generated'), { recursive: true });
    }
    return root;
  };
  after(() => {
    if (root) fs.rmSync(root, { recursive: true, force: true });
  });

  test('every accepted shape compiles as one translation unit', { skip }, () => {
    const { common, demo, apiNs, namespaces } = fixture();
    const generated = path.join(scratch(), 'api', 'generated');
    for (const ns of [common, demo, apiNs]) fs.writeFileSync(path.join(generated, path.basename(cppHeaderPath(ns))), emitCppHeader(ns, namespaces));
    fs.writeFileSync(path.join(generated, path.basename(EVENT_REGISTRY_PATH)), emitEventRegistry(namespaces));
    // The reference is C's only route to params and to the writer: both must exist.
    const header = fs.readFileSync(path.join(generated, 'DemoSchema.h'), 'utf8');
    assert.match(header, /inline bool FromJson\(const json& j, C& out, std::string& error, const std::string& where\) \{/);
    assert.match(header, /inline json ToJson\(const C& v\) \{/);
    const probe = path.join(scratch(), 'probe.cpp');
    fs.writeFileSync(probe, PROBE);
    const { status, output } = compile(vcvars, scratch(), probe);
    assert.equal(status, 0, `cl rejected the generated headers:\n${output}`);
  });

  test('the harness reports a translation unit that does not compile', { skip }, () => {
    const probe = path.join(scratch(), 'broken.cpp');
    fs.writeFileSync(probe, '#include "api/ApiParams.h"\nint main() { int x = "not an int"; return x; }\n');
    const { status, output } = compile(vcvars, scratch(), probe);
    assert.notEqual(status, 0, 'cl accepted a translation unit with a type error');
    assert.match(output, /C2440|C4047|error/);
  });

  test('the harness treats a warning as an error', { skip }, () => {
    // C4101 (unreferenced local variable) is a level-3 warning: it fails only under /WX.
    const probe = path.join(scratch(), 'warns.cpp');
    fs.writeFileSync(probe, 'int main() { int unused; return 0; }\n');
    const { status, output } = compile(vcvars, scratch(), probe);
    assert.notEqual(status, 0, 'cl accepted a translation unit that warns');
    assert.match(output, /C4101/);
  });
});

// scripts/__tests__/gen_sdk_doc_signatures.test.mjs
//
// The signature lines of the SDK pages are rendered from sdk/src. These tests pin the line
// parser, and render a few real facade methods to show which parts come from the SDK and
// which from the page.
//
// Run:
//   node --test scripts/__tests__/gen_sdk_doc_signatures.test.mjs

import { test, describe, before } from 'node:test';
import assert from 'node:assert/strict';

import {
  createSignatureRenderer,
  parameterNames,
  parseSignatureLine,
} from '../gen_sdk_doc_signatures.mjs';

describe('parseSignatureLine', () => {
  test('reads both spellings and keeps what follows the code span', () => {
    const en = parseSignatureLine('Signature: `fb.tray.destroy(): Promise<TrayDestroyResponse>` (panel mode fails)');
    assert.equal(en.namespace, 'tray');
    assert.equal(en.method, 'destroy');
    assert.equal(en.suffix, ' (panel mode fails)');
    const zh = parseSignatureLine('签名：`fb.library.addToPlaylist(paths: string[], playlist?: PlaylistRef): Promise<X>`');
    assert.equal(zh.prefix, '签名：');
    assert.deepEqual(zh.names, ['paths', 'playlist']);
  });

  test('ignores lines that are not signature lines', () => {
    assert.equal(parseSignatureLine('await fb.tray.destroy();'), null);
    assert.equal(parseSignatureLine('Signature: fb.tray.destroy()'), null);
  });
});

describe('parameterNames', () => {
  test('splits only at top-level commas, with nested generics, objects and arrow types', () => {
    assert.deepEqual(
      parameterNames(
        "fb.x.y(a: Omit<P, 'k' | 'l'>, b?: { c: number; d: string }, handler: (e: E, f: F) => void, ...rest: R[]): Z",
      ),
      ['a', 'b', 'handler', 'rest'],
    );
  });

  test('an empty list has no names, and a list it cannot read gives null', () => {
    assert.deepEqual(parameterNames('fb.x.y(): Z'), []);
    assert.equal(parameterNames('fb.x.y({ a }: P): Z'), null);
  });
});

describe('createSignatureRenderer against sdk/src', () => {
  let render;
  before(() => {
    render = createSignatureRenderer();
  });

  test('types come from the SDK, parameter names from the page when the counts agree', () => {
    const kept = render('library', 'addToPlaylist', ['paths', 'playlistIndex']);
    assert.match(kept.code, /^fb\.library\.addToPlaylist\(paths: string\[\], playlistIndex\?: PlaylistRef\): Promise<\w+>$/);
    const own = render('library', 'addToPlaylist', ['onlyOne']);
    assert.match(own.code, /^fb\.library\.addToPlaylist\(paths: string\[\], playlist\?: PlaylistRef\)/);
  });

  test('an overloaded method is shown by its implementation signature', () => {
    const r = render('http', 'get');
    assert.equal(r.problem, undefined);
    assert.match(r.code, /^fb\.http\.get\(url: string, opts\?: HttpRequestOptions\): Promise<HttpResponse \| HttpBinaryResponse>$/);
  });

  test('a namespace or method the SDK does not have is a problem, not a rendering', () => {
    assert.match(render('nope', 'x').problem, /fb\.nope does not exist/);
    assert.match(render('tray', 'noSuchMethod').problem, /fb\.tray\.noSuchMethod does not exist/);
  });
});

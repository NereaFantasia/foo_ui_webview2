// English type reference for the SDK's public entries, generated from the SDK sources on every
// docs build (`prebuild` / `predev`). The output lands in public/ so VitePress copies it as
// static HTML instead of rendering hundreds of extra pages; it is git-ignored.
import { OptionDefaults } from 'typedoc';

// Tags read by scripts/gen_sdk_types.mjs; they carry no meaning for readers.
const CODEGEN_TAGS = ['@codegen-override', '@codegen-snapshot'];

/** @type {Partial<import('typedoc').TypeDocOptions>} */
export default {
  name: 'foo-webview-sdk',
  entryPoints: [
    '../../sdk/src/index.ts',
    '../../sdk/src/components/index.ts',
    '../../sdk/src/smp/index.ts',
  ],
  tsconfig: '../../sdk/tsconfig.json',
  out: 'public/sdk-reference',
  readme: 'none',
  excludePrivate: true,
  excludeInternal: true,
  blockTags: [...OptionDefaults.blockTags, ...CODEGEN_TAGS],
  excludeTags: [...OptionDefaults.excludeTags, ...CODEGEN_TAGS],
  // Most generated params/response properties have no per-field comment; listing each one
  // would bury the warnings that matter (broken links, missing exports).
  validation: { notDocumented: false, notExported: true, invalidLink: true },
};

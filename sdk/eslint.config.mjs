import tseslint from 'typescript-eslint';

// Conditions on a type position holding `unknown`, `any` or `{}`, which say nothing about the value.
const looseAt = (path) => [
    `[${path}.type=/^TS(Unknown|Any)Keyword$/]`,
    `[${path}.type='TSTypeLiteral'][${path}.members.length=0]`,
];

// Loose types where the SDK states its contract: exported shapes, response promises, bridge calls.
const looseTypes = [
    ...looseAt('typeAnnotation.typeAnnotation').map((loose) => ({
        selector: `TSIndexSignature[parameters.0.typeAnnotation.typeAnnotation.type='TSStringKeyword']${loose}`,
        message: 'A catch-all index signature hides the contract; declare the keys.',
    })),
    ...looseAt('typeArguments.params.1').map((loose) => ({
        selector: `TSTypeReference[typeName.name='Record'][typeArguments.params.0.type='TSStringKeyword']${loose}`,
        message: 'Record<string, unknown | any | {}> is a catch-all index signature; declare the keys.',
    })),
    ...looseAt('typeArguments.params.0').map((loose) => ({
        selector: `TSTypeReference[typeName.name='Promise']${loose}`,
        message: 'Use the concrete response type instead of a loose Promise type.',
    })),
    ...looseAt('typeArguments.params.0').map((loose) => ({
        selector: `CallExpression:matches([callee.name=/^(invoke|on)$/], [callee.property.name=/^(invoke|on)$/])${loose}`,
        message: 'Give the bridge call a concrete type argument, or use call() / subscribe().',
    })),
];

export default tseslint.config(
    { ignores: ['dist/', '**/*.test.ts', '**/__tests__/**'] },
    {
        files: ['src/**/*.ts'],
        languageOptions: {
            parser: tseslint.parser,
            parserOptions: { projectService: true, tsconfigRootDir: import.meta.dirname },
        },
        plugins: { '@typescript-eslint': tseslint.plugin },
        // The source carries `no-console` directives for rules this config does not enable.
        linterOptions: { reportUnusedDisableDirectives: 'off' },
        rules: {
            // Bridge calls are promises; one left unhandled loses its failure silently.
            '@typescript-eslint/no-floating-promises': 'error',
            '@typescript-eslint/no-misused-promises': 'error',
            '@typescript-eslint/await-thenable': 'error',
            'no-restricted-syntax': ['error', ...looseTypes],
        },
    },
);

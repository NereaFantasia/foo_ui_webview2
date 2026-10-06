// Narrowing helpers for tests that read fields off a bridge response. Responses
// are `XxxSuccess | ApiFailure`, so a test has to settle which branch it got
// before the compiler lets it read the fields of that branch.
//
// No vitest import: this file sits outside the *.test.ts exclusion and is also
// type-checked with the published sources.

/** Throws unless `res` is the success branch; afterwards `res` is narrowed to it. */
export function expectSuccess<T extends { success: boolean }>(
    res: T,
): asserts res is Extract<T, { success: true }> {
    if (res.success !== true) {
        throw new Error(`expected a success response, got ${JSON.stringify(res)}`);
    }
}

/** Throws unless `res` is the failure branch; afterwards `res` is narrowed to it. */
export function expectFailure<T extends { success: boolean }>(
    res: T,
): asserts res is Extract<T, { success: false }> {
    if (res.success !== false) {
        throw new Error(`expected a failure response, got ${JSON.stringify(res)}`);
    }
}

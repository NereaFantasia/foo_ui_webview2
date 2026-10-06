// Checks the parameter keys of every declared host call a test makes, by the rules the host
// applies before a handler runs (`findParamKeyProblem` of `foo-webview-sdk/schema`). The type
// check does not see a key that reaches the params through a spread or a variable of a wider
// type, and the real host refuses such a call with INVALID_PARAMS; this makes the test that
// exercises the call fail instead. Only calls the tests actually make are checked.
//
// The wrap sits on `typedCall`, which the facades, the components and the SMP layer use for
// their declared calls; tests replace `bridge.invoke` and `window.fb2k` with stubs of their
// own, so a wrap there would not survive. Not checked: undeclared methods, the `fb.invoke` and
// `smp.invoke` escape hatches, and the SMP `NotifyOthers` call to `window.broadcast`, which
// goes to the native bridge directly.
//
// Problems fail the test from `afterEach` rather than being thrown at the call, where a caller
// that catches errors would hide them. A call made after its test ended is reported by the
// next test, or by `afterAll` when it lands within one macrotask after the file's last test;
// under concurrent tests a problem can be reported by a test other than the one that made it.
import { afterAll, afterEach, vi } from 'vitest';

const problems = vi.hoisted((): string[] => []);

vi.mock('./src/utils/typedCall.js', async (importOriginal) => {
    const actual = await importOriginal<typeof import('./src/utils/typedCall.js')>();
    const { findParamKeyProblem } = await import('./src/schema/index.js');
    return {
        ...actual,
        typedCall: <R>(invoke: (method: string, params?: object) => Promise<R>) =>
            actual.typedCall((...args: [method: string, params?: object]) => {
                const [method, params] = args;
                const problem = findParamKeyProblem(method, params);
                if (problem) problems.push(`${method}: ${problem.reason} '${problem.path || '(params)'}'`);
                // Forward the arguments as received: a call without params reaches invoke with one.
                return invoke(...args);
            }),
    };
});

function reportProblems(): void {
    if (problems.length === 0) return;
    const found = problems.splice(0);
    throw new Error(`The host would refuse these calls for their parameter keys:\n  ${found.join('\n  ')}`);
}

afterEach(reportProblems);
// Registered first, so it runs after the test file's own `afterAll` hooks. One macrotask turn on
// real timers lets a call queued behind the last test land before the file is closed.
afterAll(async () => {
    vi.useRealTimers();
    await new Promise((resolve) => setTimeout(resolve, 0));
    reportProblems();
});

import { call } from '../call.js';
import type {
    JitSessionCreateParams, JitSessionGetStateParams, JitSessionClaimControlParams,
    JitSessionPlayParams, JitSessionSetNextParams, JitSessionSkipParams,
    JitSessionStopParams, JitSessionGetOperationParams, JitSessionReleaseParams,
    JitSessionPrepareAdoptionParams, JitSessionGetNativeContextParams, JitSessionAdoptNativeParams,
    JitSessionPlayNativeParams, JitSessionSetNativeNextParams, JitSessionSetNativeQueueParams,
} from '../../types/generated/params.js';

/** Control a document-owned JIT playback session; methods return host response envelopes. */
export const jitSession = {
    /** Read protocol capabilities before requesting playback or native adoption. */
    getCapabilities: () => call('jitSession.getCapabilities'),
    /** Acquire a session without changing playback; retry with the same requestId. */
    create: (params: JitSessionCreateParams) => call('jitSession.create', params),
    /** Find a resumable session without granting control or returning a token. */
    discover: () => call('jitSession.discover'),
    /** Read a consistent snapshot without the control token. */
    getState: (params: JitSessionGetStateParams) => call('jitSession.getState', params),
    /** Resume after a reload or hand over using the observed ownerEpoch, without replaying. */
    claimControl: (params: JitSessionClaimControlParams) => call('jitSession.claimControl', params),
    /** Accept a new playback attempt; operationCompleted reports its execution result. */
    play: (params: JitSessionPlayParams) => call('jitSession.play', params),
    /** Replace the next entry, or omit entryId to withdraw it while the current entry continues. */
    setNext: (params: JitSessionSetNextParams) => call('jitSession.setNext', params),
    /**
     * Skip to a prepared next entry, preserving the native queue. Without one, returns
     * NO_ACTIVE_ITEM without changing the current attempt or revision.
     */
    skip: (params: JitSessionSkipParams) => call('jitSession.skip', params),
    /** Stop playback and pending operations while retaining the session for queries. */
    stop: (params: JitSessionStopParams) => call('jitSession.stop', params),
    /** Read pending operations or the latest 64 finished records within the retention period. */
    getOperation: (params: JitSessionGetOperationParams) => call('jitSession.getOperation', params),
    /** Release control and the buffer without stopping the resource currently decoding. */
    release: (params: JitSessionReleaseParams) => call('jitSession.release', params),
    /**
     * Snapshot native playback and its queue for a later adoption without changing either; the
     * snapshot token expires after 30 seconds. Adoption itself is not available yet.
     * @experimental
     */
    prepareAdoption: (params: JitSessionPrepareAdoptionParams) => call('jitSession.prepareAdoption', params),
    /**
     * Read one page of a native context's queue occurrences or source rows; reading changes no
     * revision.
     * @experimental
     */
    getNativeContext: (params: JitSessionGetNativeContextParams) => call('jitSession.getNativeContext', params),
    /**
     * Adopt a prepared snapshot without interrupting the track playing; stopping or releasing the
     * session gives the remaining queue occurrences back to native playback.
     * @experimental
     */
    adoptNative: (params: JitSessionAdoptNativeParams) => call('jitSession.adoptNative', params),
    /**
     * Play an adopted source row or queue occurrence now; starting a queue occurrence consumes it.
     * @experimental
     */
    playNative: (params: JitSessionPlayNativeParams) => call('jitSession.playNative', params),
    /**
     * Buffer an adopted source row or queue occurrence after the current attempt.
     * @experimental
     */
    setNativeNext: (params: JitSessionSetNativeNextParams) => call('jitSession.setNativeNext', params),
    /**
     * Keep and reorder the queue occurrences still to be given back; the ones left out are removed.
     * @experimental
     */
    setNativeQueue: (params: JitSessionSetNativeQueueParams) => call('jitSession.setNativeQueue', params),
};

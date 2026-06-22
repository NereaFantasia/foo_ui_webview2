// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/index.ts
// Source: docs/graph/auto/code-index.json
// Emitter: scripts/gen_sdk_types.mjs --all
// schema_version: auto-1.0.0
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

// ── Barrel re-export ─────────────────────────────────────────────────────
export * from './params.js';
export * from './responses.js';
export * from './events.js';
// ── API-method map (tuple of [Params, Response] per handler) ──────────────
/**
 * The master `api_name` → `[Params, Response]` tuple map.
 *
 * Consumed by the typed `bridge.invoke<M extends keyof ApiMethodMap>(m, p)
 * → Promise<ApiMethodMap[M][1]>` overload. Dynamic dispatch (string method
 * not known at compile-time) falls back to the permissive overload
 * declared in the bridge core — see PLAN §12.4.
 */
import type {
    ArtworkGetAvailableArtworkParams,
    ArtworkGetAvailableTypesParams,
    ArtworkGetBatchParams,
    ArtworkGetByPathParams,
    ArtworkGetByPlaylistItemParams,
    ArtworkGetCurrentParams,
    ArtworkGetFb2kUrlParams,
    ArtworkGetFb2kUrlByPathParams,
    ArtworkGetFb2kUrlByPathBatchParams,
    ArtworkGetFolderImagesParams,
    ArtworkGetForTrackParams,
    ArtworkGetLyricsParams,
    ArtworkGetMetadataParams,
    AudioAnalyzeBPMParams,
    AudioGenerateFullWaveformParams,
    AudioGenerateWaveformParams,
    AudioGetOutputInfoParams,
    AudioGetSpectrumParams,
    AudioGetSpectrumDebugStateParams,
    AudioGetStreamInfoParams,
    AudioGetWaveformParams,
    AudioIsVisualizationAvailableParams,
    AudioSetChannelModeParams,
    AudioSubscribeSpectrumParams,
    AudioSubscribeStreamParams,
    AudioUnsubscribeSpectrumParams,
    AudioUnsubscribeStreamParams,
    ClipboardReadParams,
    ClipboardWriteParams,
    ClipboardWriteFilesParams,
    ClipboardWriteHTMLParams,
    ConfigExportParams,
    ConfigGetParams,
    ConfigGetActiveDspPresetParams,
    ConfigGetAdvancedConfigParams,
    ConfigGetAdvancedConfigValueParams,
    ConfigGetAllParams,
    ConfigGetComponentsParams,
    ConfigGetCursorFollowPlaybackParams,
    ConfigGetDspPresetsParams,
    ConfigGetLibraryFilePatternsParams,
    ConfigGetLibraryStatusParams,
    ConfigGetOutputConfigParams,
    ConfigGetOutputDevicesParams,
    ConfigGetPlaybackFollowCursorParams,
    ConfigGetPreferencesPagesParams,
    ConfigGetPreferencesStandardGuidsParams,
    ConfigGetReplaygainModeParams,
    ConfigGetVersionInfoParams,
    ConfigRemoveParams,
    ConfigResetAdvancedConfigParams,
    ConfigSetParams,
    ConfigSetActiveDspPresetParams,
    ConfigSetAdvancedConfigValueParams,
    ConfigSetCursorFollowPlaybackParams,
    ConfigSetOutputBufferParams,
    ConfigSetOutputDeviceParams,
    ConfigSetPlaybackFollowCursorParams,
    ConfigSetReplaygainModeParams,
    ConfigShowLibraryPreferencesParams,
    ConsoleErrorParams,
    ConsoleLogParams,
    ConsoleWarnParams,
    CursorIsHiddenParams,
    CursorSetHiddenParams,
    DialogConfirmParams,
    DialogOpenFileParams,
    DialogOpenFolderParams,
    DialogSaveFileParams,
    DiscoveryExecuteContextMenuByPathParams,
    DiscoveryExecuteContextMenuCommandParams,
    DiscoveryExecuteMainMenuCommandParams,
    DiscoveryGetAllServicesParams,
    DiscoveryGetComponentsParams,
    DiscoveryGetContextMenuCommandsParams,
    DiscoveryGetContextMenuTreeParams,
    DiscoveryGetDspEntriesParams,
    DiscoveryGetInputFormatsParams,
    DiscoveryGetMainMenuCommandsParams,
    DiscoveryGetMainMenuGroupsParams,
    DiscoveryGetOutputDevicesParams,
    DiscoveryGetPreferencePagesParams,
    DiscoveryGetUIElementsParams,
    DiscoverySearchCommandsParams,
    DndGetDropZonesParams,
    DndRegisterDropZoneParams,
    DndStartDragParams,
    DndUnregisterDropZoneParams,
    DspAddDspParams,
    DspApplyPresetParams,
    DspGetAvailableParams,
    DspGetChainParams,
    DspGetPresetsParams,
    DspMoveDspParams,
    DspRemoveDspParams,
    DspSetChainParams,
    EventEmitParams,
    EventEmitToParams,
    FileCopyParams,
    FileDeleteParams,
    FileExistsParams,
    FileGetInfoParams,
    FileListParams,
    FileMkdirParams,
    FileMoveParams,
    FileReadParams,
    FileRenameParams,
    FileWriteParams,
    HttpAbortParams,
    HttpDeleteParams,
    HttpDownloadParams,
    HttpGetParams,
    HttpHeadParams,
    HttpPatchParams,
    HttpPostParams,
    HttpPutParams,
    JitQueueClearParams,
    JitQueueEnqueueNextParams,
    JitQueueGetStateParams,
    JitQueueNotifyEmptyParams,
    JitQueuePlayNowParams,
    JitQueuePreloadBatchParams,
    JitQueueSkipParams,
    JitQueueStopParams,
    KeyboardGetRegisteredHotkeysParams,
    KeyboardRegisterHotkeyParams,
    KeyboardRegisterShortcutParams,
    KeyboardUnregisterHotkeyParams,
    LibraryAddToPlaylistParams,
    LibraryBrowseDirectoryParams,
    LibraryBrowseTreeParams,
    LibraryGetAlbumsParams,
    LibraryGetAlbumTracksParams,
    LibraryGetAllParams,
    LibraryGetArtistAlbumsParams,
    LibraryGetArtistsParams,
    LibraryGetArtistTracksParams,
    LibraryGetByPathParams,
    LibraryGetCacheStatsParams,
    LibraryGetCountParams,
    LibraryGetFieldValuesParams,
    LibraryGetGenresParams,
    LibraryGetRandomTracksParams,
    LibraryGetRecentlyAddedParams,
    LibraryGetRootsParams,
    LibraryGetStatsParams,
    LibraryGetStatusParams,
    LibraryInvalidateCacheParams,
    LibraryIsEnabledParams,
    LibraryQueryParams,
    LibraryRefreshParams,
    LibraryRescanParams,
    LibrarySearchParams,
    LogClearParams,
    LogReadParams,
    LogWriteParams,
    LyricsExistsParams,
    LyricsGetParams,
    LyricsSaveParams,
    MenuCloseParams,
    MenuGetContextMenuParams,
    MenuGetMainMenuParams,
    MenuRunContextCommandParams,
    MenuRunContextCommandByIdParams,
    MenuRunMainMenuCommandParams,
    MenuShowParams,
    MenuShowNativePopupParams,
    MetadataEmbedArtworkParams,
    MetadataReadParams,
    MetadataReadBatchParams,
    MetadataReadByPathParams,
    MetadataReadRawParams,
    MetadataRemoveEmbeddedArtParams,
    MetadataRemoveFieldParams,
    MetadataRemoveTagParams,
    MetadataWriteParams,
    MetadataWriteBatchParams,
    MiscExitParams,
    MiscGetComponentPathParams,
    MiscGetFoobarPathParams,
    MiscGetProfilePathParams,
    MiscRestartParams,
    MiscShowConsoleParams,
    MiscShowLibrarySearchParams,
    MiscShowPopupMessageParams,
    MiscShowPreferencesParams,
    OutputGetDevicesParams,
    OutputGetEntriesParams,
    OutputGetSettingsParams,
    PanelGetConfigParams,
    PanelSetConfigParams,
    PlaybackGetCurrentTrackParams,
    PlaybackGetCurrentTrackIndexParams,
    PlaybackGetPlaybackOrderParams,
    PlaybackGetPlayingPlaylistParams,
    PlaybackGetPositionParams,
    PlaybackGetStateParams,
    PlaybackGetStopAfterCurrentParams,
    PlaybackGetVolumeParams,
    PlaybackMuteParams,
    PlaybackNextParams,
    PlaybackPauseParams,
    PlaybackPlayParams,
    PlaybackPlayOrPauseParams,
    PlaybackPlayPathParams,
    PlaybackPlayPathsParams,
    PlaybackPlayPauseParams,
    PlaybackPreviousParams,
    PlaybackRandomParams,
    PlaybackSetPlaybackOrderParams,
    PlaybackSetPositionParams,
    PlaybackSetStopAfterCurrentParams,
    PlaybackSetVolumeParams,
    PlaybackStopParams,
    PlaybackToggleMuteParams,
    PlaybackToggleStopAfterCurrentParams,
    PlaybackVolumeDownParams,
    PlaybackVolumeUpParams,
    PlaycountGetParams,
    PlaycountGetBatchParams,
    PlaycountGetStatsParams,
    PlaycountSetParams,
    PlaylistAddHandlesParams,
    PlaylistAddPathsParams,
    PlaylistAddPathsAsyncParams,
    PlaylistAddPathsSequentialParams,
    PlaylistClearParams,
    PlaylistConvertToAutoplaylistParams,
    PlaylistCreateParams,
    PlaylistCreateAutoplaylistParams,
    PlaylistDeselectAllParams,
    PlaylistDuplicateParams,
    PlaylistFocusTrackParams,
    PlaylistGetActiveParams,
    PlaylistGetAllParams,
    PlaylistGetAutoplaylistInfoParams,
    PlaylistGetAutoplaylistQueryParams,
    PlaylistGetAvailableColumnsParams,
    PlaylistGetCountParams,
    PlaylistGetFocusedTrackParams,
    PlaylistGetFocusTrackParams,
    PlaylistGetLockInfoParams,
    PlaylistGetPlayingParams,
    PlaylistGetSelectedTracksParams,
    PlaylistGetSelectionParams,
    PlaylistGetTrackCountParams,
    PlaylistGetTracksParams,
    PlaylistInsertTracksParams,
    PlaylistIsAutoplaylistParams,
    PlaylistIsLockedParams,
    PlaylistMoveTracksParams,
    PlaylistPlayTrackParams,
    PlaylistRedoParams,
    PlaylistRemoveParams,
    PlaylistRemoveAutoplaylistParams,
    PlaylistRemoveSelectedTracksParams,
    PlaylistRemoveTracksParams,
    PlaylistRenameParams,
    PlaylistReorderParams,
    PlaylistReorderPlaylistsParams,
    PlaylistReplaceAllAndPlayParams,
    PlaylistReverseParams,
    PlaylistSelectAllParams,
    PlaylistSetActiveParams,
    PlaylistSetFocusedTrackParams,
    PlaylistSetSelectionParams,
    PlaylistShuffleParams,
    PlaylistSortParams,
    PlaylistUndoParams,
    PortConnectParams,
    PortDisconnectParams,
    PortGetPortsParams,
    PortPostMessageParams,
    PortPostMessageToParams,
    QueueAddParams,
    QueueAddPathsParams,
    QueueClearParams,
    QueueFlushParams,
    QueueGetParams,
    QueueGetCountParams,
    QueueMoveToTopParams,
    QueueRemoveParams,
    RatingGetParams,
    RatingSetParams,
    ReplaygainClearParams,
    ReplaygainGetParams,
    ReplaygainGetModeParams,
    ReplaygainGetPreampParams,
    ReplaygainGetSettingsParams,
    ReplaygainScanParams,
    ReplaygainSetModeParams,
    ReplaygainSetPreampParams,
    SelectionGetParams,
    SelectionGetTypeParams,
    SelectionGetViewerModeParams,
    SelectionGetViewingTrackParams,
    SelectionSetParams,
    SelectionSetPlaylistTrackingParams,
    ShellExecParams,
    ShellOpenExternalParams,
    ShellOpenWithParams,
    ShellShowInExplorerParams,
    ShellSpawnParams,
    StateDeleteParams,
    StateGetParams,
    StateKeysParams,
    StateSetParams,
    SystemGetApisByNamespaceParams,
    SystemGetApiStatsParams,
    SystemGetDPIParams,
    SystemGetLocaleParams,
    SystemGetRegisteredPluginsParams,
    SystemGetThemeParams,
    SystemIsPluginRegisteredParams,
    SystemListAvailableApisParams,
    SystemSearchApisParams,
    TaskbarFlashParams,
    TaskbarSetOverlayIconParams,
    TaskbarSetProgressParams,
    TaskbarSetThumbnailButtonsParams,
    TaskbarUpdateButtonParams,
    TestEchoParams,
    TestPingParams,
    TitleformatEvalParams,
    TitleformatEvalBatchParams,
    TitleformatEvalFieldsParams,
    TitleformatEvalFieldsBatchParams,
    TitleformatGetBuiltinFieldsParams,
    TrayAppendMenuItemsParams,
    TrayClearMenuItemsParams,
    TrayCreateParams,
    TrayDestroyParams,
    TrayGetMenuItemsParams,
    TrayIsVisibleParams,
    TrayRemoveMenuItemsParams,
    TraySetCloseToTrayParams,
    TraySetContextMenuParams,
    TraySetIconParams,
    TraySetMenuItemStateParams,
    TraySetMinimizeToTrayParams,
    TraySetTooltipParams,
    TrayShowBalloonParams,
    UiHideNotificationParams,
    UiShowContextMenuParams,
    UiShowCustomMenuParams,
    UiShowNotificationParams,
    UiShowToastParams,
    WindowBlurParams,
    WindowBroadcastParams,
    WindowCancelCloseParams,
    WindowCenterParams,
    WindowClearClickThroughExcludeRegionsParams,
    WindowClearDragRegionsParams,
    WindowClearNoDragRegionsParams,
    WindowCloseParams,
    WindowCloseAllPopupsParams,
    WindowClosePopupParams,
    WindowConfirmCloseParams,
    WindowCreatePopupParams,
    WindowEnterFullscreenParams,
    WindowExitFullscreenParams,
    WindowFlashParams,
    WindowFlashTaskbarParams,
    WindowFocusParams,
    WindowGetAllWindowsParams,
    WindowGetBackdropPolicyParams,
    WindowGetBoundsParams,
    WindowGetCaptionButtonsWidthParams,
    WindowGetCornerPreferenceParams,
    WindowGetCurrentWindowIdParams,
    WindowGetDevServerConfigParams,
    WindowGetDpiScaleParams,
    WindowGetMaxSizeParams,
    WindowGetMinSizeParams,
    WindowGetModeParams,
    WindowGetPopupBehaviorParams,
    WindowGetStateParams,
    WindowGetTitleParams,
    WindowGetTitlebarHeightParams,
    WindowGetTitlebarInfoParams,
    WindowGetZoomParams,
    WindowHasSavedBoundsParams,
    WindowIsAlwaysOnTopParams,
    WindowIsClickThroughParams,
    WindowIsFullscreenParams,
    WindowIsMaximizedParams,
    WindowIsMinimizedParams,
    WindowIsResizableParams,
    WindowMaximizeParams,
    WindowMinimizeParams,
    WindowRefreshWebViewParams,
    WindowReloadParams,
    WindowResetZoomParams,
    WindowRestoreParams,
    WindowSendMessageParams,
    WindowSetAcrylicParams,
    WindowSetAlwaysOnTopParams,
    WindowSetBackdropPolicyParams,
    WindowSetBackgroundTransparencyParams,
    WindowSetBlurParams,
    WindowSetBoundsParams,
    WindowSetClickThroughParams,
    WindowSetClickThroughExcludeRegionsParams,
    WindowSetCornerPreferenceParams,
    WindowSetDarkModeParams,
    WindowSetDevServerConfigParams,
    WindowSetDragRegionsParams,
    WindowSetFramelessParams,
    WindowSetFullscreenParams,
    WindowSetMaxSizeParams,
    WindowSetMicaParams,
    WindowSetMicaEffectParams,
    WindowSetMinSizeParams,
    WindowSetNoDragRegionsParams,
    WindowSetPopupBehaviorParams,
    WindowSetPositionParams,
    WindowSetResizableParams,
    WindowSetSizeParams,
    WindowSetTitleParams,
    WindowSetTitlebarHeightParams,
    WindowSetZoomParams,
    WindowSetZoomForDpiParams,
    WindowShowSystemMenuParams,
    WindowStartDragParams,
    WindowStartResizeParams,
    WindowToggleAlwaysOnTopParams,
    WindowToggleFullscreenParams,
    WindowToggleMaximizeParams,
} from './params.js';
import type {
    ArtworkGetAvailableArtworkResponse,
    ArtworkGetAvailableTypesResponse,
    ArtworkGetBatchResponse,
    ArtworkGetByPathResponse,
    ArtworkGetByPlaylistItemResponse,
    ArtworkGetCurrentResponse,
    ArtworkGetFb2kUrlResponse,
    ArtworkGetFb2kUrlByPathResponse,
    ArtworkGetFb2kUrlByPathBatchResponse,
    ArtworkGetFolderImagesResponse,
    ArtworkGetForTrackResponse,
    ArtworkGetLyricsResponse,
    ArtworkGetMetadataResponse,
    AudioAnalyzeBPMResponse,
    AudioGenerateFullWaveformResponse,
    AudioGenerateWaveformResponse,
    AudioGetOutputInfoResponse,
    AudioGetSpectrumResponse,
    AudioGetSpectrumDebugStateResponse,
    AudioGetStreamInfoResponse,
    AudioGetWaveformResponse,
    AudioIsVisualizationAvailableResponse,
    AudioSetChannelModeResponse,
    AudioSubscribeSpectrumResponse,
    AudioSubscribeStreamResponse,
    AudioUnsubscribeSpectrumResponse,
    AudioUnsubscribeStreamResponse,
    ClipboardReadResponse,
    ClipboardWriteResponse,
    ClipboardWriteFilesResponse,
    ClipboardWriteHTMLResponse,
    ConfigExportResponse,
    ConfigGetResponse,
    ConfigGetActiveDspPresetResponse,
    ConfigGetAdvancedConfigResponse,
    ConfigGetAdvancedConfigValueResponse,
    ConfigGetAllResponse,
    ConfigGetComponentsResponse,
    ConfigGetCursorFollowPlaybackResponse,
    ConfigGetDspPresetsResponse,
    ConfigGetLibraryFilePatternsResponse,
    ConfigGetLibraryStatusResponse,
    ConfigGetOutputConfigResponse,
    ConfigGetOutputDevicesResponse,
    ConfigGetPlaybackFollowCursorResponse,
    ConfigGetPreferencesPagesResponse,
    ConfigGetPreferencesStandardGuidsResponse,
    ConfigGetReplaygainModeResponse,
    ConfigGetVersionInfoResponse,
    ConfigRemoveResponse,
    ConfigResetAdvancedConfigResponse,
    ConfigSetResponse,
    ConfigSetActiveDspPresetResponse,
    ConfigSetAdvancedConfigValueResponse,
    ConfigSetCursorFollowPlaybackResponse,
    ConfigSetOutputBufferResponse,
    ConfigSetOutputDeviceResponse,
    ConfigSetPlaybackFollowCursorResponse,
    ConfigSetReplaygainModeResponse,
    ConfigShowLibraryPreferencesResponse,
    ConsoleErrorResponse,
    ConsoleLogResponse,
    ConsoleWarnResponse,
    CursorIsHiddenResponse,
    CursorSetHiddenResponse,
    DialogConfirmResponse,
    DialogOpenFileResponse,
    DialogOpenFolderResponse,
    DialogSaveFileResponse,
    DiscoveryExecuteContextMenuByPathResponse,
    DiscoveryExecuteContextMenuCommandResponse,
    DiscoveryExecuteMainMenuCommandResponse,
    DiscoveryGetAllServicesResponse,
    DiscoveryGetComponentsResponse,
    DiscoveryGetContextMenuCommandsResponse,
    DiscoveryGetContextMenuTreeResponse,
    DiscoveryGetDspEntriesResponse,
    DiscoveryGetInputFormatsResponse,
    DiscoveryGetMainMenuCommandsResponse,
    DiscoveryGetMainMenuGroupsResponse,
    DiscoveryGetOutputDevicesResponse,
    DiscoveryGetPreferencePagesResponse,
    DiscoveryGetUIElementsResponse,
    DiscoverySearchCommandsResponse,
    DndGetDropZonesResponse,
    DndRegisterDropZoneResponse,
    DndStartDragResponse,
    DndUnregisterDropZoneResponse,
    DspAddDspResponse,
    DspApplyPresetResponse,
    DspGetAvailableResponse,
    DspGetChainResponse,
    DspGetPresetsResponse,
    DspMoveDspResponse,
    DspRemoveDspResponse,
    DspSetChainResponse,
    EventEmitResponse,
    EventEmitToResponse,
    FileCopyResponse,
    FileDeleteResponse,
    FileExistsResponse,
    FileGetInfoResponse,
    FileListResponse,
    FileMkdirResponse,
    FileMoveResponse,
    FileReadResponse,
    FileRenameResponse,
    FileWriteResponse,
    HttpAbortResponse,
    HttpDeleteResponse,
    HttpDownloadResponse,
    HttpGetResponse,
    HttpHeadResponse,
    HttpPatchResponse,
    HttpPostResponse,
    HttpPutResponse,
    JitQueueClearResponse,
    JitQueueEnqueueNextResponse,
    JitQueueGetStateResponse,
    JitQueueNotifyEmptyResponse,
    JitQueuePlayNowResponse,
    JitQueuePreloadBatchResponse,
    JitQueueSkipResponse,
    JitQueueStopResponse,
    KeyboardGetRegisteredHotkeysResponse,
    KeyboardRegisterHotkeyResponse,
    KeyboardRegisterShortcutResponse,
    KeyboardUnregisterHotkeyResponse,
    LibraryAddToPlaylistResponse,
    LibraryBrowseDirectoryResponse,
    LibraryBrowseTreeResponse,
    LibraryGetAlbumsResponse,
    LibraryGetAlbumTracksResponse,
    LibraryGetAllResponse,
    LibraryGetArtistAlbumsResponse,
    LibraryGetArtistsResponse,
    LibraryGetArtistTracksResponse,
    LibraryGetByPathResponse,
    LibraryGetCacheStatsResponse,
    LibraryGetCountResponse,
    LibraryGetFieldValuesResponse,
    LibraryGetGenresResponse,
    LibraryGetRandomTracksResponse,
    LibraryGetRecentlyAddedResponse,
    LibraryGetRootsResponse,
    LibraryGetStatsResponse,
    LibraryGetStatusResponse,
    LibraryInvalidateCacheResponse,
    LibraryIsEnabledResponse,
    LibraryQueryResponse,
    LibraryRefreshResponse,
    LibraryRescanResponse,
    LibrarySearchResponse,
    LogClearResponse,
    LogReadResponse,
    LogWriteResponse,
    LyricsExistsResponse,
    LyricsGetResponse,
    LyricsSaveResponse,
    MenuCloseResponse,
    MenuGetContextMenuResponse,
    MenuGetMainMenuResponse,
    MenuRunContextCommandResponse,
    MenuRunContextCommandByIdResponse,
    MenuRunMainMenuCommandResponse,
    MenuShowResponse,
    MenuShowNativePopupResponse,
    MetadataEmbedArtworkResponse,
    MetadataReadResponse,
    MetadataReadBatchResponse,
    MetadataReadByPathResponse,
    MetadataReadRawResponse,
    MetadataRemoveEmbeddedArtResponse,
    MetadataRemoveFieldResponse,
    MetadataRemoveTagResponse,
    MetadataWriteResponse,
    MetadataWriteBatchResponse,
    MiscExitResponse,
    MiscGetComponentPathResponse,
    MiscGetFoobarPathResponse,
    MiscGetProfilePathResponse,
    MiscRestartResponse,
    MiscShowConsoleResponse,
    MiscShowLibrarySearchResponse,
    MiscShowPopupMessageResponse,
    MiscShowPreferencesResponse,
    OutputGetDevicesResponse,
    OutputGetEntriesResponse,
    OutputGetSettingsResponse,
    PanelGetConfigResponse,
    PanelSetConfigResponse,
    PlaybackGetCurrentTrackResponse,
    PlaybackGetCurrentTrackIndexResponse,
    PlaybackGetPlaybackOrderResponse,
    PlaybackGetPlayingPlaylistResponse,
    PlaybackGetPositionResponse,
    PlaybackGetStateResponse,
    PlaybackGetStopAfterCurrentResponse,
    PlaybackGetVolumeResponse,
    PlaybackMuteResponse,
    PlaybackNextResponse,
    PlaybackPauseResponse,
    PlaybackPlayResponse,
    PlaybackPlayOrPauseResponse,
    PlaybackPlayPathResponse,
    PlaybackPlayPathsResponse,
    PlaybackPlayPauseResponse,
    PlaybackPreviousResponse,
    PlaybackRandomResponse,
    PlaybackSetPlaybackOrderResponse,
    PlaybackSetPositionResponse,
    PlaybackSetStopAfterCurrentResponse,
    PlaybackSetVolumeResponse,
    PlaybackStopResponse,
    PlaybackToggleMuteResponse,
    PlaybackToggleStopAfterCurrentResponse,
    PlaybackVolumeDownResponse,
    PlaybackVolumeUpResponse,
    PlaycountGetResponse,
    PlaycountGetBatchResponse,
    PlaycountGetStatsResponse,
    PlaycountSetResponse,
    PlaylistAddHandlesResponse,
    PlaylistAddPathsResponse,
    PlaylistAddPathsAsyncResponse,
    PlaylistAddPathsSequentialResponse,
    PlaylistClearResponse,
    PlaylistConvertToAutoplaylistResponse,
    PlaylistCreateResponse,
    PlaylistCreateAutoplaylistResponse,
    PlaylistDeselectAllResponse,
    PlaylistDuplicateResponse,
    PlaylistFocusTrackResponse,
    PlaylistGetActiveResponse,
    PlaylistGetAllResponse,
    PlaylistGetAutoplaylistInfoResponse,
    PlaylistGetAutoplaylistQueryResponse,
    PlaylistGetAvailableColumnsResponse,
    PlaylistGetCountResponse,
    PlaylistGetFocusedTrackResponse,
    PlaylistGetFocusTrackResponse,
    PlaylistGetLockInfoResponse,
    PlaylistGetPlayingResponse,
    PlaylistGetSelectedTracksResponse,
    PlaylistGetSelectionResponse,
    PlaylistGetTrackCountResponse,
    PlaylistGetTracksResponse,
    PlaylistInsertTracksResponse,
    PlaylistIsAutoplaylistResponse,
    PlaylistIsLockedResponse,
    PlaylistMoveTracksResponse,
    PlaylistPlayTrackResponse,
    PlaylistRedoResponse,
    PlaylistRemoveResponse,
    PlaylistRemoveAutoplaylistResponse,
    PlaylistRemoveSelectedTracksResponse,
    PlaylistRemoveTracksResponse,
    PlaylistRenameResponse,
    PlaylistReorderResponse,
    PlaylistReorderPlaylistsResponse,
    PlaylistReplaceAllAndPlayResponse,
    PlaylistReverseResponse,
    PlaylistSelectAllResponse,
    PlaylistSetActiveResponse,
    PlaylistSetFocusedTrackResponse,
    PlaylistSetSelectionResponse,
    PlaylistShuffleResponse,
    PlaylistSortResponse,
    PlaylistUndoResponse,
    PortConnectResponse,
    PortDisconnectResponse,
    PortGetPortsResponse,
    PortPostMessageResponse,
    PortPostMessageToResponse,
    QueueAddResponse,
    QueueAddPathsResponse,
    QueueClearResponse,
    QueueFlushResponse,
    QueueGetResponse,
    QueueGetCountResponse,
    QueueMoveToTopResponse,
    QueueRemoveResponse,
    RatingGetResponse,
    RatingSetResponse,
    ReplaygainClearResponse,
    ReplaygainGetResponse,
    ReplaygainGetModeResponse,
    ReplaygainGetPreampResponse,
    ReplaygainGetSettingsResponse,
    ReplaygainScanResponse,
    ReplaygainSetModeResponse,
    ReplaygainSetPreampResponse,
    SelectionGetResponse,
    SelectionGetTypeResponse,
    SelectionGetViewerModeResponse,
    SelectionGetViewingTrackResponse,
    SelectionSetResponse,
    SelectionSetPlaylistTrackingResponse,
    ShellExecResponse,
    ShellOpenExternalResponse,
    ShellOpenWithResponse,
    ShellShowInExplorerResponse,
    ShellSpawnResponse,
    StateDeleteResponse,
    StateGetResponse,
    StateKeysResponse,
    StateSetResponse,
    SystemGetApisByNamespaceResponse,
    SystemGetApiStatsResponse,
    SystemGetDPIResponse,
    SystemGetLocaleResponse,
    SystemGetRegisteredPluginsResponse,
    SystemGetThemeResponse,
    SystemIsPluginRegisteredResponse,
    SystemListAvailableApisResponse,
    SystemSearchApisResponse,
    TaskbarFlashResponse,
    TaskbarSetOverlayIconResponse,
    TaskbarSetProgressResponse,
    TaskbarSetThumbnailButtonsResponse,
    TaskbarUpdateButtonResponse,
    TestEchoResponse,
    TestPingResponse,
    TitleformatEvalResponse,
    TitleformatEvalBatchResponse,
    TitleformatEvalFieldsResponse,
    TitleformatEvalFieldsBatchResponse,
    TitleformatGetBuiltinFieldsResponse,
    TrayAppendMenuItemsResponse,
    TrayClearMenuItemsResponse,
    TrayCreateResponse,
    TrayDestroyResponse,
    TrayGetMenuItemsResponse,
    TrayIsVisibleResponse,
    TrayRemoveMenuItemsResponse,
    TraySetCloseToTrayResponse,
    TraySetContextMenuResponse,
    TraySetIconResponse,
    TraySetMenuItemStateResponse,
    TraySetMinimizeToTrayResponse,
    TraySetTooltipResponse,
    TrayShowBalloonResponse,
    UiHideNotificationResponse,
    UiShowContextMenuResponse,
    UiShowCustomMenuResponse,
    UiShowNotificationResponse,
    UiShowToastResponse,
    WindowBlurResponse,
    WindowBroadcastResponse,
    WindowCancelCloseResponse,
    WindowCenterResponse,
    WindowClearClickThroughExcludeRegionsResponse,
    WindowClearDragRegionsResponse,
    WindowClearNoDragRegionsResponse,
    WindowCloseResponse,
    WindowCloseAllPopupsResponse,
    WindowClosePopupResponse,
    WindowConfirmCloseResponse,
    WindowCreatePopupResponse,
    WindowEnterFullscreenResponse,
    WindowExitFullscreenResponse,
    WindowFlashResponse,
    WindowFlashTaskbarResponse,
    WindowFocusResponse,
    WindowGetAllWindowsResponse,
    WindowGetBackdropPolicyResponse,
    WindowGetBoundsResponse,
    WindowGetCaptionButtonsWidthResponse,
    WindowGetCornerPreferenceResponse,
    WindowGetCurrentWindowIdResponse,
    WindowGetDevServerConfigResponse,
    WindowGetDpiScaleResponse,
    WindowGetMaxSizeResponse,
    WindowGetMinSizeResponse,
    WindowGetModeResponse,
    WindowGetPopupBehaviorResponse,
    WindowGetStateResponse,
    WindowGetTitleResponse,
    WindowGetTitlebarHeightResponse,
    WindowGetTitlebarInfoResponse,
    WindowGetZoomResponse,
    WindowHasSavedBoundsResponse,
    WindowIsAlwaysOnTopResponse,
    WindowIsClickThroughResponse,
    WindowIsFullscreenResponse,
    WindowIsMaximizedResponse,
    WindowIsMinimizedResponse,
    WindowIsResizableResponse,
    WindowMaximizeResponse,
    WindowMinimizeResponse,
    WindowRefreshWebViewResponse,
    WindowReloadResponse,
    WindowResetZoomResponse,
    WindowRestoreResponse,
    WindowSendMessageResponse,
    WindowSetAcrylicResponse,
    WindowSetAlwaysOnTopResponse,
    WindowSetBackdropPolicyResponse,
    WindowSetBackgroundTransparencyResponse,
    WindowSetBlurResponse,
    WindowSetBoundsResponse,
    WindowSetClickThroughResponse,
    WindowSetClickThroughExcludeRegionsResponse,
    WindowSetCornerPreferenceResponse,
    WindowSetDarkModeResponse,
    WindowSetDevServerConfigResponse,
    WindowSetDragRegionsResponse,
    WindowSetFramelessResponse,
    WindowSetFullscreenResponse,
    WindowSetMaxSizeResponse,
    WindowSetMicaResponse,
    WindowSetMicaEffectResponse,
    WindowSetMinSizeResponse,
    WindowSetNoDragRegionsResponse,
    WindowSetPopupBehaviorResponse,
    WindowSetPositionResponse,
    WindowSetResizableResponse,
    WindowSetSizeResponse,
    WindowSetTitleResponse,
    WindowSetTitlebarHeightResponse,
    WindowSetZoomResponse,
    WindowSetZoomForDpiResponse,
    WindowShowSystemMenuResponse,
    WindowStartDragResponse,
    WindowStartResizeResponse,
    WindowToggleAlwaysOnTopResponse,
    WindowToggleFullscreenResponse,
    WindowToggleMaximizeResponse,
} from './responses.js';
export interface ApiMethodMap {
    "artwork.getAvailableArtwork": [ArtworkGetAvailableArtworkParams, ArtworkGetAvailableArtworkResponse];
    "artwork.getAvailableTypes": [ArtworkGetAvailableTypesParams, ArtworkGetAvailableTypesResponse];
    "artwork.getBatch": [ArtworkGetBatchParams, ArtworkGetBatchResponse];
    "artwork.getByPath": [ArtworkGetByPathParams, ArtworkGetByPathResponse];
    "artwork.getByPlaylistItem": [ArtworkGetByPlaylistItemParams, ArtworkGetByPlaylistItemResponse];
    "artwork.getCurrent": [ArtworkGetCurrentParams, ArtworkGetCurrentResponse];
    "artwork.getFb2kUrl": [ArtworkGetFb2kUrlParams, ArtworkGetFb2kUrlResponse];
    "artwork.getFb2kUrlByPath": [ArtworkGetFb2kUrlByPathParams, ArtworkGetFb2kUrlByPathResponse];
    "artwork.getFb2kUrlByPathBatch": [ArtworkGetFb2kUrlByPathBatchParams, ArtworkGetFb2kUrlByPathBatchResponse];
    "artwork.getFolderImages": [ArtworkGetFolderImagesParams, ArtworkGetFolderImagesResponse];
    "artwork.getForTrack": [ArtworkGetForTrackParams, ArtworkGetForTrackResponse];
    "artwork.getLyrics": [ArtworkGetLyricsParams, ArtworkGetLyricsResponse];
    "artwork.getMetadata": [ArtworkGetMetadataParams, ArtworkGetMetadataResponse];
    "audio.analyzeBPM": [AudioAnalyzeBPMParams, AudioAnalyzeBPMResponse];
    "audio.generateFullWaveform": [AudioGenerateFullWaveformParams, AudioGenerateFullWaveformResponse];
    "audio.generateWaveform": [AudioGenerateWaveformParams, AudioGenerateWaveformResponse];
    "audio.getOutputInfo": [AudioGetOutputInfoParams, AudioGetOutputInfoResponse];
    "audio.getSpectrum": [AudioGetSpectrumParams, AudioGetSpectrumResponse];
    "audio.getSpectrumDebugState": [AudioGetSpectrumDebugStateParams, AudioGetSpectrumDebugStateResponse];
    "audio.getStreamInfo": [AudioGetStreamInfoParams, AudioGetStreamInfoResponse];
    "audio.getWaveform": [AudioGetWaveformParams, AudioGetWaveformResponse];
    "audio.isVisualizationAvailable": [AudioIsVisualizationAvailableParams, AudioIsVisualizationAvailableResponse];
    "audio.setChannelMode": [AudioSetChannelModeParams, AudioSetChannelModeResponse];
    "audio.subscribeSpectrum": [AudioSubscribeSpectrumParams, AudioSubscribeSpectrumResponse];
    "audio.subscribeStream": [AudioSubscribeStreamParams, AudioSubscribeStreamResponse];
    "audio.unsubscribeSpectrum": [AudioUnsubscribeSpectrumParams, AudioUnsubscribeSpectrumResponse];
    "audio.unsubscribeStream": [AudioUnsubscribeStreamParams, AudioUnsubscribeStreamResponse];
    "clipboard.read": [ClipboardReadParams, ClipboardReadResponse];
    "clipboard.write": [ClipboardWriteParams, ClipboardWriteResponse];
    "clipboard.writeFiles": [ClipboardWriteFilesParams, ClipboardWriteFilesResponse];
    "clipboard.writeHTML": [ClipboardWriteHTMLParams, ClipboardWriteHTMLResponse];
    "config.export": [ConfigExportParams, ConfigExportResponse];
    "config.get": [ConfigGetParams, ConfigGetResponse];
    "config.getActiveDspPreset": [ConfigGetActiveDspPresetParams, ConfigGetActiveDspPresetResponse];
    "config.getAdvancedConfig": [ConfigGetAdvancedConfigParams, ConfigGetAdvancedConfigResponse];
    "config.getAdvancedConfigValue": [ConfigGetAdvancedConfigValueParams, ConfigGetAdvancedConfigValueResponse];
    "config.getAll": [ConfigGetAllParams, ConfigGetAllResponse];
    "config.getComponents": [ConfigGetComponentsParams, ConfigGetComponentsResponse];
    "config.getCursorFollowPlayback": [ConfigGetCursorFollowPlaybackParams, ConfigGetCursorFollowPlaybackResponse];
    "config.getDspPresets": [ConfigGetDspPresetsParams, ConfigGetDspPresetsResponse];
    "config.getLibraryFilePatterns": [ConfigGetLibraryFilePatternsParams, ConfigGetLibraryFilePatternsResponse];
    "config.getLibraryStatus": [ConfigGetLibraryStatusParams, ConfigGetLibraryStatusResponse];
    "config.getOutputConfig": [ConfigGetOutputConfigParams, ConfigGetOutputConfigResponse];
    "config.getOutputDevices": [ConfigGetOutputDevicesParams, ConfigGetOutputDevicesResponse];
    "config.getPlaybackFollowCursor": [ConfigGetPlaybackFollowCursorParams, ConfigGetPlaybackFollowCursorResponse];
    "config.getPreferencesPages": [ConfigGetPreferencesPagesParams, ConfigGetPreferencesPagesResponse];
    "config.getPreferencesStandardGuids": [ConfigGetPreferencesStandardGuidsParams, ConfigGetPreferencesStandardGuidsResponse];
    "config.getReplaygainMode": [ConfigGetReplaygainModeParams, ConfigGetReplaygainModeResponse];
    "config.getVersionInfo": [ConfigGetVersionInfoParams, ConfigGetVersionInfoResponse];
    "config.remove": [ConfigRemoveParams, ConfigRemoveResponse];
    "config.resetAdvancedConfig": [ConfigResetAdvancedConfigParams, ConfigResetAdvancedConfigResponse];
    "config.set": [ConfigSetParams, ConfigSetResponse];
    "config.setActiveDspPreset": [ConfigSetActiveDspPresetParams, ConfigSetActiveDspPresetResponse];
    "config.setAdvancedConfigValue": [ConfigSetAdvancedConfigValueParams, ConfigSetAdvancedConfigValueResponse];
    "config.setCursorFollowPlayback": [ConfigSetCursorFollowPlaybackParams, ConfigSetCursorFollowPlaybackResponse];
    "config.setOutputBuffer": [ConfigSetOutputBufferParams, ConfigSetOutputBufferResponse];
    "config.setOutputDevice": [ConfigSetOutputDeviceParams, ConfigSetOutputDeviceResponse];
    "config.setPlaybackFollowCursor": [ConfigSetPlaybackFollowCursorParams, ConfigSetPlaybackFollowCursorResponse];
    "config.setReplaygainMode": [ConfigSetReplaygainModeParams, ConfigSetReplaygainModeResponse];
    "config.showLibraryPreferences": [ConfigShowLibraryPreferencesParams, ConfigShowLibraryPreferencesResponse];
    "console.error": [ConsoleErrorParams, ConsoleErrorResponse];
    "console.log": [ConsoleLogParams, ConsoleLogResponse];
    "console.warn": [ConsoleWarnParams, ConsoleWarnResponse];
    "cursor.isHidden": [CursorIsHiddenParams, CursorIsHiddenResponse];
    "cursor.setHidden": [CursorSetHiddenParams, CursorSetHiddenResponse];
    "dialog.confirm": [DialogConfirmParams, DialogConfirmResponse];
    "dialog.openFile": [DialogOpenFileParams, DialogOpenFileResponse];
    "dialog.openFolder": [DialogOpenFolderParams, DialogOpenFolderResponse];
    "dialog.saveFile": [DialogSaveFileParams, DialogSaveFileResponse];
    "discovery.executeContextMenuByPath": [DiscoveryExecuteContextMenuByPathParams, DiscoveryExecuteContextMenuByPathResponse];
    "discovery.executeContextMenuCommand": [DiscoveryExecuteContextMenuCommandParams, DiscoveryExecuteContextMenuCommandResponse];
    "discovery.executeMainMenuCommand": [DiscoveryExecuteMainMenuCommandParams, DiscoveryExecuteMainMenuCommandResponse];
    "discovery.getAllServices": [DiscoveryGetAllServicesParams, DiscoveryGetAllServicesResponse];
    "discovery.getComponents": [DiscoveryGetComponentsParams, DiscoveryGetComponentsResponse];
    "discovery.getContextMenuCommands": [DiscoveryGetContextMenuCommandsParams, DiscoveryGetContextMenuCommandsResponse];
    "discovery.getContextMenuTree": [DiscoveryGetContextMenuTreeParams, DiscoveryGetContextMenuTreeResponse];
    "discovery.getDspEntries": [DiscoveryGetDspEntriesParams, DiscoveryGetDspEntriesResponse];
    "discovery.getInputFormats": [DiscoveryGetInputFormatsParams, DiscoveryGetInputFormatsResponse];
    "discovery.getMainMenuCommands": [DiscoveryGetMainMenuCommandsParams, DiscoveryGetMainMenuCommandsResponse];
    "discovery.getMainMenuGroups": [DiscoveryGetMainMenuGroupsParams, DiscoveryGetMainMenuGroupsResponse];
    "discovery.getOutputDevices": [DiscoveryGetOutputDevicesParams, DiscoveryGetOutputDevicesResponse];
    "discovery.getPreferencePages": [DiscoveryGetPreferencePagesParams, DiscoveryGetPreferencePagesResponse];
    "discovery.getUIElements": [DiscoveryGetUIElementsParams, DiscoveryGetUIElementsResponse];
    "discovery.searchCommands": [DiscoverySearchCommandsParams, DiscoverySearchCommandsResponse];
    "dnd.getDropZones": [DndGetDropZonesParams, DndGetDropZonesResponse];
    "dnd.registerDropZone": [DndRegisterDropZoneParams, DndRegisterDropZoneResponse];
    "dnd.startDrag": [DndStartDragParams, DndStartDragResponse];
    "dnd.unregisterDropZone": [DndUnregisterDropZoneParams, DndUnregisterDropZoneResponse];
    "dsp.addDsp": [DspAddDspParams, DspAddDspResponse];
    "dsp.applyPreset": [DspApplyPresetParams, DspApplyPresetResponse];
    "dsp.getAvailable": [DspGetAvailableParams, DspGetAvailableResponse];
    "dsp.getChain": [DspGetChainParams, DspGetChainResponse];
    "dsp.getPresets": [DspGetPresetsParams, DspGetPresetsResponse];
    "dsp.moveDsp": [DspMoveDspParams, DspMoveDspResponse];
    "dsp.removeDsp": [DspRemoveDspParams, DspRemoveDspResponse];
    "dsp.setChain": [DspSetChainParams, DspSetChainResponse];
    "event.emit": [EventEmitParams, EventEmitResponse];
    "event.emitTo": [EventEmitToParams, EventEmitToResponse];
    "file.copy": [FileCopyParams, FileCopyResponse];
    "file.delete": [FileDeleteParams, FileDeleteResponse];
    "file.exists": [FileExistsParams, FileExistsResponse];
    "file.getInfo": [FileGetInfoParams, FileGetInfoResponse];
    "file.list": [FileListParams, FileListResponse];
    "file.mkdir": [FileMkdirParams, FileMkdirResponse];
    "file.move": [FileMoveParams, FileMoveResponse];
    "file.read": [FileReadParams, FileReadResponse];
    "file.rename": [FileRenameParams, FileRenameResponse];
    "file.write": [FileWriteParams, FileWriteResponse];
    "http.abort": [HttpAbortParams, HttpAbortResponse];
    "http.delete": [HttpDeleteParams, HttpDeleteResponse];
    "http.download": [HttpDownloadParams, HttpDownloadResponse];
    "http.get": [HttpGetParams, HttpGetResponse];
    "http.head": [HttpHeadParams, HttpHeadResponse];
    "http.patch": [HttpPatchParams, HttpPatchResponse];
    "http.post": [HttpPostParams, HttpPostResponse];
    "http.put": [HttpPutParams, HttpPutResponse];
    "jitQueue.clear": [JitQueueClearParams, JitQueueClearResponse];
    "jitQueue.enqueueNext": [JitQueueEnqueueNextParams, JitQueueEnqueueNextResponse];
    "jitQueue.getState": [JitQueueGetStateParams, JitQueueGetStateResponse];
    "jitQueue.notifyEmpty": [JitQueueNotifyEmptyParams, JitQueueNotifyEmptyResponse];
    "jitQueue.playNow": [JitQueuePlayNowParams, JitQueuePlayNowResponse];
    "jitQueue.preloadBatch": [JitQueuePreloadBatchParams, JitQueuePreloadBatchResponse];
    "jitQueue.skip": [JitQueueSkipParams, JitQueueSkipResponse];
    "jitQueue.stop": [JitQueueStopParams, JitQueueStopResponse];
    "keyboard.getRegisteredHotkeys": [KeyboardGetRegisteredHotkeysParams, KeyboardGetRegisteredHotkeysResponse];
    "keyboard.registerHotkey": [KeyboardRegisterHotkeyParams, KeyboardRegisterHotkeyResponse];
    "keyboard.registerShortcut": [KeyboardRegisterShortcutParams, KeyboardRegisterShortcutResponse];
    "keyboard.unregisterHotkey": [KeyboardUnregisterHotkeyParams, KeyboardUnregisterHotkeyResponse];
    "library.addToPlaylist": [LibraryAddToPlaylistParams, LibraryAddToPlaylistResponse];
    "library.browseDirectory": [LibraryBrowseDirectoryParams, LibraryBrowseDirectoryResponse];
    "library.browseTree": [LibraryBrowseTreeParams, LibraryBrowseTreeResponse];
    "library.getAlbums": [LibraryGetAlbumsParams, LibraryGetAlbumsResponse];
    "library.getAlbumTracks": [LibraryGetAlbumTracksParams, LibraryGetAlbumTracksResponse];
    "library.getAll": [LibraryGetAllParams, LibraryGetAllResponse];
    "library.getArtistAlbums": [LibraryGetArtistAlbumsParams, LibraryGetArtistAlbumsResponse];
    "library.getArtists": [LibraryGetArtistsParams, LibraryGetArtistsResponse];
    "library.getArtistTracks": [LibraryGetArtistTracksParams, LibraryGetArtistTracksResponse];
    "library.getByPath": [LibraryGetByPathParams, LibraryGetByPathResponse];
    "library.getCacheStats": [LibraryGetCacheStatsParams, LibraryGetCacheStatsResponse];
    "library.getCount": [LibraryGetCountParams, LibraryGetCountResponse];
    "library.getFieldValues": [LibraryGetFieldValuesParams, LibraryGetFieldValuesResponse];
    "library.getGenres": [LibraryGetGenresParams, LibraryGetGenresResponse];
    "library.getRandomTracks": [LibraryGetRandomTracksParams, LibraryGetRandomTracksResponse];
    "library.getRecentlyAdded": [LibraryGetRecentlyAddedParams, LibraryGetRecentlyAddedResponse];
    "library.getRoots": [LibraryGetRootsParams, LibraryGetRootsResponse];
    "library.getStats": [LibraryGetStatsParams, LibraryGetStatsResponse];
    "library.getStatus": [LibraryGetStatusParams, LibraryGetStatusResponse];
    "library.invalidateCache": [LibraryInvalidateCacheParams, LibraryInvalidateCacheResponse];
    "library.isEnabled": [LibraryIsEnabledParams, LibraryIsEnabledResponse];
    "library.query": [LibraryQueryParams, LibraryQueryResponse];
    "library.refresh": [LibraryRefreshParams, LibraryRefreshResponse];
    "library.rescan": [LibraryRescanParams, LibraryRescanResponse];
    "library.search": [LibrarySearchParams, LibrarySearchResponse];
    "log.clear": [LogClearParams, LogClearResponse];
    "log.read": [LogReadParams, LogReadResponse];
    "log.write": [LogWriteParams, LogWriteResponse];
    "lyrics.exists": [LyricsExistsParams, LyricsExistsResponse];
    "lyrics.get": [LyricsGetParams, LyricsGetResponse];
    "lyrics.save": [LyricsSaveParams, LyricsSaveResponse];
    "menu.close": [MenuCloseParams, MenuCloseResponse];
    "menu.getContextMenu": [MenuGetContextMenuParams, MenuGetContextMenuResponse];
    "menu.getMainMenu": [MenuGetMainMenuParams, MenuGetMainMenuResponse];
    "menu.runContextCommand": [MenuRunContextCommandParams, MenuRunContextCommandResponse];
    "menu.runContextCommandById": [MenuRunContextCommandByIdParams, MenuRunContextCommandByIdResponse];
    "menu.runMainMenuCommand": [MenuRunMainMenuCommandParams, MenuRunMainMenuCommandResponse];
    "menu.show": [MenuShowParams, MenuShowResponse];
    "menu.showNativePopup": [MenuShowNativePopupParams, MenuShowNativePopupResponse];
    "metadata.embedArtwork": [MetadataEmbedArtworkParams, MetadataEmbedArtworkResponse];
    "metadata.read": [MetadataReadParams, MetadataReadResponse];
    "metadata.readBatch": [MetadataReadBatchParams, MetadataReadBatchResponse];
    "metadata.readByPath": [MetadataReadByPathParams, MetadataReadByPathResponse];
    "metadata.readRaw": [MetadataReadRawParams, MetadataReadRawResponse];
    "metadata.removeEmbeddedArt": [MetadataRemoveEmbeddedArtParams, MetadataRemoveEmbeddedArtResponse];
    "metadata.removeField": [MetadataRemoveFieldParams, MetadataRemoveFieldResponse];
    "metadata.removeTag": [MetadataRemoveTagParams, MetadataRemoveTagResponse];
    "metadata.write": [MetadataWriteParams, MetadataWriteResponse];
    "metadata.writeBatch": [MetadataWriteBatchParams, MetadataWriteBatchResponse];
    "misc.exit": [MiscExitParams, MiscExitResponse];
    "misc.getComponentPath": [MiscGetComponentPathParams, MiscGetComponentPathResponse];
    "misc.getFoobarPath": [MiscGetFoobarPathParams, MiscGetFoobarPathResponse];
    "misc.getProfilePath": [MiscGetProfilePathParams, MiscGetProfilePathResponse];
    "misc.restart": [MiscRestartParams, MiscRestartResponse];
    "misc.showConsole": [MiscShowConsoleParams, MiscShowConsoleResponse];
    "misc.showLibrarySearch": [MiscShowLibrarySearchParams, MiscShowLibrarySearchResponse];
    "misc.showPopupMessage": [MiscShowPopupMessageParams, MiscShowPopupMessageResponse];
    "misc.showPreferences": [MiscShowPreferencesParams, MiscShowPreferencesResponse];
    "output.getDevices": [OutputGetDevicesParams, OutputGetDevicesResponse];
    "output.getEntries": [OutputGetEntriesParams, OutputGetEntriesResponse];
    "output.getSettings": [OutputGetSettingsParams, OutputGetSettingsResponse];
    "panel.getConfig": [PanelGetConfigParams, PanelGetConfigResponse];
    "panel.setConfig": [PanelSetConfigParams, PanelSetConfigResponse];
    "playback.getCurrentTrack": [PlaybackGetCurrentTrackParams, PlaybackGetCurrentTrackResponse];
    "playback.getCurrentTrackIndex": [PlaybackGetCurrentTrackIndexParams, PlaybackGetCurrentTrackIndexResponse];
    "playback.getPlaybackOrder": [PlaybackGetPlaybackOrderParams, PlaybackGetPlaybackOrderResponse];
    "playback.getPlayingPlaylist": [PlaybackGetPlayingPlaylistParams, PlaybackGetPlayingPlaylistResponse];
    "playback.getPosition": [PlaybackGetPositionParams, PlaybackGetPositionResponse];
    "playback.getState": [PlaybackGetStateParams, PlaybackGetStateResponse];
    "playback.getStopAfterCurrent": [PlaybackGetStopAfterCurrentParams, PlaybackGetStopAfterCurrentResponse];
    "playback.getVolume": [PlaybackGetVolumeParams, PlaybackGetVolumeResponse];
    "playback.mute": [PlaybackMuteParams, PlaybackMuteResponse];
    "playback.next": [PlaybackNextParams, PlaybackNextResponse];
    "playback.pause": [PlaybackPauseParams, PlaybackPauseResponse];
    "playback.play": [PlaybackPlayParams, PlaybackPlayResponse];
    "playback.playOrPause": [PlaybackPlayOrPauseParams, PlaybackPlayOrPauseResponse];
    "playback.playPath": [PlaybackPlayPathParams, PlaybackPlayPathResponse];
    "playback.playPaths": [PlaybackPlayPathsParams, PlaybackPlayPathsResponse];
    "playback.playPause": [PlaybackPlayPauseParams, PlaybackPlayPauseResponse];
    "playback.previous": [PlaybackPreviousParams, PlaybackPreviousResponse];
    "playback.random": [PlaybackRandomParams, PlaybackRandomResponse];
    "playback.setPlaybackOrder": [PlaybackSetPlaybackOrderParams, PlaybackSetPlaybackOrderResponse];
    "playback.setPosition": [PlaybackSetPositionParams, PlaybackSetPositionResponse];
    "playback.setStopAfterCurrent": [PlaybackSetStopAfterCurrentParams, PlaybackSetStopAfterCurrentResponse];
    "playback.setVolume": [PlaybackSetVolumeParams, PlaybackSetVolumeResponse];
    "playback.stop": [PlaybackStopParams, PlaybackStopResponse];
    "playback.toggleMute": [PlaybackToggleMuteParams, PlaybackToggleMuteResponse];
    "playback.toggleStopAfterCurrent": [PlaybackToggleStopAfterCurrentParams, PlaybackToggleStopAfterCurrentResponse];
    "playback.volumeDown": [PlaybackVolumeDownParams, PlaybackVolumeDownResponse];
    "playback.volumeUp": [PlaybackVolumeUpParams, PlaybackVolumeUpResponse];
    "playcount.get": [PlaycountGetParams, PlaycountGetResponse];
    "playcount.getBatch": [PlaycountGetBatchParams, PlaycountGetBatchResponse];
    "playcount.getStats": [PlaycountGetStatsParams, PlaycountGetStatsResponse];
    "playcount.set": [PlaycountSetParams, PlaycountSetResponse];
    "playlist.addHandles": [PlaylistAddHandlesParams, PlaylistAddHandlesResponse];
    "playlist.addPaths": [PlaylistAddPathsParams, PlaylistAddPathsResponse];
    "playlist.addPathsAsync": [PlaylistAddPathsAsyncParams, PlaylistAddPathsAsyncResponse];
    "playlist.addPathsSequential": [PlaylistAddPathsSequentialParams, PlaylistAddPathsSequentialResponse];
    "playlist.clear": [PlaylistClearParams, PlaylistClearResponse];
    "playlist.convertToAutoplaylist": [PlaylistConvertToAutoplaylistParams, PlaylistConvertToAutoplaylistResponse];
    "playlist.create": [PlaylistCreateParams, PlaylistCreateResponse];
    "playlist.createAutoplaylist": [PlaylistCreateAutoplaylistParams, PlaylistCreateAutoplaylistResponse];
    "playlist.deselectAll": [PlaylistDeselectAllParams, PlaylistDeselectAllResponse];
    "playlist.duplicate": [PlaylistDuplicateParams, PlaylistDuplicateResponse];
    "playlist.focusTrack": [PlaylistFocusTrackParams, PlaylistFocusTrackResponse];
    "playlist.getActive": [PlaylistGetActiveParams, PlaylistGetActiveResponse];
    "playlist.getAll": [PlaylistGetAllParams, PlaylistGetAllResponse];
    "playlist.getAutoplaylistInfo": [PlaylistGetAutoplaylistInfoParams, PlaylistGetAutoplaylistInfoResponse];
    "playlist.getAutoplaylistQuery": [PlaylistGetAutoplaylistQueryParams, PlaylistGetAutoplaylistQueryResponse];
    "playlist.getAvailableColumns": [PlaylistGetAvailableColumnsParams, PlaylistGetAvailableColumnsResponse];
    "playlist.getCount": [PlaylistGetCountParams, PlaylistGetCountResponse];
    "playlist.getFocusedTrack": [PlaylistGetFocusedTrackParams, PlaylistGetFocusedTrackResponse];
    "playlist.getFocusTrack": [PlaylistGetFocusTrackParams, PlaylistGetFocusTrackResponse];
    "playlist.getLockInfo": [PlaylistGetLockInfoParams, PlaylistGetLockInfoResponse];
    "playlist.getPlaying": [PlaylistGetPlayingParams, PlaylistGetPlayingResponse];
    "playlist.getSelectedTracks": [PlaylistGetSelectedTracksParams, PlaylistGetSelectedTracksResponse];
    "playlist.getSelection": [PlaylistGetSelectionParams, PlaylistGetSelectionResponse];
    "playlist.getTrackCount": [PlaylistGetTrackCountParams, PlaylistGetTrackCountResponse];
    "playlist.getTracks": [PlaylistGetTracksParams, PlaylistGetTracksResponse];
    "playlist.insertTracks": [PlaylistInsertTracksParams, PlaylistInsertTracksResponse];
    "playlist.isAutoplaylist": [PlaylistIsAutoplaylistParams, PlaylistIsAutoplaylistResponse];
    "playlist.isLocked": [PlaylistIsLockedParams, PlaylistIsLockedResponse];
    "playlist.moveTracks": [PlaylistMoveTracksParams, PlaylistMoveTracksResponse];
    "playlist.playTrack": [PlaylistPlayTrackParams, PlaylistPlayTrackResponse];
    "playlist.redo": [PlaylistRedoParams, PlaylistRedoResponse];
    "playlist.remove": [PlaylistRemoveParams, PlaylistRemoveResponse];
    "playlist.removeAutoplaylist": [PlaylistRemoveAutoplaylistParams, PlaylistRemoveAutoplaylistResponse];
    "playlist.removeSelectedTracks": [PlaylistRemoveSelectedTracksParams, PlaylistRemoveSelectedTracksResponse];
    "playlist.removeTracks": [PlaylistRemoveTracksParams, PlaylistRemoveTracksResponse];
    "playlist.rename": [PlaylistRenameParams, PlaylistRenameResponse];
    "playlist.reorder": [PlaylistReorderParams, PlaylistReorderResponse];
    "playlist.reorderPlaylists": [PlaylistReorderPlaylistsParams, PlaylistReorderPlaylistsResponse];
    "playlist.replaceAllAndPlay": [PlaylistReplaceAllAndPlayParams, PlaylistReplaceAllAndPlayResponse];
    "playlist.reverse": [PlaylistReverseParams, PlaylistReverseResponse];
    "playlist.selectAll": [PlaylistSelectAllParams, PlaylistSelectAllResponse];
    "playlist.setActive": [PlaylistSetActiveParams, PlaylistSetActiveResponse];
    "playlist.setFocusedTrack": [PlaylistSetFocusedTrackParams, PlaylistSetFocusedTrackResponse];
    "playlist.setSelection": [PlaylistSetSelectionParams, PlaylistSetSelectionResponse];
    "playlist.shuffle": [PlaylistShuffleParams, PlaylistShuffleResponse];
    "playlist.sort": [PlaylistSortParams, PlaylistSortResponse];
    "playlist.undo": [PlaylistUndoParams, PlaylistUndoResponse];
    "port.connect": [PortConnectParams, PortConnectResponse];
    "port.disconnect": [PortDisconnectParams, PortDisconnectResponse];
    "port.getPorts": [PortGetPortsParams, PortGetPortsResponse];
    "port.postMessage": [PortPostMessageParams, PortPostMessageResponse];
    "port.postMessageTo": [PortPostMessageToParams, PortPostMessageToResponse];
    "queue.add": [QueueAddParams, QueueAddResponse];
    "queue.addPaths": [QueueAddPathsParams, QueueAddPathsResponse];
    "queue.clear": [QueueClearParams, QueueClearResponse];
    "queue.flush": [QueueFlushParams, QueueFlushResponse];
    "queue.get": [QueueGetParams, QueueGetResponse];
    "queue.getCount": [QueueGetCountParams, QueueGetCountResponse];
    "queue.moveToTop": [QueueMoveToTopParams, QueueMoveToTopResponse];
    "queue.remove": [QueueRemoveParams, QueueRemoveResponse];
    "rating.get": [RatingGetParams, RatingGetResponse];
    "rating.set": [RatingSetParams, RatingSetResponse];
    "replaygain.clear": [ReplaygainClearParams, ReplaygainClearResponse];
    "replaygain.get": [ReplaygainGetParams, ReplaygainGetResponse];
    "replaygain.getMode": [ReplaygainGetModeParams, ReplaygainGetModeResponse];
    "replaygain.getPreamp": [ReplaygainGetPreampParams, ReplaygainGetPreampResponse];
    "replaygain.getSettings": [ReplaygainGetSettingsParams, ReplaygainGetSettingsResponse];
    "replaygain.scan": [ReplaygainScanParams, ReplaygainScanResponse];
    "replaygain.setMode": [ReplaygainSetModeParams, ReplaygainSetModeResponse];
    "replaygain.setPreamp": [ReplaygainSetPreampParams, ReplaygainSetPreampResponse];
    "selection.get": [SelectionGetParams, SelectionGetResponse];
    "selection.getType": [SelectionGetTypeParams, SelectionGetTypeResponse];
    "selection.getViewerMode": [SelectionGetViewerModeParams, SelectionGetViewerModeResponse];
    "selection.getViewingTrack": [SelectionGetViewingTrackParams, SelectionGetViewingTrackResponse];
    "selection.set": [SelectionSetParams, SelectionSetResponse];
    "selection.setPlaylistTracking": [SelectionSetPlaylistTrackingParams, SelectionSetPlaylistTrackingResponse];
    "shell.exec": [ShellExecParams, ShellExecResponse];
    "shell.openExternal": [ShellOpenExternalParams, ShellOpenExternalResponse];
    "shell.openWith": [ShellOpenWithParams, ShellOpenWithResponse];
    "shell.showInExplorer": [ShellShowInExplorerParams, ShellShowInExplorerResponse];
    "shell.spawn": [ShellSpawnParams, ShellSpawnResponse];
    "state.delete": [StateDeleteParams, StateDeleteResponse];
    "state.get": [StateGetParams, StateGetResponse];
    "state.keys": [StateKeysParams, StateKeysResponse];
    "state.set": [StateSetParams, StateSetResponse];
    "system.getApisByNamespace": [SystemGetApisByNamespaceParams, SystemGetApisByNamespaceResponse];
    "system.getApiStats": [SystemGetApiStatsParams, SystemGetApiStatsResponse];
    "system.getDPI": [SystemGetDPIParams, SystemGetDPIResponse];
    "system.getLocale": [SystemGetLocaleParams, SystemGetLocaleResponse];
    "system.getRegisteredPlugins": [SystemGetRegisteredPluginsParams, SystemGetRegisteredPluginsResponse];
    "system.getTheme": [SystemGetThemeParams, SystemGetThemeResponse];
    "system.isPluginRegistered": [SystemIsPluginRegisteredParams, SystemIsPluginRegisteredResponse];
    "system.listAvailableApis": [SystemListAvailableApisParams, SystemListAvailableApisResponse];
    "system.searchApis": [SystemSearchApisParams, SystemSearchApisResponse];
    "taskbar.flash": [TaskbarFlashParams, TaskbarFlashResponse];
    "taskbar.setOverlayIcon": [TaskbarSetOverlayIconParams, TaskbarSetOverlayIconResponse];
    "taskbar.setProgress": [TaskbarSetProgressParams, TaskbarSetProgressResponse];
    "taskbar.setThumbnailButtons": [TaskbarSetThumbnailButtonsParams, TaskbarSetThumbnailButtonsResponse];
    "taskbar.updateButton": [TaskbarUpdateButtonParams, TaskbarUpdateButtonResponse];
    "test.echo": [TestEchoParams, TestEchoResponse];
    "test.ping": [TestPingParams, TestPingResponse];
    "titleformat.eval": [TitleformatEvalParams, TitleformatEvalResponse];
    "titleformat.evalBatch": [TitleformatEvalBatchParams, TitleformatEvalBatchResponse];
    "titleformat.evalFields": [TitleformatEvalFieldsParams, TitleformatEvalFieldsResponse];
    "titleformat.evalFieldsBatch": [TitleformatEvalFieldsBatchParams, TitleformatEvalFieldsBatchResponse];
    "titleformat.getBuiltinFields": [TitleformatGetBuiltinFieldsParams, TitleformatGetBuiltinFieldsResponse];
    "tray.appendMenuItems": [TrayAppendMenuItemsParams, TrayAppendMenuItemsResponse];
    "tray.clearMenuItems": [TrayClearMenuItemsParams, TrayClearMenuItemsResponse];
    "tray.create": [TrayCreateParams, TrayCreateResponse];
    "tray.destroy": [TrayDestroyParams, TrayDestroyResponse];
    "tray.getMenuItems": [TrayGetMenuItemsParams, TrayGetMenuItemsResponse];
    "tray.isVisible": [TrayIsVisibleParams, TrayIsVisibleResponse];
    "tray.removeMenuItems": [TrayRemoveMenuItemsParams, TrayRemoveMenuItemsResponse];
    "tray.setCloseToTray": [TraySetCloseToTrayParams, TraySetCloseToTrayResponse];
    "tray.setContextMenu": [TraySetContextMenuParams, TraySetContextMenuResponse];
    "tray.setIcon": [TraySetIconParams, TraySetIconResponse];
    "tray.setMenuItemState": [TraySetMenuItemStateParams, TraySetMenuItemStateResponse];
    "tray.setMinimizeToTray": [TraySetMinimizeToTrayParams, TraySetMinimizeToTrayResponse];
    "tray.setTooltip": [TraySetTooltipParams, TraySetTooltipResponse];
    "tray.showBalloon": [TrayShowBalloonParams, TrayShowBalloonResponse];
    "ui.hideNotification": [UiHideNotificationParams, UiHideNotificationResponse];
    "ui.showContextMenu": [UiShowContextMenuParams, UiShowContextMenuResponse];
    "ui.showCustomMenu": [UiShowCustomMenuParams, UiShowCustomMenuResponse];
    "ui.showNotification": [UiShowNotificationParams, UiShowNotificationResponse];
    "ui.showToast": [UiShowToastParams, UiShowToastResponse];
    "window.blur": [WindowBlurParams, WindowBlurResponse];
    "window.broadcast": [WindowBroadcastParams, WindowBroadcastResponse];
    "window.cancelClose": [WindowCancelCloseParams, WindowCancelCloseResponse];
    "window.center": [WindowCenterParams, WindowCenterResponse];
    "window.clearClickThroughExcludeRegions": [WindowClearClickThroughExcludeRegionsParams, WindowClearClickThroughExcludeRegionsResponse];
    "window.clearDragRegions": [WindowClearDragRegionsParams, WindowClearDragRegionsResponse];
    "window.clearNoDragRegions": [WindowClearNoDragRegionsParams, WindowClearNoDragRegionsResponse];
    "window.close": [WindowCloseParams, WindowCloseResponse];
    "window.closeAllPopups": [WindowCloseAllPopupsParams, WindowCloseAllPopupsResponse];
    "window.closePopup": [WindowClosePopupParams, WindowClosePopupResponse];
    "window.confirmClose": [WindowConfirmCloseParams, WindowConfirmCloseResponse];
    "window.createPopup": [WindowCreatePopupParams, WindowCreatePopupResponse];
    "window.enterFullscreen": [WindowEnterFullscreenParams, WindowEnterFullscreenResponse];
    "window.exitFullscreen": [WindowExitFullscreenParams, WindowExitFullscreenResponse];
    "window.flash": [WindowFlashParams, WindowFlashResponse];
    "window.flashTaskbar": [WindowFlashTaskbarParams, WindowFlashTaskbarResponse];
    "window.focus": [WindowFocusParams, WindowFocusResponse];
    "window.getAllWindows": [WindowGetAllWindowsParams, WindowGetAllWindowsResponse];
    "window.getBackdropPolicy": [WindowGetBackdropPolicyParams, WindowGetBackdropPolicyResponse];
    "window.getBounds": [WindowGetBoundsParams, WindowGetBoundsResponse];
    "window.getCaptionButtonsWidth": [WindowGetCaptionButtonsWidthParams, WindowGetCaptionButtonsWidthResponse];
    "window.getCornerPreference": [WindowGetCornerPreferenceParams, WindowGetCornerPreferenceResponse];
    "window.getCurrentWindowId": [WindowGetCurrentWindowIdParams, WindowGetCurrentWindowIdResponse];
    "window.getDevServerConfig": [WindowGetDevServerConfigParams, WindowGetDevServerConfigResponse];
    "window.getDpiScale": [WindowGetDpiScaleParams, WindowGetDpiScaleResponse];
    "window.getMaxSize": [WindowGetMaxSizeParams, WindowGetMaxSizeResponse];
    "window.getMinSize": [WindowGetMinSizeParams, WindowGetMinSizeResponse];
    "window.getMode": [WindowGetModeParams, WindowGetModeResponse];
    "window.getPopupBehavior": [WindowGetPopupBehaviorParams, WindowGetPopupBehaviorResponse];
    "window.getState": [WindowGetStateParams, WindowGetStateResponse];
    "window.getTitle": [WindowGetTitleParams, WindowGetTitleResponse];
    "window.getTitlebarHeight": [WindowGetTitlebarHeightParams, WindowGetTitlebarHeightResponse];
    "window.getTitlebarInfo": [WindowGetTitlebarInfoParams, WindowGetTitlebarInfoResponse];
    "window.getZoom": [WindowGetZoomParams, WindowGetZoomResponse];
    "window.hasSavedBounds": [WindowHasSavedBoundsParams, WindowHasSavedBoundsResponse];
    "window.isAlwaysOnTop": [WindowIsAlwaysOnTopParams, WindowIsAlwaysOnTopResponse];
    "window.isClickThrough": [WindowIsClickThroughParams, WindowIsClickThroughResponse];
    "window.isFullscreen": [WindowIsFullscreenParams, WindowIsFullscreenResponse];
    "window.isMaximized": [WindowIsMaximizedParams, WindowIsMaximizedResponse];
    "window.isMinimized": [WindowIsMinimizedParams, WindowIsMinimizedResponse];
    "window.isResizable": [WindowIsResizableParams, WindowIsResizableResponse];
    "window.maximize": [WindowMaximizeParams, WindowMaximizeResponse];
    "window.minimize": [WindowMinimizeParams, WindowMinimizeResponse];
    "window.refreshWebView": [WindowRefreshWebViewParams, WindowRefreshWebViewResponse];
    "window.reload": [WindowReloadParams, WindowReloadResponse];
    "window.resetZoom": [WindowResetZoomParams, WindowResetZoomResponse];
    "window.restore": [WindowRestoreParams, WindowRestoreResponse];
    "window.sendMessage": [WindowSendMessageParams, WindowSendMessageResponse];
    "window.setAcrylic": [WindowSetAcrylicParams, WindowSetAcrylicResponse];
    "window.setAlwaysOnTop": [WindowSetAlwaysOnTopParams, WindowSetAlwaysOnTopResponse];
    "window.setBackdropPolicy": [WindowSetBackdropPolicyParams, WindowSetBackdropPolicyResponse];
    "window.setBackgroundTransparency": [WindowSetBackgroundTransparencyParams, WindowSetBackgroundTransparencyResponse];
    "window.setBlur": [WindowSetBlurParams, WindowSetBlurResponse];
    "window.setBounds": [WindowSetBoundsParams, WindowSetBoundsResponse];
    "window.setClickThrough": [WindowSetClickThroughParams, WindowSetClickThroughResponse];
    "window.setClickThroughExcludeRegions": [WindowSetClickThroughExcludeRegionsParams, WindowSetClickThroughExcludeRegionsResponse];
    "window.setCornerPreference": [WindowSetCornerPreferenceParams, WindowSetCornerPreferenceResponse];
    "window.setDarkMode": [WindowSetDarkModeParams, WindowSetDarkModeResponse];
    "window.setDevServerConfig": [WindowSetDevServerConfigParams, WindowSetDevServerConfigResponse];
    "window.setDragRegions": [WindowSetDragRegionsParams, WindowSetDragRegionsResponse];
    "window.setFrameless": [WindowSetFramelessParams, WindowSetFramelessResponse];
    "window.setFullscreen": [WindowSetFullscreenParams, WindowSetFullscreenResponse];
    "window.setMaxSize": [WindowSetMaxSizeParams, WindowSetMaxSizeResponse];
    "window.setMica": [WindowSetMicaParams, WindowSetMicaResponse];
    "window.setMicaEffect": [WindowSetMicaEffectParams, WindowSetMicaEffectResponse];
    "window.setMinSize": [WindowSetMinSizeParams, WindowSetMinSizeResponse];
    "window.setNoDragRegions": [WindowSetNoDragRegionsParams, WindowSetNoDragRegionsResponse];
    "window.setPopupBehavior": [WindowSetPopupBehaviorParams, WindowSetPopupBehaviorResponse];
    "window.setPosition": [WindowSetPositionParams, WindowSetPositionResponse];
    "window.setResizable": [WindowSetResizableParams, WindowSetResizableResponse];
    "window.setSize": [WindowSetSizeParams, WindowSetSizeResponse];
    "window.setTitle": [WindowSetTitleParams, WindowSetTitleResponse];
    "window.setTitlebarHeight": [WindowSetTitlebarHeightParams, WindowSetTitlebarHeightResponse];
    "window.setZoom": [WindowSetZoomParams, WindowSetZoomResponse];
    "window.setZoomForDpi": [WindowSetZoomForDpiParams, WindowSetZoomForDpiResponse];
    "window.showSystemMenu": [WindowShowSystemMenuParams, WindowShowSystemMenuResponse];
    "window.startDrag": [WindowStartDragParams, WindowStartDragResponse];
    "window.startResize": [WindowStartResizeParams, WindowStartResizeResponse];
    "window.toggleAlwaysOnTop": [WindowToggleAlwaysOnTopParams, WindowToggleAlwaysOnTopResponse];
    "window.toggleFullscreen": [WindowToggleFullscreenParams, WindowToggleFullscreenResponse];
    "window.toggleMaximize": [WindowToggleMaximizeParams, WindowToggleMaximizeResponse];
}

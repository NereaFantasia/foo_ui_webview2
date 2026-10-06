import type { Int, Json, PlaylistGuid, ReportedPlaylistGuid, Track } from './common.js';

export interface Api {
  /**
   * Report how many playlists there are.
   * @zh 报告有多少个播放列表。
   */
  getCount(): GetCountResult;

  /**
   * List every playlist in playlist order.
   * @zh 按顺序列出全部播放列表。
   * @effect read
   */
  getAll(): GetAllResult;

  /**
   * Describe the active playlist, the one the user is looking at. `found` is `false` when there is
   * none, and the other fields are then absent.
   * @zh 描述活动播放列表，即用户正在看的那个。没有时 `found` 为 `false`，其余字段不出现。
   * @effect read
   */
  getActive(): GetActiveResult;

  /**
   * Make a playlist the active one. An index past the last playlist fails with `INVALID_INDEX`.
   * @zh 把一个播放列表设为活动列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败。
   * @effect write
   * @idempotent
   */
  setActive(params: SetActiveParams): void;

  /**
   * Describe the playing playlist, the one playback takes its next track from. `found` is `false`
   * when there is none, and the other fields are then absent.
   * @zh 描述正在播放的播放列表，即播放从中取下一首的那个。没有时 `found` 为 `false`，其余字段不出现。
   * @effect read
   */
  getPlaying(): GetPlayingResult;

  /**
   * Create an empty playlist. Fails with `OPERATION_FAILED` when foobar2000 refuses to create one.
   * @zh 新建一个空播放列表。foobar2000 拒绝创建时以 `OPERATION_FAILED` 失败。
   * @effect write
   */
  create(params: CreateParams): CreateResult;

  /**
   * Delete a playlist. Fails with `LOCKED` when a lock on the playlist refuses the removal, and
   * with `OPERATION_FAILED` when foobar2000 refuses it otherwise.
   * @zh 删除一个播放列表。列表上的锁拒绝删除时以 `LOCKED` 失败，foobar2000 因其他原因拒绝时以 `OPERATION_FAILED` 失败。
   * @effect destructive
   */
  remove(params: RemoveParams): void;

  /**
   * Rename a playlist. Fails with `LOCKED` when a lock on the playlist refuses the new name, and
   * with `OPERATION_FAILED` when foobar2000 refuses it otherwise.
   * @zh 重命名一个播放列表。列表上的锁拒绝新名字时以 `LOCKED` 失败，foobar2000 因其他原因拒绝时以 `OPERATION_FAILED` 失败。
   * @effect destructive
   * @idempotent
   */
  rename(params: RenameParams): void;

  /**
   * Remove every track from a playlist, saving an undo point first. A locked playlist fails with
   * `LOCKED` before anything changes; tracks left behind afterwards fail with `OPERATION_FAILED`,
   * the failure carrying the same fields as a success.
   * @zh 移除播放列表里的全部曲目，事先保存撤销点。已上锁的列表在任何改动之前以 `LOCKED` 失败；清除后仍有曲目时以 `OPERATION_FAILED` 失败，失败里带与成功时相同的字段。
   * @effect destructive
   * @idempotent
   */
  clear(params: ClearParams): ClearResult;

  /**
   * Copy a playlist, tracks included, into a new playlist placed right after it.
   * @zh 把一个播放列表连同曲目复制成新列表，放在原列表紧后面。
   * @effect write
   */
  duplicate(params: DuplicateParams): DuplicateResult;

  /**
   * Reorder the playlist list, giving for each new position either the current index of the
   * playlist that goes there (`newOrder`) or its GUID (`newOrderGuids`); give exactly one of the
   * two, or the call fails with `INVALID_PARAMS`. Indices are read as the call arrives, so indices
   * taken from `playlist.getAll` before another change reorder the wrong playlists without an
   * error as long as the count still matches; GUIDs keep naming the playlists they were read
   * from. A list whose length is not the playlist count fails with `INVALID_PARAMS`, and so does
   * one naming a playlist twice or a malformed GUID; an index past the last playlist fails with
   * `INVALID_INDEX`, a GUID no playlist has with `NOT_FOUND`.
   * @zh 重排播放列表的顺序，为每个新位置给出放到那里的播放列表的当前索引（`newOrder`）或它的 GUID（`newOrderGuids`）；两者恰好给一个，否则以 `INVALID_PARAMS` 失败。索引按调用到达时的清单解读，所以在别的改动之前从 `playlist.getAll` 取来的索引，只要个数还对得上，就会不报错地排错列表；GUID 始终指向读取时的那些列表。长度与播放列表个数不同时以 `INVALID_PARAMS` 失败，同一个列表出现两次或 GUID 格式不对也是；某一项索引超出最后一个播放列表时以 `INVALID_INDEX` 失败，没有列表持有某个 GUID 时以 `NOT_FOUND` 失败。
   * @effect destructive
   */
  reorderPlaylists(params: ReorderPlaylistsParams): ReorderPlaylistsResult;

  /**
   * Revert a playlist to its last undo point. Fails with `NOT_FOUND` when there is none, and with
   * `LOCKED` when a lock on the playlist refuses the change.
   * @zh 把播放列表恢复到上一个撤销点。没有撤销点时以 `NOT_FOUND` 失败，列表上的锁拒绝改动时以 `LOCKED` 失败。
   * @effect destructive
   */
  undo(params: UndoParams): void;

  /**
   * Reapply the change the last `playlist.undo` reverted. Fails with `NOT_FOUND` when there is
   * nothing to redo, and with `LOCKED` when a lock on the playlist refuses the change.
   * @zh 重新应用上一次 `playlist.undo` 撤销的改动。没有可重做的改动时以 `NOT_FOUND` 失败，列表上的锁拒绝改动时以 `LOCKED` 失败。
   * @effect destructive
   */
  redo(params: RedoParams): void;

  /**
   * Report whether a playlist is an autoplaylist: one foobar2000 fills from a library query, or
   * one carrying a lock whose name contains `Auto` (autoplaylists that other components manage).
   * @zh 报告一个播放列表是否为自动播放列表：由 foobar2000 按媒体库查询填充的，或带有名字含 `Auto` 的锁的（由其他组件管理的自动播放列表）。
   */
  isAutoplaylist(params: IsAutoplaylistParams): IsAutoplaylistResult;

  /**
   * Create a playlist that foobar2000 fills from a library query and keeps up to date. An
   * autoplaylist foobar2000 refuses to set up fails with `OPERATION_FAILED`, and the new playlist
   * is removed again.
   * @zh 新建一个由 foobar2000 按媒体库查询填充并保持更新的播放列表。foobar2000 拒绝建立自动播放列表时以 `OPERATION_FAILED` 失败，新建的列表随之删除。
   * @effect write
   */
  createAutoplaylist(params: CreateAutoplaylistParams): CreateAutoplaylistResult;

  /**
   * Turn an existing playlist into an autoplaylist; its tracks are replaced by the query results.
   * Fails with `OPERATION_FAILED` when foobar2000 refuses, for instance because the playlist is
   * already an autoplaylist.
   * @zh 把已有的播放列表变成自动播放列表，曲目换成查询结果。foobar2000 拒绝时（例如它已是自动播放列表）以 `OPERATION_FAILED` 失败。
   * @effect destructive
   */
  convertToAutoplaylist(params: ConvertToAutoplaylistParams): ConvertToAutoplaylistResult;

  /**
   * Turn an autoplaylist back into an ordinary playlist that keeps its current tracks. An
   * autoplaylist that another component manages cannot be released this way: the call succeeds
   * with `source` `dui` and changes nothing, and `playlist.remove` deletes it. A playlist that is
   * not an autoplaylist fails with `NOT_FOUND`.
   * @zh 把自动播放列表变回保留当前曲目的普通列表。由其他组件管理的自动播放列表不能这样解除：调用成功、`source` 为 `dui`，但什么也不改，要删除它用 `playlist.remove`。不是自动播放列表时以 `NOT_FOUND` 失败。
   * @effect destructive
   */
  removeAutoplaylist(params: RemoveAutoplaylistParams): RemoveAutoplaylistResult;

  /**
   * Describe a playlist's autoplaylist status.
   * @zh 描述一个播放列表的自动播放列表状态。
   * @effect read
   */
  getAutoplaylistInfo(params: GetAutoplaylistInfoParams): GetAutoplaylistInfoResult;

  /**
   * Describe a playlist's autoplaylist status, as `playlist.getAutoplaylistInfo` does. foobar2000
   * does not expose the query of an autoplaylist, so `query` is always `null`.
   * @zh 描述一个播放列表的自动播放列表状态，与 `playlist.getAutoplaylistInfo` 相同。foobar2000 不公开自动播放列表的查询语句，所以 `query` 总是 `null`。
   */
  getAutoplaylistQuery(params: GetAutoplaylistQueryParams): GetAutoplaylistQueryResult;

  /**
   * Report whether a playlist carries a lock, such as an autoplaylist's.
   * @zh 报告一个播放列表是否带锁，例如自动播放列表的锁。
   * @effect read
   */
  getLockInfo(params: GetLockInfoParams): GetLockInfoResult;

  /**
   * Report whether a playlist carries a lock. An index past the last playlist fails with
   * `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`. Every failure to find the
   * playlist, and giving both keys or a malformed GUID, carries `isLocked` `false`.
   * @zh 报告一个播放列表是否带锁。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败。找不到列表的各种失败，以及两个键都给或 GUID 格式不对，失败里都带 `isLocked` 为 `false`。
   */
  isLocked(params: IsLockedParams): IsLockedResult;

  /**
   * List the playlist columns that foobar2000 and the installed components define for the
   * Default UI playlist view.
   * @zh 列出 foobar2000 与已装组件为默认界面播放列表视图定义的列。
   * @effect read
   */
  getAvailableColumns(): GetAvailableColumnsResult;

  /**
   * Sort a playlist by a Title Formatting pattern, saving an undo point first. A locked playlist
   * fails with `LOCKED`.
   * @zh 按 Title Formatting 模式给播放列表排序，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。
   * @effect destructive
   * @idempotent
   */
  sort(params: SortParams): void;

  /**
   * Put a playlist's tracks in random order, saving an undo point first. A locked playlist fails
   * with `LOCKED`.
   * @zh 把播放列表的曲目打乱成随机顺序，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。
   * @effect destructive
   */
  shuffle(params: ShuffleParams): void;

  /**
   * Reverse the order of a playlist's tracks, saving an undo point first. A locked playlist fails
   * with `LOCKED`.
   * @zh 把播放列表的曲目顺序倒过来，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。
   * @effect destructive
   */
  reverse(params: ReverseParams): void;

  /**
   * Reorder a playlist's tracks, saving an undo point first: `newOrder[i]` is the current row of
   * the track that moves to row `i`. A locked playlist fails with `LOCKED`, a list whose length is
   * not the track count or that names a row twice with `INVALID_PARAMS`, an entry past the last
   * row with `INVALID_INDEX`.
   * @zh 重排播放列表的曲目，事先保存撤销点：`newOrder[i]` 是移到第 `i` 行的那首曲目的当前行号。已上锁的列表以 `LOCKED` 失败，长度与曲目数不同或同一行出现两次时以 `INVALID_PARAMS` 失败，某一项超出最后一行时以 `INVALID_INDEX` 失败。
   * @effect destructive
   */
  reorder(params: ReorderParams): ReorderResult;


  /**
   * Report how many tracks a playlist holds. An index past the last playlist, a `playlistGuid`
   * no playlist has, or no active playlist when both are omitted, reports `0` rather than
   * failing.
   * @zh 报告一个播放列表有多少首曲目。超出最后一个播放列表的索引、没有列表持有的 `playlistGuid`，或两者都省略且没有活动播放列表时，报告 `0` 而不是失败。
   */
  getTrackCount(params: GetTrackCountParams): GetTrackCountResult;

  /**
   * List the selected rows of a playlist.
   * @zh 列出播放列表中被选中的行。
   * @effect read
   */
  getSelection(params: GetSelectionParams): GetSelectionResult;

  /**
   * Select rows of a playlist. Rows past the last one are ignored; a negative row fails with
   * `INVALID_PARAMS`.
   * @zh 选中播放列表中的行。超出最后一行的行号忽略，负数行号以 `INVALID_PARAMS` 失败。
   * @effect write
   * @idempotent
   */
  setSelection(params: SetSelectionParams): void;

  /**
   * Select every row of a playlist.
   * @zh 选中播放列表中的全部行。
   * @effect write
   * @idempotent
   */
  selectAll(params: SelectAllParams): void;

  /**
   * Clear the selection of a playlist.
   * @zh 清除播放列表中的选中。
   * @effect write
   * @idempotent
   */
  deselectAll(params: DeselectAllParams): void;

  /**
   * Remove rows from a playlist, saving an undo point first. Rows past the last one are ignored;
   * a negative row fails with `INVALID_PARAMS`. A locked playlist fails with `LOCKED`.
   * @zh 从播放列表移除行，事先保存撤销点。超出最后一行的行号忽略，负数行号以 `INVALID_PARAMS` 失败。已上锁的列表以 `LOCKED` 失败。
   * @effect destructive
   */
  removeTracks(params: RemoveTracksParams): void;

  /**
   * Remove the selected rows from a playlist, saving an undo point first. A locked playlist fails
   * with `LOCKED`.
   * @zh 从播放列表移除选中的行，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。
   * @effect destructive
   */
  removeSelectedTracks(params: RemoveSelectedTracksParams): void;

  /**
   * Move the selected rows of a playlist by `delta` positions, saving an undo point first. With
   * `items`, the selection is first replaced by those rows; the caller's own selection is lost. A
   * locked playlist fails with `LOCKED`.
   * @zh 把播放列表中选中的行移动 `delta` 个位置，事先保存撤销点。给了 `items` 时先把选中换成这些行，调用方原有的选中随之丢失。已上锁的列表以 `LOCKED` 失败。
   * @effect destructive
   */
  moveTracks(params: MoveTracksParams): void;

  /**
   * Play a row of a playlist, as double-clicking it does. A row past the last one fails with
   * `INVALID_INDEX`.
   * @zh 播放播放列表中的一行，与双击它相同。超出最后一行的行号以 `INVALID_INDEX` 失败。
   * @effect write
   */
  playTrack(params: PlayTrackParams): void;

  /**
   * Move the focus of a playlist; the same operation as `playlist.setFocusedTrack` under its older
   * name.
   * @zh 移动播放列表的焦点；与 `playlist.setFocusedTrack` 是同一个操作，这是它的旧名字。
   */
  focusTrack(params: FocusTrackParams): void;

  /**
   * Report the focused row of a playlist; the same as `playlist.getFocusedTrack` under its older
   * name, except that a missing playlist fails instead of answering `index` `-1`: an index past
   * the last playlist with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`,
   * and no active playlist when both are omitted with `NO_ACTIVE_ITEM`. On success `playlist` and
   * `playlistGuid` are always present.
   * @zh 报告播放列表的焦点行；与 `playlist.getFocusedTrack` 相同，这是它的旧名字，区别是列表不存在时失败而不是回答 `index` 为 `-1`：超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败，两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。成功时恒带 `playlist` 与 `playlistGuid`。
   */
  getFocusTrack(params: GetFocusTrackParams): GetFocusTrackResult;

  /**
   * Report the focused row of a playlist. An index past the last playlist, a `playlistGuid` no
   * playlist has, or no active playlist when both are omitted, reports `index` `-1` without
   * `playlist` and `playlistGuid` rather than failing.
   * @zh 报告播放列表的焦点行。超出最后一个播放列表的索引、没有列表持有的 `playlistGuid`，或两者都省略且没有活动播放列表时，报告 `index` 为 `-1` 且不带 `playlist` 与 `playlistGuid`，而不是失败。
   * @effect read
   */
  getFocusedTrack(params: GetFocusedTrackParams): GetFocusedTrackResult;

  /**
   * Move the focus of a playlist to a row, or remove it. A row past the last one fails with
   * `INVALID_INDEX`.
   * @zh 把播放列表的焦点移到一行，或去掉焦点。超出最后一行的行号以 `INVALID_INDEX` 失败。
   * @effect write
   * @idempotent
   */
  setFocusedTrack(params: SetFocusedTrackParams): void;

  /**
   * Insert tracks at a position of a playlist, saving an undo point first. Each entry of `handles`
   * is a path with an optional `|subsong:N` suffix, or `{ path, subsong }`; entries that are
   * neither, or that name nothing, are counted in `invalidCount`. When none is usable the call
   * fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`.
   * @zh 在播放列表的某个位置插入曲目，事先保存撤销点。`handles` 的每一项是路径（可带 `|subsong:N` 后缀）或 `{ path, subsong }`；两者都不是或指不到任何东西的项计入 `invalidCount`。一项都不可用时以 `NOT_FOUND` 失败。已上锁的列表以 `LOCKED` 失败。
   * @effect write
   */
  insertTracks(params: InsertTracksParams): InsertTracksResult;

  /**
   * Append files, folders or URLs to a playlist, saving an undo point first. foobar2000 resolves
   * the batch as its own Add Files does: folders and playlist files are expanded, duplicates are
   * dropped and the rows are sorted by the user's incoming-item order, so they do not keep the
   * given order (`playlist.addPathsSequential` does). Entries longer than 2048 characters, and
   * entries that resolve to nothing, are counted in `invalidCount`. When nothing is added the call
   * fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`. foobar2000 can show a progress
   * dialog while it resolves the paths, and the playlist is looked up again afterwards: moved
   * meanwhile, the tracks still go into it and `playlist` in the result is its new index; removed
   * meanwhile, the call fails with `OPERATION_FAILED`.
   * @zh 把文件、文件夹或 URL 追加到播放列表，事先保存撤销点。foobar2000 按它自己的「添加文件」处理这批路径：展开文件夹与播放列表文件、去掉重复项，并按用户设置的新增项顺序排序，所以不保持传入顺序（要保序用 `playlist.addPathsSequential`）。超过 2048 个字符的项与解析不出任何曲目的项计入 `invalidCount`。一首都没加上时以 `NOT_FOUND` 失败。已上锁的列表以 `LOCKED` 失败。foobar2000 解析路径时可能显示进度框，解析完会重新查找这张列表：期间被挪动，曲目照样加进它，结果里的 `playlist` 是它的新索引；期间被删掉，以 `OPERATION_FAILED` 失败。
   * @effect write
   */
  addPaths(params: AddPathsParams): AddPathsResult;

  /**
   * Append tracks to a playlist, saving an undo point first. `handles` takes the same entries as
   * `playlist.insertTracks`; when none is usable the call fails with `NOT_FOUND`. A locked playlist
   * fails with `LOCKED`.
   * @zh 把曲目追加到播放列表，事先保存撤销点。`handles` 的项与 `playlist.insertTracks` 相同；一项都不可用时以 `NOT_FOUND` 失败。已上锁的列表以 `LOCKED` 失败。
   * @effect write
   */
  addHandles(params: AddHandlesParams): AddHandlesResult;

  /**
   * Append paths to a playlist in the given order, saving an undo point first; a path that
   * expands to several tracks (a folder, a cue sheet) keeps its place. Entries longer than 2048
   * characters, and entries that resolve to nothing, are skipped. A locked playlist fails with
   * `LOCKED`. A playlist moved or removed while the paths are resolved is handled as in
   * `playlist.addPaths`.
   * @zh 按传入顺序把路径追加到播放列表，事先保存撤销点；展开成多首曲目的路径（文件夹、cue）占住它自己的位置。超过 2048 个字符的项与解析不出任何曲目的项跳过。已上锁的列表以 `LOCKED` 失败。解析路径期间列表被挪动或删掉时，处理同 `playlist.addPaths`。
   * @effect write
   */
  addPathsSequential(params: AddPathsSequentialParams): AddPathsSequentialResult;

  /**
   * Start appending paths to a playlist without waiting: the call returns a receipt, and
   * `playlist:addComplete` with the same `operationId` reports the outcome. Plain files and URLs
   * are added before the call returns; playlist files (`.pls`, `.m3u`, `.cue` and the like) are
   * expanded in the background. Entries that are empty or longer than 2048 characters are counted
   * in `invalidCount`; when no entry is left the call fails with `INVALID_PARAMS`. A locked
   * playlist fails with `LOCKED`. The expanded tracks go into the same playlist even if it was
   * moved meanwhile; if it was removed or locked meanwhile they are dropped.
   * @zh 开始向播放列表追加路径，不等它完成：调用返回一张回执，完成情况由带同一 `operationId` 的 `playlist:addComplete` 报告。普通文件与 URL 在调用返回前就已加入；播放列表文件（`.pls`、`.m3u`、`.cue` 等）在后台展开。为空或超过 2048 个字符的项计入 `invalidCount`；一项都不剩时以 `INVALID_PARAMS` 失败。已上锁的列表以 `LOCKED` 失败。展开出的曲目加进同一张列表，期间它被挪动也一样；期间它被删掉或上锁，这些曲目就丢弃。
   */
  addPathsAsync(params: AddPathsAsyncParams): AddPathsAsyncResult;

  /**
   * Replace the whole content of a playlist and play it: stop playback, clear the playlist
   * (saving an undo point), add the paths as `playlist.addPaths` does, make the playlist active
   * and play or focus `playIndex`. When nothing could be added the call fails with `NOT_FOUND`,
   * and the playlist stays empty. A locked playlist fails with `LOCKED` before anything changes.
   * The paths are resolved after the clear, as in `playlist.addPaths`: a playlist moved meanwhile
   * is still the one filled and played; one removed meanwhile fails with `OPERATION_FAILED`, and
   * one locked meanwhile fails with `LOCKED` and stays empty.
   * @zh 替换播放列表的全部内容并播放：停止播放，清空列表（保存撤销点），按 `playlist.addPaths` 的方式加入路径，把列表设为活动列表，再播放或聚焦 `playIndex`。一首都没加上时以 `NOT_FOUND` 失败，列表保持为空。已上锁的列表在任何改动之前以 `LOCKED` 失败。路径在清空之后解析，处理同 `playlist.addPaths`：期间被挪动的列表照样被填充和播放；期间被删掉时以 `OPERATION_FAILED` 失败；期间被上锁时以 `LOCKED` 失败，列表保持为空。
   * @effect destructive
   */
  replaceAllAndPlay(params: ReplaceAllAndPlayParams): ReplaceAllAndPlayResult;

  /**
   * A page of a playlist's rows. Without `fields` each row is a whole playlist row with the play
   * statistics foo_playcount provides; `fields` narrows every row to the named keys, from the list
   * `library.query` takes, and always keeps `index`. A playlist index past the last one, a
   * `playlistGuid` no playlist has, and no active playlist when both are omitted, answer an
   * empty page.
   * @zh 播放列表的一页行。不带 `fields` 时每行是完整的播放列表行，并带上 foo_playcount 提供的播放统计；`fields` 把每行收窄到点名的键（可用的名字与 `library.query` 相同），`index` 总会保留。播放列表索引超出最后一个、没有列表持有的 `playlistGuid`，以及两者都省略且没有活动播放列表时，回答一个空页。
   * @effect read
   */
  getTracks(params: GetTracksParams): GetTracksResult;

  /**
   * The selected rows of a playlist in playlist order, without the play statistics. An index past
   * the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with
   * `NOT_FOUND`; with both omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`.
   * Each of these failures, and giving both keys or a malformed GUID, carries an empty `tracks`.
   * @zh 播放列表里选中的行，按列表顺序，不带播放统计。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败；两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。这些失败，以及两个键都给或 GUID 格式不对，都带空的 `tracks`。
   * @effect read
   */
  getSelectedTracks(params: GetSelectedTracksParams): GetSelectedTracksResult;

  /**
   * Split a whole playlist into runs of adjacent rows whose group key is equal, with ASCII letters
   * compared case-insensitively; a second pattern sub-groups each run. Rows are never reordered and
   * only the run boundaries come back, so the answer grows with the number of runs, not of rows.
   * Keys are evaluated off the main thread. More than two patterns, an empty pattern or one that
   * fails to compile fails with `INVALID_PARAMS`, the last two with `details.pattern` giving its
   * position. An index past the last playlist fails with `INVALID_INDEX` and `details.playlist`
   * giving the index asked for, a `playlistGuid` no playlist has with `NOT_FOUND` and
   * `details.playlistGuid`; with both omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`. An error while the keys are evaluated fails with `OPERATION_FAILED`, the
   * failure carrying an empty `runs` and `total` `0`.
   * @zh 把整个播放列表切成相邻且分组键相等的若干游程，ASCII 字母不区分大小写；第二个模式在每个游程内再分组。不会重排行，只回游程边界，所以回答随游程数而不是行数增长。分组键在主线程之外求值。多于两个模式、空模式或编译失败的模式以 `INVALID_PARAMS` 失败，后两者带 `details.pattern` 指出位置。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败并以 `details.playlist` 带回请求的索引，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败并带 `details.playlistGuid`；两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。求分组键时出错以 `OPERATION_FAILED` 失败，失败里带空的 `runs` 与为 `0` 的 `total`。
   */
  getGroupRuns(params: GetGroupRunsParams): GetGroupRunsResult;

  /**
   * The rows of a playlist whose tracks match a foobar2000 query, in playlist order, in one call
   * however long the playlist is; read the rows themselves with `playlist.getTracksAt`. The query
   * takes the Media Library search syntax without `SORT BY`. A query the parser rejects fails with
   * `INVALID_PARAMS` and `details.param` `query`, and so does one carrying `SORT BY`; many
   * malformed queries are not rejected but simply match nothing. An index past the last playlist
   * fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and with both
   * omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. The answer is a snapshot
   * taken on the main thread: editing the playlist or a track's tags can change it, and so can the
   * passing of time for a query such as `%added% DURING LAST 2 WEEKS`, which no event announces.
   * @zh 播放列表里曲目符合 foobar2000 查询的行，按列表顺序；不管列表多长都只要一次调用，各行本身用 `playlist.getTracksAt` 读取。查询用媒体库搜索的语法，不收 `SORT BY`。解析器拒绝的查询以 `INVALID_PARAMS` 失败并带 `details.param` 为 `query`，带 `SORT BY` 的同样如此；很多畸形查询并不会被拒绝，只是什么也匹配不到。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败，两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。回答是在主线程上取的快照：改动这张列表或曲目的标签都可能改变它，`%added% DURING LAST 2 WEEKS` 这类查询还会随时间变化，这一种没有事件通知。
   * @effect read
   */
  getMatchingRows(params: GetMatchingRowsParams): GetMatchingRowsResult;

  /**
   * Rows of a playlist picked by row number, in the order given, such as the rows
   * `playlist.getMatchingRows` answers, which need not be adjacent. Each row is shaped as in
   * `playlist.getTracks` and carries its `index`; a row past the last one is skipped, and a row
   * given twice comes back twice. A playlist index past the last one, a `playlistGuid` no playlist
   * has, and no active playlist when both are omitted, answer an empty result as
   * `playlist.getTracks` does.
   * @zh 按行号挑出的播放列表行，按传入顺序，例如 `playlist.getMatchingRows` 回答的那些不一定相邻的行。每行的形状与 `playlist.getTracks` 相同并带 `index`；超出最后一行的行号跳过，给了两次的行返回两次。播放列表索引超出最后一个、没有列表持有的 `playlistGuid`，以及两者都省略且没有活动播放列表时，与 `playlist.getTracks` 一样回答空结果。
   * @effect read
   */
  getTracksAt(params: GetTracksAtParams): GetTracksAtResult;
}

export interface Events {
  /**
   * Tracks were inserted into a playlist. The JIT queue's hidden playlist sends nothing.
   * @zh 曲目插入了某个播放列表。JIT 队列的隐藏播放列表不发。
   * @delivery broadcast
   */
  itemsAdded: ItemsAddedPayload;

  /**
   * Tracks were removed from a playlist. The JIT queue's hidden playlist sends nothing.
   * @zh 曲目从某个播放列表里移除了。JIT 队列的隐藏播放列表不发。
   * @delivery broadcast
   */
  itemsRemoved: ItemsRemovedPayload;

  /**
   * A playlist's tracks were reordered. The JIT queue's hidden playlist sends nothing.
   * @zh 某个播放列表的曲目顺序变了。JIT 队列的隐藏播放列表不发。
   * @delivery broadcast
   */
  itemsReordered: ItemsReorderedPayload;

  /**
   * The selection in a playlist changed; read it with `playlist.getSelection`. The JIT queue's
   * hidden playlist sends nothing.
   * @zh 某个播放列表的选择变了，用 `playlist.getSelection` 读取。JIT 队列的隐藏播放列表不发。
   * @delivery broadcast
   */
  selectionChanged: SelectionChangedPayload;

  /**
   * The focused row of a playlist moved. The JIT queue's hidden playlist sends nothing.
   * @zh 某个播放列表的焦点行移动了。JIT 队列的隐藏播放列表不发。
   * @delivery broadcast
   */
  focusChanged: FocusChangedPayload;

  /**
   * Tracks of a playlist were replaced in place by other tracks. The JIT queue's hidden playlist
   * sends nothing.
   * @zh 某个播放列表的曲目被原位换成了别的曲目。JIT 队列的隐藏播放列表不发。
   * @delivery broadcast
   */
  itemsReplaced: ItemsReplacedPayload;

  /**
   * A playlist was created.
   * @zh 新建了一个播放列表。
   * @delivery broadcast
   */
  created: CreatedPayload;

  /**
   * One or more playlists were removed.
   * @zh 移除了一个或多个播放列表。
   * @delivery broadcast
   */
  removed: RemovedPayload;

  /**
   * The playlists were reordered.
   * @zh 播放列表的顺序变了。
   * @delivery broadcast
   */
  reordered: ReorderedPayload;

  /**
   * The active playlist changed.
   * @zh 活动播放列表变了。
   * @delivery broadcast
   */
  activated: ActivatedPayload;

  /**
   * A playlist was renamed.
   * @zh 某个播放列表改了名。
   * @delivery broadcast
   */
  renamed: RenamedPayload;

  /**
   * A lock was put on a playlist or taken off it.
   * @zh 某个播放列表加上或去掉了锁。
   * @delivery broadcast
   */
  lockChanged: LockChangedPayload;

  /**
   * foobar2000 reported that its default playlist format changed.
   * @zh foobar2000 报告它的默认播放列表格式变了。
   * @delivery broadcast
   */
  defaultFormatChanged: void;

  /**
   * A `playlist.addPathsAsync` call finished adding its paths. Paths that need expanding (such as
   * `.cue` or `.m3u`) are expanded first; when every path can be added directly, this follows the
   * call at once. A call that was answered with an error sends nothing.
   * @zh 一次 `playlist.addPathsAsync` 调用加完了它的路径。需要展开的路径（如 `.cue`、`.m3u`）先展开；所有路径都能直接加入时，紧跟调用发出。以错误应答的调用不发。
   * @delivery broadcast
   */
  addComplete: AddCompletePayload;
}

/**
 * One playlist row: the shared track fields plus the row number and two more tags.
 * @zh 播放列表的一行：共用的曲目字段，加上行号与另外两个标签。
 */
interface PlaylistTrack extends Track {
  /**
   * Row in the playlist, from `0`.
   * @zh 在播放列表中的行号，从 `0` 起。
   */
  index: Int;
  /**
   * Every COMPOSER value joined with `", "`; empty when untagged.
   * @zh 全部 COMPOSER 值用 `", "` 连接；没有标签时为空。
   */
  composer: string;
  /**
   * First COMMENT value; empty when untagged.
   * @zh 第一个 COMMENT 值；没有标签时为空。
   */
  comment: string;
  /**
   * `%play_count%` from foo_playcount; absent when it gives no number, and on rows of
   * `playlist.getSelectedTracks` or of a `fields` request.
   * @zh foo_playcount 的 `%play_count%`；它给不出数字时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。
   */
  playCount?: Int;
  /**
   * `%first_played%` as foo_playcount formats it; absent when empty, on rows of
   * `playlist.getSelectedTracks` and on rows of a `fields` request.
   * @zh foo_playcount 格式化的 `%first_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。
   */
  firstPlayed?: string;
  /**
   * `%last_played%` as foo_playcount formats it; absent when empty, on rows of
   * `playlist.getSelectedTracks` and on rows of a `fields` request.
   * @zh foo_playcount 格式化的 `%last_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。
   */
  lastPlayed?: string;
  /**
   * `%added%` as foo_playcount formats it; absent when empty, on rows of
   * `playlist.getSelectedTracks` and on rows of a `fields` request.
   * @zh foo_playcount 格式化的 `%added%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。
   */
  added?: string;
  /**
   * The `formats` columns of `playlist.getTracks`, by the names given; present only when `formats`
   * was passed.
   * @zh `playlist.getTracks` 的 `formats` 列，按给定的列名；只在传了 `formats` 时出现。
   */
  formats?: Record<string, string>;
}

/**
 * One run of adjacent rows sharing a group key.
 * @zh 分组键相同的一段相邻行。
 */
interface PlaylistGroupRun {
  /**
   * First row of the run.
   * @zh 游程的第一行。
   */
  start: Int;
  /**
   * Rows in the run.
   * @zh 游程的行数。
   */
  count: Int;
  /**
   * The group key as the first row of the run spells it.
   * @zh 分组键，取游程第一行的写法。
   */
  key: string;
  /**
   * Second-level runs inside this one; present only when two patterns were given.
   * @zh 这一游程内的第二级游程；只在给了两个模式时出现。
   */
  sub?: PlaylistGroupSubRun[];
}

/**
 * One second-level run. `start` is a row of the playlist, not an offset within the parent run.
 * @zh 一个第二级游程。`start` 是播放列表的行号，不是在上级游程内的偏移。
 */
interface PlaylistGroupSubRun {
  /**
   * First row of the run.
   * @zh 游程的第一行。
   */
  start: Int;
  /**
   * Rows in the run.
   * @zh 游程的行数。
   */
  count: Int;
  /**
   * The second-level key as the first row of the run spells it.
   * @zh 第二级分组键，取游程第一行的写法。
   */
  key: string;
}

/**
 * One entry of the playlist list.
 * @zh 播放列表清单中的一项。
 */
interface PlaylistInfo {
  /**
   * Position in the playlist list, from `0`.
   * @zh 在播放列表清单中的位置，从 `0` 起。
   */
  index: Int;
  guid: ReportedPlaylistGuid;
  /**
   * Playlist name.
   * @zh 播放列表名称。
   */
  name: string;
  /**
   * Number of tracks.
   * @zh 曲目数。
   */
  trackCount: Int;
  /**
   * Whether this is the active playlist.
   * @zh 是否为活动播放列表。
   */
  isActive: boolean;
  /**
   * Whether this is the playing playlist.
   * @zh 是否为正在播放的播放列表。
   */
  isPlaying: boolean;
  /**
   * Whether the playlist carries a lock.
   * @zh 播放列表是否带锁。
   */
  isLocked: boolean;
  /**
   * Whether the playlist is an autoplaylist, as `playlist.isAutoplaylist` reports it.
   * @zh 是否为自动播放列表，口径同 `playlist.isAutoplaylist`。
   */
  isAutoplaylist: boolean;
}

/**
 * A playlist column the Default UI playlist view offers.
 * @zh 默认界面播放列表视图提供的一列。
 */
interface PlaylistColumnDefinition {
  /**
   * Column GUID, written as `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX` in upper case without braces,
   * unlike a playlist GUID.
   * @zh 列的 GUID，写成大写、不带花括号的 `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`，与播放列表 GUID 的写法不同。
   */
  id: string;
  /**
   * Display name.
   * @zh 显示名称。
   */
  name: string;
  /**
   * Title Formatting pattern of the cell text.
   * @zh 单元格文字的 Title Formatting 模式。
   */
  pattern: string;
  /**
   * Alignment of the cell text.
   * @zh 单元格文字的对齐方式。
   */
  alignment: 'left' | 'right' | 'center';
  /**
   * Whether the column holds numbers.
   * @zh 该列是否为数字。
   */
  numeric: boolean;
  /**
   * Title Formatting pattern used to sort by this column; absent when sorting uses `pattern`.
   * @zh 按该列排序时使用的 Title Formatting 模式；排序使用 `pattern` 时不出现。
   */
  sortPattern?: string;
}

interface GetCountResult {
  /**
   * Number of playlists.
   * @zh 播放列表个数。
   */
  count: Int;
}

interface GetAllResult {
  /**
   * Every playlist, in playlist order.
   * @zh 全部播放列表，按顺序。
   */
  playlists: PlaylistInfo[];
  /**
   * Number of entries in `playlists`.
   * @zh `playlists` 的条目数。
   */
  count: Int;
}

interface GetActiveResult {
  /**
   * Whether there is an active playlist.
   * @zh 是否有活动播放列表。
   */
  found: boolean;
  /**
   * Index of the active playlist.
   * @zh 活动播放列表的索引。
   */
  index?: Int;
  guid?: ReportedPlaylistGuid;
  /**
   * Its name.
   * @zh 它的名称。
   */
  name?: string;
  /**
   * Number of tracks.
   * @zh 曲目数。
   */
  trackCount?: Int;
  /**
   * Always `true` here.
   * @zh 这里总是 `true`。
   */
  isActive?: boolean;
  /**
   * Whether it is also the playing playlist.
   * @zh 它是否同时是正在播放的播放列表。
   */
  isPlaying?: boolean;
  /**
   * Whether it carries a lock.
   * @zh 它是否带锁。
   */
  isLocked?: boolean;
  /**
   * Total length of its tracks in seconds; tracks of unknown length count as `0`.
   * @zh 曲目总时长，单位秒；时长未知的曲目按 `0` 计。
   */
  duration?: number;
}

interface SetActiveParams {
  /**
   * Index of the playlist to activate. Give this or `playlistGuid`; with neither the call fails
   * with `INVALID_PARAMS`.
   * @zh 要设为活动的播放列表的索引。给它或 `playlistGuid` 之一；两个都没给时以 `INVALID_PARAMS` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetPlayingResult {
  /**
   * Whether there is a playing playlist.
   * @zh 是否有正在播放的播放列表。
   */
  found: boolean;
  /**
   * Index of the playing playlist.
   * @zh 正在播放的播放列表的索引。
   */
  index?: Int;
  guid?: ReportedPlaylistGuid;
  /**
   * Its name.
   * @zh 它的名称。
   */
  name?: string;
  /**
   * Number of tracks.
   * @zh 曲目数。
   */
  trackCount?: Int;
  /**
   * Whether it is also the active playlist.
   * @zh 它是否同时是活动播放列表。
   */
  isActive?: boolean;
  /**
   * Always `true` here.
   * @zh 这里总是 `true`。
   */
  isPlaying?: boolean;
  /**
   * Whether it carries a lock.
   * @zh 它是否带锁。
   */
  isLocked?: boolean;
  /**
   * Total length of its tracks in seconds; tracks of unknown length count as `0`.
   * @zh 曲目总时长，单位秒；时长未知的曲目按 `0` 计。
   */
  duration?: number;
}

interface CreateParams {
  /**
   * Name of the new playlist.
   * @zh 新播放列表的名称。
   * @default "New Playlist"
   */
  name?: string;
  /**
   * Where to insert it in the playlist list; omitted, at the end.
   * @zh 插入到播放列表清单中的位置；省略时放在末尾。
   * @minimum 0
   */
  position?: Int;
}

interface CreateResult {
  /**
   * Index of the new playlist.
   * @zh 新播放列表的索引。
   */
  index: Int;
  guid: ReportedPlaylistGuid;
}

interface RemoveParams {
  /**
   * Index of the playlist to delete; omitted, the active playlist. An index past the last
   * playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails
   * with `NO_ACTIVE_ITEM`.
   * @zh 要删除的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface RenameParams {
  /**
   * Index of the playlist to rename. An index past the last playlist fails with `INVALID_INDEX`.
   * Give this or `playlistGuid`; with neither the call fails with `INVALID_PARAMS`.
   * @zh 要重命名的播放列表的索引。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败。给它或 `playlistGuid` 之一；两个都没给时以 `INVALID_PARAMS` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * New name.
   * @zh 新名称。
   */
  name: string;
}

interface ClearParams {
  /**
   * Index of the playlist to empty; omitted, the active playlist. An index past the last playlist
   * fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 要清空的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface ClearResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Number of tracks it held before.
   * @zh 清空前的曲目数。
   */
  clearedCount: Int;
  /**
   * Number of tracks left; `0` on success.
   * @zh 剩下的曲目数；成功时为 `0`。
   */
  remainingCount: Int;
}

interface DuplicateParams {
  /**
   * Index of the playlist to copy; omitted, the active playlist. An index past the last playlist
   * fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 要复制的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Name of the copy; omitted or empty, the original name followed by ` (Copy)`.
   * @zh 副本的名称；省略或为空时为原名称后加 ` (Copy)`。
   */
  name?: string;
}

interface DuplicateResult {
  /**
   * Index of the copy.
   * @zh 副本的索引。
   */
  index: Int;
  /**
   * Index of the playlist that was copied.
   * @zh 被复制的播放列表的索引。
   */
  sourcePlaylist: Int;
  /**
   * GUID of the playlist that was copied, as `playlistGuid` takes it.
   * @zh 被复制的播放列表的 GUID，写法与 `playlistGuid` 接受的一致。
   */
  sourcePlaylistGuid: ReportedPlaylistGuid;
  /**
   * Index of the copy; the same as `index`.
   * @zh 副本的索引；与 `index` 相同。
   */
  newPlaylist: Int;
  /**
   * GUID of the copy, as `playlistGuid` takes it.
   * @zh 副本的 GUID，写法与 `playlistGuid` 接受的一致。
   */
  guid: ReportedPlaylistGuid;
  /**
   * Name of the copy.
   * @zh 副本的名称。
   */
  name: string;
  /**
   * Number of tracks copied.
   * @zh 复制的曲目数。
   */
  trackCount: Int;
}

interface ReorderPlaylistsParams {
  /**
   * For each new position, the current index of the playlist that goes there; every playlist
   * exactly once.
   * @zh 每个新位置上放的那个播放列表的当前索引；每个播放列表恰好出现一次。
   * @minimum 0
   */
  newOrder?: Int[];
  /**
   * For each new position, the `guid` of the playlist that goes there, as `playlist.getAll`
   * reports it; every playlist exactly once.
   * @zh 每个新位置上放的那个播放列表的 `guid`（取 `playlist.getAll` 报告的值）；每个播放列表恰好出现一次。
   */
  newOrderGuids?: PlaylistGuid[];
}

interface ReorderPlaylistsResult {
  /**
   * Number of playlists.
   * @zh 播放列表个数。
   */
  count: Int;
}

interface UndoParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface RedoParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface IsAutoplaylistParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface IsAutoplaylistResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Whether it is an autoplaylist.
   * @zh 它是否为自动播放列表。
   */
  isAutoplaylist: boolean;
  /**
   * Name of the playlist's lock, present when the playlist carries a named lock and is not an
   * autoplaylist foobar2000 runs, whether or not the lock makes it count as one.
   * @zh 播放列表的锁的名字；播放列表带有具名的锁、且不是 foobar2000 运行的自动播放列表时出现，不论这把锁是否让它算作自动播放列表。
   */
  lockName?: string;
}

interface CreateAutoplaylistParams {
  /**
   * Name of the new playlist.
   * @zh 新播放列表的名称。
   * @default "New Autoplaylist"
   */
  name?: string;
  /**
   * Library query that selects the tracks, in foobar2000's query syntax.
   * @zh 选出曲目的媒体库查询，使用 foobar2000 的查询语法。
   * @minLength 1
   */
  query: string;
  /**
   * Title Formatting pattern to sort the results by; empty, library order.
   * @zh 结果的排序依据，Title Formatting 模式；为空时按媒体库顺序。
   * @default ""
   */
  sort?: string;
  /**
   * Keep the playlist sorted by `sort`: when set, the user cannot reorder its tracks.
   * @zh 让播放列表始终按 `sort` 排序：设置后用户不能手动调整曲目顺序。
   * @default false
   */
  keepSorted?: boolean;
}

interface CreateAutoplaylistResult {
  /**
   * Index of the new playlist.
   * @zh 新播放列表的索引。
   */
  index: Int;
  /**
   * Index of the new playlist; the same as `index`.
   * @zh 新播放列表的索引；与 `index` 相同。
   */
  playlist: Int;
  guid: ReportedPlaylistGuid;
  /**
   * Name of the new playlist.
   * @zh 新播放列表的名称。
   */
  name: string;
  /**
   * The query, as given.
   * @zh 查询语句，原样返回。
   */
  query: string;
}

interface ConvertToAutoplaylistParams {
  /**
   * Index of the playlist to convert; omitted, the active playlist. An index past the last
   * playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails
   * with `NO_ACTIVE_ITEM`.
   * @zh 要转换的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Library query that selects the tracks, in foobar2000's query syntax.
   * @zh 选出曲目的媒体库查询，使用 foobar2000 的查询语法。
   * @minLength 1
   */
  query: string;
  /**
   * Title Formatting pattern to sort the results by; empty, library order.
   * @zh 结果的排序依据，Title Formatting 模式；为空时按媒体库顺序。
   * @default ""
   */
  sort?: string;
  /**
   * Keep the playlist sorted by `sort`: when set, the user cannot reorder its tracks.
   * @zh 让播放列表始终按 `sort` 排序：设置后用户不能手动调整曲目顺序。
   * @default false
   */
  keepSorted?: boolean;
}

interface ConvertToAutoplaylistResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
}

interface RemoveAutoplaylistParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface RemoveAutoplaylistResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * `sdk` when foobar2000's autoplaylist was released, `dui` when the playlist is an
   * autoplaylist another component manages and nothing changed.
   * @zh foobar2000 的自动播放列表已解除时为 `sdk`；播放列表是由其他组件管理的自动播放列表、什么也没改时为 `dui`。
   */
  source: 'sdk' | 'dui';
  /**
   * Explanation, present when `source` is `dui`.
   * @zh 说明，`source` 为 `dui` 时出现。
   */
  note?: string;
}

interface GetAutoplaylistInfoParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetAutoplaylistInfoResult {
  /**
   * Whether the playlist is an autoplaylist; the remaining fields other than `playlist` are
   * present only when it is.
   * @zh 播放列表是否为自动播放列表；除 `playlist` 外的其余字段只在它是时出现。
   */
  isAutoplaylist: boolean;
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Whether the autoplaylist keeps its tracks sorted; always `false` when `source` is `dui`.
   * @zh 自动播放列表是否保持排序；`source` 为 `dui` 时总是 `false`。
   */
  keepSorted?: boolean;
  /**
   * `sdk` for an autoplaylist foobar2000 runs, `dui` for one another component manages.
   * @zh foobar2000 运行的自动播放列表为 `sdk`，由其他组件管理的为 `dui`。
   */
  source?: 'sdk' | 'dui';
  /**
   * Name of the playlist's lock, present when `source` is `dui`.
   * @zh 播放列表的锁的名字，`source` 为 `dui` 时出现。
   */
  lockName?: string;
}

interface GetAutoplaylistQueryParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetAutoplaylistQueryResult {
  /**
   * Whether the playlist is an autoplaylist; `keepSorted`, `source`, `note` and `lockName` are
   * present only when it is.
   * @zh 播放列表是否为自动播放列表；`keepSorted`、`source`、`note` 与 `lockName` 只在它是时出现。
   */
  isAutoplaylist: boolean;
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Always `null`: foobar2000 does not expose the query.
   * @zh 总是 `null`：foobar2000 不公开查询语句。
   */
  query: string | null;
  /**
   * Whether the autoplaylist keeps its tracks sorted; always `false` when `source` is `dui`.
   * @zh 自动播放列表是否保持排序；`source` 为 `dui` 时总是 `false`。
   */
  keepSorted?: boolean;
  /**
   * `sdk` for an autoplaylist foobar2000 runs, `dui` for one another component manages.
   * @zh foobar2000 运行的自动播放列表为 `sdk`，由其他组件管理的为 `dui`。
   */
  source?: 'sdk' | 'dui';
  /**
   * Says that the query is not available.
   * @zh 说明查询语句不可得。
   */
  note?: string;
  /**
   * Name of the playlist's lock, present when `source` is `dui`.
   * @zh 播放列表的锁的名字，`source` 为 `dui` 时出现。
   */
  lockName?: string;
}

interface GetLockInfoParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetLockInfoResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Whether it carries a lock.
   * @zh 它是否带锁。
   */
  isLocked: boolean;
}

interface IsLockedParams {
  /**
   * Index of the playlist; omitted, the active playlist. With this omitted and no active playlist
   * the call fails with `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface IsLockedResult {
  /**
   * Whether the playlist carries a lock.
   * @zh 播放列表是否带锁。
   */
  isLocked: boolean;
}

interface GetAvailableColumnsResult {
  /**
   * Every column, component by component.
   * @zh 全部列，按组件依次排列。
   */
  columns: PlaylistColumnDefinition[];
  /**
   * Number of entries in `columns`.
   * @zh `columns` 的条目数。
   */
  count: Int;
}

interface SortParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Title Formatting pattern to sort by.
   * @zh 排序依据，Title Formatting 模式。
   * @default "%title%"
   */
  pattern?: string;
  /**
   * Reverse the whole playlist after sorting. The reversal covers every track, the unselected
   * ones too, even with `selectedOnly`.
   * @zh 排序后把整个播放列表倒过来。倒序覆盖全部曲目，即使设了 `selectedOnly`，未选中的也一样。
   * @default false
   */
  descending?: boolean;
  /**
   * Sort only the selected tracks among themselves, keeping the others in place.
   * @zh 只在选中的曲目之间排序，其余曲目位置不变。
   * @default false
   */
  selectedOnly?: boolean;
}

interface ShuffleParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface ReverseParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface ReorderParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * For each new row, the current row of the track that goes there; every row exactly once.
   * @zh 每个新行上放的那首曲目的当前行号；每一行恰好出现一次。
   * @minimum 0
   */
  newOrder: Int[];
}

interface ReorderResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Number of tracks reordered.
   * @zh 重排的曲目数。
   */
  itemCount: Int;
}


/**
 * A playlist's GUID instead of its index, written as the results and events that name a playlist
 * report it, for a query that answers an empty result for a missing playlist: a GUID no
 * playlist has answers the same way. Either hex case is accepted. Giving both this and the
 * index, or a malformed GUID, fails with `INVALID_PARAMS`.
 * @zh 播放列表的 GUID，用来代替序号，写法与指明播放列表的结果、事件报出的一致；用于列表不存在时回空结果的查询，没有列表持有的 GUID 也照此回答。十六进制大小写均可。同时给了它和序号，或 GUID 格式不对，以 `INVALID_PARAMS` 失败。
 */
type QueriedPlaylistGuid = string;

interface GetTrackCountParams {
  /**
   * Index of the playlist; omitted, the active playlist.
   * @zh 播放列表的索引；省略时为活动播放列表。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: QueriedPlaylistGuid;
}

interface GetTrackCountResult {
  /**
   * Number of tracks.
   * @zh 曲目数。
   */
  count: Int;
}

interface GetSelectionParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetSelectionResult {
  /**
   * Selected rows, in playlist order.
   * @zh 选中的行，按播放列表顺序。
   */
  items: Int[];
  /**
   * Number of entries in `items`.
   * @zh `items` 的条目数。
   */
  count: Int;
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
}

interface SetSelectionParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Rows to select; an empty list selects nothing.
   * @zh 要选中的行；空列表表示一行都不选。
   * @minimum 0
   */
  indices: Int[];
  /**
   * Deselect every other row; `false` adds `indices` to the current selection.
   * @zh 取消选中其余所有行；为 `false` 时把 `indices` 加到当前选中里。
   * @default true
   */
  clearOthers?: boolean;
}

interface SelectAllParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface DeselectAllParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface RemoveTracksParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Rows to remove.
   * @zh 要移除的行。
   * @minimum 0
   */
  items: Int[];
}

interface RemoveSelectedTracksParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface MoveTracksParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Rows to move; they replace the current selection first. Omitted or empty, the current
   * selection moves.
   * @zh 要移动的行，先用它们替换当前选中。省略或为空时移动当前选中的行。
   * @minimum 0
   */
  items?: Int[];
  /**
   * How many positions to move: negative towards the top, positive towards the bottom.
   * @zh 移动的位置数：负数向上，正数向下。
   */
  delta: Int;
}

interface PlayTrackParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Row to play.
   * @zh 要播放的行。
   * @default 0
   * @minimum 0
   */
  index?: Int;
  /**
   * Start the playback after the current message has been handled instead of before the call
   * returns. The playlist is looked up again by its GUID when the playback starts: moved
   * meanwhile, the same playlist plays; removed meanwhile, or shorter than `index` by then,
   * nothing plays.
   * @zh 在当前消息处理完之后再开始播放，而不是在调用返回之前。开始播放时按 GUID 重新找这张列表：期间被挪动，照样播放这张列表；期间被删掉，或那时已不到 `index` 行，就什么也不播。
   * @default false
   */
  deferred?: boolean;
  /**
   * Mute before starting, so a page that sets the volume right afterwards leaves no audible gap.
   * The mute is not undone by this call.
   * @zh 开始播放前先静音，让紧接着设置音量的页面不留下能听见的间隙。本调用不会取消这次静音。
   * @default false
   */
  muted?: boolean;
}

interface FocusTrackParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Row to focus; omitted or negative, the playlist loses its focus.
   * @zh 要聚焦的行；省略或为负数时播放列表失去焦点。
   */
  index?: Int;
}

interface GetFocusTrackParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetFocusTrackResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Focused row; `-1` when the playlist has no focus.
   * @zh 焦点行；播放列表没有焦点时为 `-1`。
   */
  index: Int;
}

interface GetFocusedTrackParams {
  /**
   * Index of the playlist; omitted, the active playlist.
   * @zh 播放列表的索引；省略时为活动播放列表。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: QueriedPlaylistGuid;
}

interface GetFocusedTrackResult {
  /**
   * Index of the playlist; absent when there is no such playlist.
   * @zh 播放列表的索引；没有这个播放列表时不出现。
   */
  playlist?: Int;
  /**
   * GUID of the playlist, as `playlistGuid` takes it; absent when there is no such playlist.
   * @zh 播放列表的 GUID，写法与 `playlistGuid` 接受的一致；没有这个播放列表时不出现。
   */
  playlistGuid?: ReportedPlaylistGuid;
  /**
   * Focused row; `-1` when the playlist has no focus or does not exist.
   * @zh 焦点行；播放列表没有焦点或不存在时为 `-1`。
   */
  index: Int;
}

interface SetFocusedTrackParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Row to focus; omitted or negative, the playlist loses its focus.
   * @zh 要聚焦的行；省略或为负数时播放列表失去焦点。
   */
  index?: Int;
}

interface InsertTracksParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Row before which the tracks go; past the last row, they are appended.
   * @zh 曲目插在这一行之前；超出最后一行时追加到末尾。
   * @default 0
   * @minimum 0
   */
  position?: Int;
  /**
   * Tracks to insert: each a path with an optional `|subsong:N` suffix, or `{ path, subsong }`.
   * @zh 要插入的曲目：每项是路径（可带 `|subsong:N` 后缀）或 `{ path, subsong }`。
   * @minItems 1
   * @security MediaRead
   * @pathKey path
   */
  handles: Json[];
}

interface InsertTracksResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * The `position` that was asked for, before it was limited to the end of the playlist.
   * @zh 请求的 `position`，未按列表末尾截短。
   */
  insertIndex: Int;
  /**
   * Number of entries in `handles`.
   * @zh `handles` 的条目数。
   */
  requestedCount: Int;
  /**
   * Tracks inserted.
   * @zh 插入的曲目数。
   */
  addedCount: Int;
  /**
   * Entries that were not usable.
   * @zh 不可用的条目数。
   */
  invalidCount: Int;
  /**
   * Number of tracks before the insertion.
   * @zh 插入前的曲目数。
   */
  countBefore: Int;
  /**
   * Number of tracks after the insertion.
   * @zh 插入后的曲目数。
   */
  totalCount: Int;
}

interface AddPathsParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Files, folders or URLs; a `|subsong:N` suffix selects a subsong.
   * @zh 文件、文件夹或 URL；`|subsong:N` 后缀选子曲目。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
}

interface AddPathsResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Number of entries in `paths`.
   * @zh `paths` 的条目数。
   */
  requestedPaths: Int;
  /**
   * Tracks added; a folder counts every track it holds.
   * @zh 加入的曲目数；文件夹按其中的每首曲目计。
   */
  addedCount: Int;
  /**
   * Entries that were not usable.
   * @zh 不可用的条目数。
   */
  invalidCount: Int;
  /**
   * Number of tracks before the addition.
   * @zh 加入前的曲目数。
   */
  countBefore: Int;
  /**
   * Number of tracks after the addition.
   * @zh 加入后的曲目数。
   */
  totalCount: Int;
}

interface AddHandlesParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Tracks to append: each a path with an optional `|subsong:N` suffix, or `{ path, subsong }`.
   * @zh 要追加的曲目：每项是路径（可带 `|subsong:N` 后缀）或 `{ path, subsong }`。
   * @minItems 1
   * @security MediaRead
   * @pathKey path
   */
  handles: Json[];
}

interface AddHandlesResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Number of entries in `handles`.
   * @zh `handles` 的条目数。
   */
  requestedCount: Int;
  /**
   * Tracks appended.
   * @zh 追加的曲目数。
   */
  addedCount: Int;
  /**
   * Entries that were not usable.
   * @zh 不可用的条目数。
   */
  invalidCount: Int;
  /**
   * Number of tracks before the addition.
   * @zh 追加前的曲目数。
   */
  countBefore: Int;
  /**
   * Number of tracks after the addition.
   * @zh 追加后的曲目数。
   */
  totalCount: Int;
}

interface AddPathsSequentialParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Files, folders or URLs, in the order to add them; a `|subsong:N` suffix selects a subsong.
   * @zh 文件、文件夹或 URL，按要加入的顺序；`|subsong:N` 后缀选子曲目。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
}

interface AddPathsSequentialResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Tracks added; `0` when nothing resolved, which still succeeds.
   * @zh 加入的曲目数；什么都没解析出来时为 `0`，调用仍然成功。
   */
  addedCount: Int;
  /**
   * Row of each added track, in the order they were added.
   * @zh 每首加入的曲目所在的行，按加入的顺序。
   */
  order: Int[];
}

interface AddPathsAsyncParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * Files, folders, URLs or playlist files; a `|subsong:N` suffix selects a subsong.
   * @zh 文件、文件夹、URL 或播放列表文件；`|subsong:N` 后缀选子曲目。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
}

interface AddPathsAsyncResult {
  /**
   * Id of the operation; `playlist:addComplete` carries the same id.
   * @zh 操作的 id；`playlist:addComplete` 带同一个 id。
   */
  operationId: string;
  /**
   * GUID of the playlist the paths go to; `playlist:addComplete` carries the same one.
   * @zh 路径要加入的播放列表的 GUID；`playlist:addComplete` 带同一个。
   */
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Always `pending`.
   * @zh 总是 `pending`。
   */
  status: 'pending';
  /**
   * Entries that were accepted for processing.
   * @zh 接受处理的条目数。
   */
  totalCount: Int;
  /**
   * Entries refused before processing: empty or longer than 2048 characters.
   * @zh 处理前就拒绝的条目数：为空或超过 2048 个字符。
   */
  invalidCount: Int;
}

interface ReplaceAllAndPlayParams {
  /**
   * Index of the playlist; omitted, the active playlist. An index past the last playlist fails
   * with `INVALID_INDEX`; with this omitted and no active playlist the call fails with
   * `NO_ACTIVE_ITEM`.
   * @zh 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
  /**
   * The new content: files, folders or URLs.
   * @zh 新的内容：文件、文件夹或 URL。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * Row to play, or to focus with `autoPlay` `false`; past the last row, the first one. The rows
   * do not keep the order of `paths`, so this names a position, not one of the given paths.
   * @zh 要播放的行，`autoPlay` 为 `false` 时是要聚焦的行；超出最后一行时取第一行。各行不保持 `paths` 的顺序，所以它指的是位置，而不是传入的某个路径。
   * @default 0
   * @minimum 0
   */
  playIndex?: Int;
  /**
   * Stop playback first, when something is playing.
   * @zh 正在播放时先停止播放。
   * @default true
   */
  stopFirst?: boolean;
  /**
   * Play `playIndex`; `false` only focuses it.
   * @zh 播放 `playIndex`；为 `false` 时只聚焦它。
   * @default true
   */
  autoPlay?: boolean;
}

interface ReplaceAllAndPlayResult {
  /**
   * Index of the playlist.
   * @zh 播放列表的索引。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Number of tracks removed.
   * @zh 移除的曲目数。
   */
  clearedCount: Int;
  /**
   * Tracks added.
   * @zh 加入的曲目数。
   */
  addedCount: Int;
  /**
   * Number of tracks afterwards.
   * @zh 之后的曲目数。
   */
  totalCount: Int;
  /**
   * Row that was played or focused.
   * @zh 播放或聚焦的行。
   */
  playIndex: Int;
}

// ---- getTracks ----

interface GetTracksParams {
  /**
   * Playlist index; omitted, the active playlist.
   * @zh 播放列表索引；省略时为活动播放列表。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: QueriedPlaylistGuid;
  /**
   * First row to return.
   * @zh 返回的第一行。
   * @default 0
   * @minimum 0
   */
  start?: Int;
  /**
   * Most rows to return.
   * @zh 最多返回的行数。
   * @default 100
   * @minimum 0
   */
  count?: Int;
  /**
   * Extra columns, each a Title Formatting pattern under a name of your choosing; every row
   * carries their values under `formats`, in projected rows too.
   * @zh 额外的列，每列是一个 Title Formatting 模式，列名自定；每行（包括投影的行）在 `formats` 下带上它们的值。
   */
  formats?: Record<string, string>;
  /**
   * Keys each row carries besides `index`; omitted, the whole row. The names are those
   * `library.query` takes; an unknown name fails with `INVALID_PARAMS` and is listed in
   * `details.unknownFields`.
   * @zh 每行除 `index` 外带的键；省略时为整行。可用的名字与 `library.query` 相同；未知的名字以 `INVALID_PARAMS` 失败，并列在 `details.unknownFields` 里。
   * @minItems 1
   */
  fields?: string[];
}

interface GetTracksResult {
  /**
   * The playlist read, as given or resolved. `-1` when there is none to name: `playlist` was
   * omitted and there is no active playlist, or no playlist has the `playlistGuid`. An index past
   * the last playlist comes back as given.
   * @zh 读取的播放列表（给定的或解析出的）。没有可指的列表时为 `-1`：省略 `playlist` 且没有活动播放列表，或者没有列表持有 `playlistGuid`。超出最后一个播放列表的索引原样返回。
   */
  playlist: Int;
  /**
   * GUID of the playlist read, as `playlistGuid` takes it; absent when the playlist does not
   * exist, so its absence is what tells a missing playlist from an empty one.
   * @zh 读取的播放列表的 GUID，写法与 `playlistGuid` 接受的一致；列表不存在时不出现，凭它缺席就能把「列表不存在」与「列表为空」分开。
   */
  playlistGuid?: ReportedPlaylistGuid;
  /**
   * The `start` applied.
   * @zh 生效的 `start`。
   */
  start: Int;
  /**
   * Rows returned.
   * @zh 返回的行数。
   */
  count: Int;
  /**
   * Rows in the playlist; `0` when it does not exist.
   * @zh 播放列表的行数；列表不存在时为 `0`。
   */
  total: Int;
  /**
   * The rows from `start`: whole rows, or `index` plus the `fields` asked for.
   * @zh 从 `start` 起的行：整行，或 `index` 加上 `fields` 要求的键。
   */
  tracks: Partial<PlaylistTrack>[];
}

// ---- getSelectedTracks ----

interface GetSelectedTracksParams {
  /**
   * Playlist index; omitted, the active playlist.
   * @zh 播放列表索引；省略时为活动播放列表。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetSelectedTracksResult {
  /**
   * The playlist read.
   * @zh 读取的播放列表。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * The selected rows in playlist order.
   * @zh 选中的行，按列表顺序。
   */
  tracks: PlaylistTrack[];
  /**
   * Rows returned.
   * @zh 返回的行数。
   */
  count: Int;
}

// ---- getGroupRuns ----

interface GetGroupRunsParams {
  /**
   * One or two Title Formatting patterns; the second sub-groups each run.
   * @zh 一个或两个 Title Formatting 模式；第二个在每个游程内再分组。
   * @minItems 1
   */
  patterns: string[];
  /**
   * Playlist index; omitted, the active playlist.
   * @zh 播放列表索引；省略时为活动播放列表。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetGroupRunsResult {
  /**
   * The playlist grouped.
   * @zh 分组的播放列表。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Rows in the playlist; the runs' `count` values add up to it.
   * @zh 播放列表的行数；各游程的 `count` 相加等于它。
   */
  total: Int;
  /**
   * The runs in playlist order, the first starting at row `0`; empty for an empty playlist.
   * @zh 各游程，按列表顺序，第一个从第 `0` 行开始；空列表时为空。
   */
  runs: PlaylistGroupRun[];
}

// ---- getMatchingRows ----

interface GetMatchingRowsParams {
  /**
   * foobar2000 query, as in the Media Library search; `SORT BY` is refused.
   * @zh foobar2000 查询，写法同媒体库搜索；不收 `SORT BY`。
   * @minLength 1
   */
  query: string;
  /**
   * Playlist index; omitted, the active playlist.
   * @zh 播放列表索引；省略时为活动播放列表。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: PlaylistGuid;
}

interface GetMatchingRowsResult {
  /**
   * The playlist searched.
   * @zh 搜索的播放列表。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Rows in the playlist when the query ran. Equal to the `total` of a later
   * `playlist.getTracks` does not mean the playlist is unchanged: reordering and tag edits keep
   * the count.
   * @zh 查询时播放列表的行数。与之后 `playlist.getTracks` 的 `total` 相等不代表列表没变：重排与改标签都不改变行数。
   */
  total: Int;
  /**
   * The matching rows, ascending.
   * @zh 命中的行，升序。
   */
  items: Int[];
  /**
   * Number of entries in `items`.
   * @zh `items` 的条目数。
   */
  count: Int;
}

// ---- getTracksAt ----

interface GetTracksAtParams {
  /**
   * Rows to read, in the order to return them.
   * @zh 要读的行，按返回的顺序。
   * @minimum 0
   */
  rows: Int[];
  /**
   * Playlist index; omitted, the active playlist.
   * @zh 播放列表索引；省略时为活动播放列表。
   * @minimum 0
   */
  playlist?: Int;
  playlistGuid?: QueriedPlaylistGuid;
  /**
   * Extra columns, as in `playlist.getTracks`.
   * @zh 额外的列，同 `playlist.getTracks`。
   */
  formats?: Record<string, string>;
  /**
   * Keys each row carries besides `index`, as in `playlist.getTracks`; omitted, the whole row.
   * @zh 每行除 `index` 外带的键，同 `playlist.getTracks`；省略时为整行。
   * @minItems 1
   */
  fields?: string[];
}

interface GetTracksAtResult {
  /**
   * The playlist read, as `playlist` in the result of `playlist.getTracks`.
   * @zh 读取的播放列表，同 `playlist.getTracks` 结果里的 `playlist`。
   */
  playlist: Int;
  /**
   * GUID of the playlist read; absent when it does not exist, as in `playlist.getTracks`.
   * @zh 读取的播放列表的 GUID；列表不存在时不出现，同 `playlist.getTracks`。
   */
  playlistGuid?: ReportedPlaylistGuid;
  /**
   * Rows in the playlist; `0` when it does not exist.
   * @zh 播放列表的行数；列表不存在时为 `0`。
   */
  total: Int;
  /**
   * Rows returned.
   * @zh 返回的行数。
   */
  count: Int;
  /**
   * The rows in the order asked for, skipping those past the last row.
   * @zh 按请求顺序排列的行，超出最后一行的已跳过。
   */
  tracks: Partial<PlaylistTrack>[];
}

// ---- events ----

interface ItemsAddedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Row of the first inserted track.
   * @zh 第一条插入曲目所在的行。
   */
  start: Int;
  /**
   * Number of tracks inserted.
   * @zh 插入的曲目数。
   */
  count: Int;
}

interface ItemsRemovedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Track count before the removal.
   * @zh 移除前的曲目数。
   */
  oldCount: Int;
  /**
   * Track count after the removal.
   * @zh 移除后的曲目数。
   */
  newCount: Int;
}

interface ItemsReorderedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Track count of the playlist.
   * @zh 播放列表的曲目数。
   */
  count: Int;
}

interface SelectionChangedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
}

interface FocusChangedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Row focused before; `-1` when none was.
   * @zh 之前的焦点行；没有时为 `-1`。
   */
  from: Int;
  /**
   * Row focused now; `-1` when none is.
   * @zh 现在的焦点行；没有时为 `-1`。
   */
  to: Int;
}

interface ItemsReplacedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Number of tracks replaced.
   * @zh 被替换的曲目数。
   */
  count: Int;
}

interface CreatedPayload {
  /**
   * Index of the new playlist.
   * @zh 新播放列表的序号。
   */
  index: Int;
  guid: ReportedPlaylistGuid;
  /**
   * Its name.
   * @zh 它的名字。
   */
  name: string;
}

interface RemovedPayload {
  /**
   * Playlist count before the removal.
   * @zh 移除前的播放列表数。
   */
  oldCount: Int;
  /**
   * Playlist count after the removal.
   * @zh 移除后的播放列表数。
   */
  newCount: Int;
  /**
   * Indices the removed playlists had before the removal, ascending.
   * @zh 被移除的播放列表在移除之前的序号，升序。
   */
  indices: Int[];
  /**
   * GUIDs of the removed playlists, in the order of `indices`; empty when foobar2000 reported the
   * removal without announcing it first, so that the GUIDs could no longer be read.
   * @zh 被移除的播放列表的 GUID，与 `indices` 同序；foobar2000 事先没有预告就报告移除、GUID 已无从读取时为空数组。
   */
  guids: ReportedPlaylistGuid[];
}

interface ReorderedPayload {
  /**
   * Number of playlists.
   * @zh 播放列表数。
   */
  count: Int;
  /**
   * GUID of every playlist, in the new order.
   * @zh 全部播放列表的 GUID，按新的顺序。
   */
  guids: ReportedPlaylistGuid[];
}

interface ActivatedPayload {
  /**
   * Index of the playlist active before; `-1` when none was.
   * @zh 之前的活动播放列表序号；没有时为 `-1`。
   */
  oldIndex: Int;
  /**
   * Index of the playlist active now; `-1` when none is.
   * @zh 现在的活动播放列表序号；没有时为 `-1`。
   */
  newIndex: Int;
  /**
   * GUID of the playlist active now; `null` when none is. No GUID is given for the playlist active
   * before: it may be the one just removed.
   * @zh 现在的活动播放列表的 GUID；没有时为 `null`。之前的活动播放列表不给 GUID：它可能正是刚被移除的那张。
   */
  newGuid: ReportedPlaylistGuid | null;
}

interface RenamedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  index: Int;
  guid: ReportedPlaylistGuid;
  /**
   * The new name.
   * @zh 新名字。
   */
  name: string;
}

interface LockChangedPayload {
  /**
   * Index of the playlist.
   * @zh 播放列表的序号。
   */
  playlist: Int;
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Whether a lock is on it now.
   * @zh 现在是否有锁。
   */
  locked: boolean;
}

interface AddCompletePayload {
  /**
   * Id from the call's answer.
   * @zh 调用应答里的 id。
   */
  operationId: string;
  /**
   * GUID of the playlist the call added to, as the call resolved it; still that playlist when it
   * was moved meanwhile, and no longer anyone's when it was removed.
   * @zh 调用加入的那张播放列表的 GUID，取调用解析出的那张；期间被挪动仍指向它，期间被删掉就不再属于任何列表。
   */
  playlistGuid: ReportedPlaylistGuid;
  /**
   * Always `true`.
   * @zh 恒为 `true`。
   */
  success: boolean;
  /**
   * Tracks added, after expanding playlists and cue sheets. Tracks dropped because the playlist
   * was removed or locked before the expansion finished are not counted.
   * @zh 展开播放列表与 cue 之后加入的曲目数。展开完成前列表被删掉或上锁而丢弃的曲目不计入。
   */
  addedCount: Int;
  /**
   * Paths the call accepted.
   * @zh 调用受理的路径数。
   */
  totalCount: Int;
}

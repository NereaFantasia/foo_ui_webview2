#pragma once
// Fb2kPlaybackService.h - Production implementation of IPlaybackService
// Delegates to foobar2000 SDK playback_control singleton.

#include "interfaces/IPlaybackService.h"
#include "api/PlaybackApi.h"  // for GetTrackInfo
#include "utils/SubsongUtils.h"  // for ParseSubsongPath

class Fb2kPlaybackService : public IPlaybackService {
public:
    void play_or_unpause() override {
        playback_control::get()->play_or_unpause();
    }

    void pause(bool paused) override {
        playback_control::get()->pause(paused);
    }

    void stop() override {
        playback_control::get()->stop();
    }

    // Transport extensions
    void play_or_pause() override {
        playback_control::get()->play_or_pause();
    }

    void next() override {
        playback_control::get()->next();
    }

    void previous() override {
        playback_control::get()->previous();
    }

    void start(unsigned command, bool paused) override {
        playback_control::get()->start(
            static_cast<playback_control::t_track_command>(command), paused);
    }

    bool is_playing() const override {
        return playback_control::get()->is_playing();
    }

    bool is_paused() const override {
        return playback_control::get()->is_paused();
    }

    bool playback_can_seek() const override {
        return playback_control::get()->playback_can_seek();
    }

    float get_volume() const override {
        return playback_control::get()->get_volume();
    }

    bool is_muted() const override {
        return playback_control::get()->is_muted();
    }

    // Volume control
    void set_volume(float db) override {
        playback_control::get()->set_volume(db);
    }

    void volume_up() override {
        playback_control::get()->volume_up();
    }

    void volume_down() override {
        playback_control::get()->volume_down();
    }

    void volume_mute_toggle() override {
        playback_control::get()->volume_mute_toggle();
    }

    // Stop after current
    bool get_stop_after_current() const override {
        return playback_control::get()->get_stop_after_current();
    }

    void set_stop_after_current(bool enabled) override {
        playback_control::get()->set_stop_after_current(enabled);
    }

    void toggle_stop_after_current() override {
        playback_control::get()->toggle_stop_after_current();
    }

    // Playback order (cross-module: delegates to playlist_manager)
    int playback_order_get_active() const override {
        return static_cast<int>(playlist_manager::get()->playback_order_get_active());
    }

    void playback_order_set_active(int order) override {
        playlist_manager::get()->playback_order_set_active(static_cast<t_size>(order));
    }

    // Position/seek
    double playback_get_position() const override {
        return playback_control::get()->playback_get_position();
    }

    void playback_seek(double seconds) override {
        playback_control::get()->playback_seek(seconds);
    }

    NowPlayingInfo get_now_playing_info() const override {
        NowPlayingInfo info;
        metadb_handle_ptr track;
        if (playback_control::get()->get_now_playing(track) && track.is_valid()) {
            info.found = true;
            info.duration = track->get_length();
            info.subsong = track->get_subsong_index();
            info.path = track->get_path();
        }
        return info;
    }

    // Current track (full metadata)
    nlohmann::json get_current_track_json() const override {
        metadb_handle_ptr track;
        if (!playback_control::get()->get_now_playing(track) || !track.is_valid())
            return nullptr;
        return GetTrackInfo(track);
    }

    // Playing item location
    PlayingItemLocation get_playing_item_location() const override {
        PlayingItemLocation loc;
        size_t playlist = pfc::infinite_size;
        size_t index = pfc::infinite_size;
        loc.found = playlist_manager::get()->get_playing_item_location(&playlist, &index);
        if (loc.found) {
            loc.playlist = playlist;
            loc.index = index;
        }
        return loc;
    }

    nlohmann::json get_track_info_at(size_t playlist, size_t index) const override {
        metadb_handle_ptr handle;
        if (playlist_manager::get()->playlist_get_item_handle(handle, playlist, index))
            return GetTrackInfo(handle);
        return nullptr;
    }

    // Playing playlist info
    size_t get_playing_playlist() const override {
        return playlist_manager::get()->get_playing_playlist();
    }

    std::string get_playlist_name(size_t playlist) const override {
        pfc::string8 name;
        playlist_manager::get()->playlist_get_name(playlist, name);
        return name.c_str();
    }

    // Direct path playback — migrated from PlaybackPlayPath handler
    PathPlayResult play_single_path(const std::string& filePath, int subsongIndex, bool subsongRequested) override {
        PathPlayResult result;

        try {
            pfc::string_list_impl pathList;
            pathList.add_item(filePath.c_str());

            auto piif = playlist_incoming_item_filter::get();
            metadb_handle_list handles;
            piif->process_locations(pathList, handles, true, nullptr, nullptr, core_api::get_main_window());

            if (handles.get_count() == 0) {
                result.error = "Could not resolve path to playable content";
                return result;
            }

            // Subsong resolution
            metadb_handle_ptr handle;
            if (!subsongRequested) {
                handle = handles[0];
            } else {
                // Look for exact subsong match in resolved handles
                if (handles.get_count() > 1) {
                    for (size_t i = 0; i < handles.get_count(); i++) {
                        if (static_cast<int>(handles[i]->get_subsong_index()) == subsongIndex) {
                            handle = handles[i];
                            break;
                        }
                    }
                    if (!handle.is_valid() && subsongIndex < static_cast<int>(handles.get_count())) {
                        handle = handles[subsongIndex];
                    }
                    if (!handle.is_valid()) {
                        handle = handles[0];
                    }
                } else {
                    handle = handles[0];
                }

                // Fallback: explicit handle_create if subsong doesn't match
                if (!handle.is_valid() || static_cast<int>(handle->get_subsong_index()) != subsongIndex) {
                    auto mm = metadb::get();
                    metadb_handle_ptr explicitHandle = mm->handle_create(filePath.c_str(), subsongIndex);
                    if (explicitHandle.is_valid()) {
                        handle = explicitHandle;
                    }
                }
                if (!handle.is_valid()) {
                    handle = handles[0];
                }
            }

            // Ensure active playlist, insert and play
            auto pm = playlist_manager::get();
            t_size playlist = pm->get_active_playlist();
            if (playlist == pfc::infinite_size) {
                playlist = pm->create_playlist_autoname(0);
                pm->set_active_playlist(playlist);
            }

            metadb_handle_list playHandles;
            playHandles.add_item(handle);
            pm->playlist_undo_backup(playlist);
            t_size base = pm->playlist_get_item_count(playlist);
            pm->playlist_insert_items(playlist, base, playHandles, pfc::bit_array_true());
            pm->playlist_execute_default_action(playlist, base);

            result.success = true;
            result.tracksAdded = 1;
            result.resolvedSubsong = static_cast<int>(handle->get_subsong_index());
        } catch (const std::exception& e) {
            result.error = e.what();
        } catch (...) {
            result.error = "Unknown error";
        }

        return result;
    }

    // Multiple path playback — migrated from PlaybackPlayPaths handler
    MultiPathPlayResult play_multiple_paths(const std::vector<std::string>& paths, size_t startIndex, bool replace = false) override {
        MultiPathPlayResult result;

        try {
            auto mm = metadb::get();
            auto pm = playlist_manager::get();
            metadb_handle_list allHandles;

            for (const auto& path : paths) {
                auto [filePath, subsongIndex] = SubsongUtils::ParseSubsongPath(path);
                pfc::string8 pathStr(filePath.c_str());
                metadb_handle_ptr handle = mm->handle_create(pathStr.get_ptr(), subsongIndex);
                if (handle.is_valid()) {
                    allHandles.add_item(handle);
                }
            }

            if (allHandles.get_count() == 0) {
                result.error = "No playable content found";
                return result;
            }

            t_size playlist = pm->get_active_playlist();
            if (playlist == pfc::infinite_size) {
                playlist = pm->create_playlist_autoname(0);
                pm->set_active_playlist(playlist);
            }

            pm->playlist_undo_backup(playlist);
            t_size base;
            if (replace) {
                pm->playlist_clear(playlist);
                base = 0;
            } else {
                base = pm->playlist_get_item_count(playlist);
            }
            pm->playlist_insert_items(playlist, base, allHandles, pfc::bit_array_true());

            if (startIndex < allHandles.get_count()) {
                pm->playlist_execute_default_action(playlist, base + startIndex);
            } else {
                pm->playlist_execute_default_action(playlist, base);
            }

            result.success = true;
            result.tracksAdded = allHandles.get_count();
            result.startedAt = startIndex;
        } catch (const std::exception& e) {
            result.error = e.what();
        } catch (...) {
            result.error = "Unknown error";
        }

        return result;
    }
};

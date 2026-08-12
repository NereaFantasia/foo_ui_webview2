// test_menu_handle_list_dedup.cpp - MenuApi::ParseHandleList path-verdict dedup tests
//
// Covers the per-call verdict cache added to MenuApi::ParseHandleList, which
// collapses the redundant PathSecurity::ValidateMediaAccess calls produced when
// several subsongs of one container arrive in the same batch.
//
// Both input shapes strip the subsong before validation: the object form
// validates h["path"], and the string form cuts "|subsong:N" off the tail. So
// N subsongs of one CUE reach ValidateMediaAccess as N identical bare paths.
// handle_create is *not* deduped -- every entry still gets its own handle.
//
// The real PathSecurity singleton constructor depends on core_api (profile /
// install paths) and metadb::get() is unavailable here, so following the
// convention of test_path_security_unc_bypass.cpp the loop body is
// reimplemented with injectable fakes that count their invocations.
#include "pch.h"
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace {

// Stand-in for PathSecurity::Instance().ValidateMediaAccess. Records every
// path it is asked about so both the count and the exact strings can be pinned.
struct ValidatorFake {
    std::vector<std::string> denied;
    std::vector<std::string> seen;
    int calls = 0;

    bool operator()(const std::string& path) {
        ++calls;
        seen.push_back(path);
        for (const auto& d : denied) {
            if (d == path) return false;
        }
        return true;
    }
};

// Stand-in for metadb::get()->handle_create. `invalid` models handles the real
// metadb rejects, which ParseHandleList drops without adding to the output.
struct HandleCreateFake {
    std::vector<std::pair<std::string, uint32_t>> calls;
    std::vector<std::string> invalid;

    bool operator()(const std::string& path, uint32_t subsong) {
        calls.emplace_back(path, subsong);
        for (const auto& i : invalid) {
            if (i == path) return false;
        }
        return true;
    }
};

// Reimpl of the ParseHandleList loop. Returns the real function's
// `out.get_count() > 0`; `added` mirrors the output list size.
bool ParseHandleListReimpl(const json& handlesJson,
                           ValidatorFake& validate,
                           HandleCreateFake& handleCreate,
                           int& added) {
    added = 0;
    if (!handlesJson.is_array()) return false;

    std::unordered_map<std::string, bool> pathVerdicts;

    for (const auto& h : handlesJson) {
        std::string path;
        uint32_t subsong = 0;

        if (h.is_object()) {
            path = h.value("path", "");
            subsong = h.value("subsong", 0);
        } else if (h.is_string()) {
            path = h.get<std::string>();
            auto pos = path.find("|subsong:");
            if (pos != std::string::npos) {
                try {
                    subsong = static_cast<uint32_t>(std::stoul(path.substr(pos + 9)));
                } catch (...) {
                    subsong = 0;
                }
                // Outside the catch: a malformed suffix is still stripped.
                path = path.substr(0, pos);
            }
        } else {
            continue;
        }

        if (path.empty()) continue;

        auto verdict = pathVerdicts.find(path);
        if (verdict == pathVerdicts.end()) {
            verdict = pathVerdicts.emplace(path, validate(path)).first;
        }
        if (!verdict->second) continue;

        if (handleCreate(path, subsong)) {
            ++added;
        }
    }

    return added > 0;
}

} // namespace

// ============================================
// Core dedup — the performance win
// ============================================

TEST(MenuHandleListDedup, FiveSubsongsOfOnePathValidateOnce) {
    json handles = json::array();
    for (int i = 0; i < 5; ++i) {
        handles.push_back("x.flac|subsong:" + std::to_string(i));
    }

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 1);
    EXPECT_EQ(validate.seen.front(), "x.flac");
    ASSERT_EQ(handleCreate.calls.size(), 5u);
    EXPECT_EQ(added, 5);
    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(handleCreate.calls[i].first, "x.flac");
        EXPECT_EQ(handleCreate.calls[i].second, i);
    }
}

TEST(MenuHandleListDedup, ObjectFormDedupesToo) {
    json handles = json::array({
        json{{"path", "a.flac"}, {"subsong", 1}},
        json{{"path", "a.flac"}, {"subsong", 2}},
    });

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 1);
    ASSERT_EQ(handleCreate.calls.size(), 2u);
    EXPECT_EQ(handleCreate.calls[0].second, 1u);
    EXPECT_EQ(handleCreate.calls[1].second, 2u);
}

TEST(MenuHandleListDedup, DistinctPathsEachValidate) {
    json handles = json::array({"a.flac", "b.flac", "c.flac"});

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 3);
    EXPECT_EQ(handleCreate.calls.size(), 3u);
}

TEST(MenuHandleListDedup, MixedRepeatsValidateOncePerDistinctPath) {
    json handles = json::array({
        "a.flac|subsong:0", "b.flac|subsong:0",
        "a.flac|subsong:1", "b.flac|subsong:1",
        "a.flac|subsong:2",
    });

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 2);
    EXPECT_EQ(handleCreate.calls.size(), 5u);
}

// ============================================
// Negative verdicts are cached as well
// ============================================

TEST(MenuHandleListDedup, DeniedPathValidatesOnceAndCreatesNothing) {
    json handles = json::array();
    for (int i = 0; i < 4; ++i) {
        handles.push_back("blocked.flac|subsong:" + std::to_string(i));
    }

    ValidatorFake validate;
    validate.denied.push_back("blocked.flac");
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_FALSE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 1);
    EXPECT_TRUE(handleCreate.calls.empty());
    EXPECT_EQ(added, 0);
}

TEST(MenuHandleListDedup, AllowedAndDeniedPathsMixed) {
    json handles = json::array({
        "A.flac|subsong:0", "B.flac|subsong:0",
        "A.flac|subsong:1", "B.flac|subsong:1",
        "A.flac|subsong:2",
    });

    ValidatorFake validate;
    validate.denied.push_back("B.flac");
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 2);
    ASSERT_EQ(handleCreate.calls.size(), 3u);
    EXPECT_EQ(added, 3);
    for (const auto& call : handleCreate.calls) {
        EXPECT_EQ(call.first, "A.flac");
    }
}

// ============================================
// Subsong stripping feeds the cache key
// ============================================

TEST(MenuHandleListDedup, SubsongSuffixStrippedBeforeValidation) {
    json handles = json::array({"x.flac|subsong:12"});

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    ASSERT_EQ(validate.seen.size(), 1u);
    EXPECT_EQ(validate.seen[0], "x.flac");
    ASSERT_EQ(handleCreate.calls.size(), 1u);
    EXPECT_EQ(handleCreate.calls[0].first, "x.flac");
    EXPECT_EQ(handleCreate.calls[0].second, 12u);
}

TEST(MenuHandleListDedup, PlainStringHasSubsongZero) {
    json handles = json::array({"plain.flac"});

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.seen[0], "plain.flac");
    ASSERT_EQ(handleCreate.calls.size(), 1u);
    EXPECT_EQ(handleCreate.calls[0].second, 0u);
}

TEST(MenuHandleListDedup, MalformedSubsongStillStripsSuffix) {
    json handles = json::array({"x.flac|subsong:abc"});

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    ASSERT_EQ(validate.seen.size(), 1u);
    EXPECT_EQ(validate.seen[0], "x.flac");
    ASSERT_EQ(handleCreate.calls.size(), 1u);
    EXPECT_EQ(handleCreate.calls[0].first, "x.flac");
    EXPECT_EQ(handleCreate.calls[0].second, 0u);
}

TEST(MenuHandleListDedup, MalformedSubsongSharesCacheEntryWithValidOne) {
    json handles = json::array({"x.flac|subsong:abc", "x.flac|subsong:3"});

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 1);
    ASSERT_EQ(handleCreate.calls.size(), 2u);
    EXPECT_EQ(handleCreate.calls[0].second, 0u);
    EXPECT_EQ(handleCreate.calls[1].second, 3u);
}

// ============================================
// Boundaries — skipped entries never reach the validator
// ============================================

TEST(MenuHandleListDedup, EmptyArrayReturnsFalse) {
    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_FALSE(ParseHandleListReimpl(json::array(), validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 0);
    EXPECT_TRUE(handleCreate.calls.empty());
}

TEST(MenuHandleListDedup, NonArrayReturnsFalse) {
    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_FALSE(ParseHandleListReimpl(json::object(), validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 0);
}

TEST(MenuHandleListDedup, EmptyPathEntriesSkippedWithoutValidation) {
    json handles = json::array({
        "",
        json{{"path", ""}, {"subsong", 4}},
        json::object(),
    });

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_FALSE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 0);
    EXPECT_TRUE(handleCreate.calls.empty());
}

TEST(MenuHandleListDedup, NonObjectNonStringEntriesSkipped) {
    json handles = json::array({42, nullptr, true, json::array({"nested"}), 1.5});

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_FALSE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 0);
    EXPECT_TRUE(handleCreate.calls.empty());
}

TEST(MenuHandleListDedup, SkippedEntriesDoNotDisturbValidEntries) {
    json handles = json::array({
        42, "x.flac|subsong:0", nullptr, "", "x.flac|subsong:1",
    });

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    int added = 0;
    EXPECT_TRUE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 1);
    EXPECT_EQ(handleCreate.calls.size(), 2u);
    EXPECT_EQ(added, 2);
}

TEST(MenuHandleListDedup, InvalidHandleIsNotCountedButStillAttempted) {
    json handles = json::array({"x.flac|subsong:0", "x.flac|subsong:1"});

    ValidatorFake validate;
    HandleCreateFake handleCreate;
    handleCreate.invalid.push_back("x.flac");
    int added = 0;
    EXPECT_FALSE(ParseHandleListReimpl(handles, validate, handleCreate, added));

    EXPECT_EQ(validate.calls, 1);
    EXPECT_EQ(handleCreate.calls.size(), 2u);
    EXPECT_EQ(added, 0);
}

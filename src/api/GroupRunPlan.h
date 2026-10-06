// GroupRunPlan.h - grouping-run computation for playlist group headers
//
/* No foobar2000 SDK or Win32 dependency. The caller compiles and evaluates
 * Title Formatting patterns, then feeds the resulting keys in playlist order.
 * This allows the accumulator to run in tests without a host.
 */
#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fb2k_group_runs {

// One or two patterns; the second one sub-groups within each run.
constexpr size_t kMaxPatterns = 2;

// Sentinel for "no offending pattern". Numerically equal to (size_t)-1, but
// this module never includes pfc headers -- the equality with
// pfc::infinite_size is a documented convention, not a compile-time dependency.
constexpr size_t kNoPatternIndex = static_cast<size_t>(-1);

// Outcome of parsing the `patterns` argument. When valid is false errorMessage
// is always non-empty; errorIndex is filled only when one specific pattern is
// at fault, and is reported to the caller as details.pattern.
struct PatternSelection {
    std::vector<std::string> patterns;
    bool valid = true;
    std::string errorMessage;
    size_t errorIndex = kNoPatternIndex;
};

// Parse `patterns` from params. Fail-closed: any shape mismatch is rejected
// rather than guessed at.
//
// Contract:
//   - missing key / not an array / length outside 1..kMaxPatterns -> invalid,
//     with no offending index
//   - an entry that is not a string, or is an empty string -> invalid, with
//     errorIndex pointing at it
// Compilation failure is not decided here: that needs titleformat_compiler and
// is the caller's business.
PatternSelection ParseGroupRunPatterns(const nlohmann::json& params);

// `start` is an absolute playlist row index, not an offset within the parent.
// A parent's first sub-run therefore starts at the parent's `start`.
struct SubRun {
    size_t start = 0;
    size_t count = 0;
    std::string key;
};

// One parent run. Its subs live at [subBegin, subEnd) in the flat sub-run
// array; see the accumulator's note below for why they are not held inline.
struct ParentRun {
    size_t start = 0;
    size_t count = 0;
    std::string key;
    size_t subBegin = 0;
    size_t subEnd = 0;
};

// ASCII case folding. Folds 'a'-'z' only and leaves every other byte
// alone: each byte of a UTF-8 multi-byte sequence is >= 0x80 and can never land
// in 'a'-'z', so folding byte by byte is safe for UTF-8, at the cost of not
// merging case differences outside ASCII. Do NOT swap in toupper() -- under a
// locale it may alter bytes >= 0x80, violating byte preservation.
std::string FoldAscii(const std::string& value);

// Sequential run accumulator. Keys are pushed in playlist order; adjacent equal
// keys (ASCII case-insensitive) merge into one run whose `key` is the first
// row's spelling verbatim. Nothing is sorted -- run order is
// playlist order.
//
// The representation is "closed parent runs + flat sub-run array + per-parent
// index range" rather than each parent holding its own vector<SubRun>. That
// keeps the batch snapshot/rollback down to two lengths and a few
// scalars and the open keys. Capture's cost depends on key lengths, not the
// number of accumulated sub-runs. Were parents to hold their subs inline,
// closing a parent would move that vector away, forcing a rollback to deep-copy
// it; the worst case -- a constant first-level key with a distinct second-level
// key per row -- would then degrade to O(N^2).
class GroupRunAccumulator {
public:
    explicit GroupRunAccumulator(bool twoLevel = false) : twoLevel_(twoLevel) {}

    // Push one row. absoluteIndex must increase by one per row starting at 0;
    // subKey is read only in two-level mode.
    void Push(size_t absoluteIndex, const std::string& primaryKey, const std::string& subKey);

    // Close the trailing run. No Push may follow.
    void Finish();

    // Batch-boundary snapshot. Records the two array lengths plus the open
    // runs' scalar state (and their key strings, whose size is bounded by key
    // length, not by row count); rolling back is a truncate plus a restore.
    struct Snapshot {
        size_t parentCount = 0;
        size_t subCount = 0;
        bool hasOpenParent = false;
        size_t openStart = 0;
        size_t openCount = 0;
        size_t openSubBegin = 0;
        std::string openKey;
        std::string openFold;
        bool hasOpenSub = false;
        size_t openSubStart = 0;
        size_t openSubCount = 0;
        std::string openSubKey;
        std::string openSubFold;
    };

    Snapshot Capture() const;
    void Restore(const Snapshot& snap);

    const std::vector<ParentRun>& parents() const { return parents_; }
    const std::vector<SubRun>& subs() const { return subs_; }
    bool twoLevel() const { return twoLevel_; }

private:
    void CloseOpenSub();
    void CloseOpenParent();

    bool twoLevel_ = false;
    std::vector<ParentRun> parents_;
    std::vector<SubRun> subs_;

    bool hasOpenParent_ = false;
    size_t openStart_ = 0;
    size_t openCount_ = 0;
    size_t openSubBegin_ = 0;
    std::string openKey_;
    std::string openFold_;

    bool hasOpenSub_ = false;
    size_t openSubStart_ = 0;
    size_t openSubCount_ = 0;
    std::string openSubKey_;
    std::string openSubFold_;
};

// Append the success response body to out; the
// caller's buffer is not cleared. Written directly rather than built as a DOM:
// in the worst case runs is as long as total, and a DOM would cost one full
// intermediate copy before dump(). `sub` is emitted only in two-level mode.
// playlistGuid is the GUID of the playlist grouped, taken on the main thread when
// the request was resolved, as the declared result carries it.
void WriteGroupRunsJson(std::string& out, size_t playlist, std::string_view playlistGuid, size_t total,
                        const GroupRunAccumulator& acc);

}  // namespace fb2k_group_runs

#include "pch.h"
#include "api/GroupRunPlan.h"

#include "utils/JsonWriter.h"

namespace fb2k_group_runs {

PatternSelection ParseGroupRunPatterns(const nlohmann::json& params) {
    PatternSelection selection;

    if (!params.contains("patterns")) {
        selection.valid = false;
        selection.errorMessage = "patterns is required";
        return selection;
    }

    const nlohmann::json& raw = params.at("patterns");
    // 显式 null 也落这一支：null 不是数组
    if (!raw.is_array()) {
        selection.valid = false;
        selection.errorMessage = "patterns must be an array of title formatting strings";
        return selection;
    }
    if (raw.empty() || raw.size() > kMaxPatterns) {
        selection.valid = false;
        selection.errorMessage = "patterns must hold one or two title formatting strings";
        return selection;
    }

    for (size_t i = 0; i < raw.size(); i++) {
        const nlohmann::json& entry = raw[i];
        if (!entry.is_string()) {
            selection.valid = false;
            selection.errorMessage = "patterns must contain only title formatting strings";
            selection.errorIndex = i;
            selection.patterns.clear();
            return selection;
        }
        const std::string& pattern = entry.get_ref<const std::string&>();
        if (pattern.empty()) {
            selection.valid = false;
            selection.errorMessage = "patterns must not contain an empty string";
            selection.errorIndex = i;
            selection.patterns.clear();
            return selection;
        }
        selection.patterns.push_back(pattern);
    }

    return selection;
}

std::string FoldAscii(const std::string& value) {
    std::string folded = value;
    for (char& c : folded) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - ('a' - 'A'));
        }
    }
    return folded;
}

void GroupRunAccumulator::CloseOpenSub() {
    if (!hasOpenSub_) return;
    subs_.push_back(SubRun{openSubStart_, openSubCount_, openSubKey_});
    hasOpenSub_ = false;
}

void GroupRunAccumulator::CloseOpenParent() {
    if (!hasOpenParent_) return;
    // 子游程先闭合：它属于正要关掉的这个父游程，必须落在父游程的区间内
    CloseOpenSub();
    parents_.push_back(ParentRun{openStart_, openCount_, openKey_, openSubBegin_, subs_.size()});
    hasOpenParent_ = false;
}

void GroupRunAccumulator::Push(size_t absoluteIndex, const std::string& primaryKey,
                               const std::string& subKey) {
    std::string fold = FoldAscii(primaryKey);
    if (!hasOpenParent_ || fold != openFold_) {
        CloseOpenParent();
        openStart_ = absoluteIndex;
        openCount_ = 0;
        openKey_ = primaryKey;
        openFold_ = std::move(fold);
        openSubBegin_ = subs_.size();
        hasOpenParent_ = true;
    }
    openCount_++;

    if (!twoLevel_) return;

    std::string subFold = FoldAscii(subKey);
    // 父游程换了会把 hasOpenSub_ 清掉，所以跨父边界的同名子键也会重新开一组——
    // 子游程必须包含在自己的父游程内。
    if (!hasOpenSub_ || subFold != openSubFold_) {
        CloseOpenSub();
        openSubStart_ = absoluteIndex;
        openSubCount_ = 0;
        openSubKey_ = subKey;
        openSubFold_ = std::move(subFold);
        hasOpenSub_ = true;
    }
    openSubCount_++;
}

void GroupRunAccumulator::Finish() {
    CloseOpenParent();
}

GroupRunAccumulator::Snapshot GroupRunAccumulator::Capture() const {
    Snapshot snap;
    snap.parentCount = parents_.size();
    snap.subCount = subs_.size();
    snap.hasOpenParent = hasOpenParent_;
    snap.openStart = openStart_;
    snap.openCount = openCount_;
    snap.openSubBegin = openSubBegin_;
    snap.openKey = openKey_;
    snap.openFold = openFold_;
    snap.hasOpenSub = hasOpenSub_;
    snap.openSubStart = openSubStart_;
    snap.openSubCount = openSubCount_;
    snap.openSubKey = openSubKey_;
    snap.openSubFold = openSubFold_;
    return snap;
}

void GroupRunAccumulator::Restore(const Snapshot& snap) {
    parents_.resize(snap.parentCount);
    subs_.resize(snap.subCount);
    hasOpenParent_ = snap.hasOpenParent;
    openStart_ = snap.openStart;
    openCount_ = snap.openCount;
    openSubBegin_ = snap.openSubBegin;
    openKey_ = snap.openKey;
    openFold_ = snap.openFold;
    hasOpenSub_ = snap.hasOpenSub;
    openSubStart_ = snap.openSubStart;
    openSubCount_ = snap.openSubCount;
    openSubKey_ = snap.openSubKey;
    openSubFold_ = snap.openSubFold;
}

namespace {

void AppendRunBody(std::string& out, size_t start, size_t count, const std::string& key) {
    out.append("{\"start\":");
    JsonWriter::AppendJsonInt(out, static_cast<int64_t>(start));
    out.append(",\"count\":");
    JsonWriter::AppendJsonInt(out, static_cast<int64_t>(count));
    out.append(",\"key\":");
    JsonWriter::AppendJsonString(out, key);
}

}  // namespace

void WriteGroupRunsJson(std::string& out, size_t playlist, std::string_view playlistGuid, size_t total,
                        const GroupRunAccumulator& acc) {
    out.append("{\"success\":true,\"playlist\":");
    JsonWriter::AppendJsonInt(out, static_cast<int64_t>(playlist));
    out.append(",\"playlistGuid\":");
    JsonWriter::AppendJsonString(out, playlistGuid);
    out.append(",\"total\":");
    JsonWriter::AppendJsonInt(out, static_cast<int64_t>(total));
    out.append(",\"runs\":[");

    const std::vector<ParentRun>& parents = acc.parents();
    const std::vector<SubRun>& subs = acc.subs();
    for (size_t i = 0; i < parents.size(); i++) {
        if (i > 0) out.push_back(',');
        const ParentRun& parent = parents[i];
        AppendRunBody(out, parent.start, parent.count, parent.key);
        if (acc.twoLevel()) {
            out.append(",\"sub\":[");
            for (size_t s = parent.subBegin; s < parent.subEnd; s++) {
                if (s > parent.subBegin) out.push_back(',');
                AppendRunBody(out, subs[s].start, subs[s].count, subs[s].key);
                out.push_back('}');
            }
            out.push_back(']');
        }
        out.push_back('}');
    }

    out.append("]}");
}

}  // namespace fb2k_group_runs

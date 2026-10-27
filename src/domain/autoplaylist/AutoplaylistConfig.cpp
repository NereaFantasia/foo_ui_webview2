#include "pch.h"
#include "AutoplaylistConfig.h"
#include <array>
#include <iomanip>
#include <random>
#include <sstream>

namespace managed_autoplaylist {

ValidationError::ValidationError(std::string param, const std::string& message)
    : std::runtime_error(message), param(std::move(param)) {}

namespace {
constexpr uint32_t ConfigMagic = 0x43415057;
constexpr uint32_t ConfigVersion = 1;

bool ValidUtf8(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<uint8_t>(text[i++]);
        if (first < 0x80) continue;
        unsigned remaining = 0;
        uint32_t value = 0;
        uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { remaining = 1; value = first & 0x1f; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { remaining = 2; value = first & 0x0f; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; value = first & 0x07; minimum = 0x10000; }
        else return false;
        if (remaining > text.size() - i) return false;
        while (remaining-- > 0) {
            const auto next = static_cast<uint8_t>(text[i++]);
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

std::string NewIdentity() {
    std::random_device random;
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned i = 0; i < 4; ++i) output << std::setw(8) << random();
    return output.str();
}

void WriteWord(std::vector<uint8_t>& bytes, uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<uint8_t>(value >> shift));
}

uint32_t ReadWord(const std::vector<uint8_t>& bytes, size_t& offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) throw ValidationError("configuration", "Truncated configuration");
    uint32_t value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) value |= static_cast<uint32_t>(bytes[offset++]) << shift;
    return value;
}

std::string ReadText(const std::vector<uint8_t>& bytes, size_t& offset) {
    const size_t size = ReadWord(bytes, offset);
    if (size > MaxTextBytes || size > bytes.size() - offset) throw ValidationError("configuration", "Invalid configuration length");
    std::string text(bytes.begin() + offset, bytes.begin() + offset + size);
    offset += size;
    return text;
}

class BusyScope {
public:
    explicit BusyScope(bool& busy) : busy_(busy) { busy_ = true; }
    ~BusyScope() { busy_ = false; }
    BusyScope(const BusyScope&) = delete;
    BusyScope& operator=(const BusyScope&) = delete;
private:
    bool& busy_;
};

void CheckCurrent(IUpdateHost& host) {
    if (!host.IsCurrent()) throw std::runtime_error("Target or provider changed");
}

Failure OperationFailure(std::string reason, std::string stage) {
    Failure failure;
    failure.reason = std::move(reason);
    failure.stage = std::move(stage);
    return failure;
}
} // namespace

void ValidateText(std::string_view text, std::string_view param, bool requireNonblank) {
    if (text.size() > MaxTextBytes) throw ValidationError(std::string(param), "Text exceeds 65536 UTF-8 bytes");
    if (text.find('\0') != std::string_view::npos) throw ValidationError(std::string(param), "Text contains NUL");
    if (!ValidUtf8(text)) throw ValidationError(std::string(param), "Text is not valid UTF-8");
    if (requireNonblank && text.find_first_not_of(" \t\r\n\v\f") == std::string_view::npos) {
        throw ValidationError(std::string(param), "Query must not be empty or blank");
    }
}

Config ApplyPatch(const Config& current, const Patch& patch) {
    if (!patch.query && !patch.sort && !patch.keepSorted) throw ValidationError("query,sort,keepSorted", "At least one configuration field is required");
    Config candidate = current;
    if (patch.query) { ValidateText(*patch.query, "query", true); candidate.query = *patch.query; }
    if (patch.sort) { ValidateText(*patch.sort, "sort"); candidate.sort = *patch.sort; }
    if (patch.keepSorted) candidate.keepSorted = *patch.keepSorted;
    return candidate;
}

std::vector<uint8_t> EncodeConfiguration(const Config& config) {
    ValidateText(config.query, "query", true);
    ValidateText(config.sort, "sort");
    std::vector<uint8_t> bytes;
    bytes.reserve(16 + config.query.size() + config.sort.size());
    WriteWord(bytes, ConfigMagic);
    WriteWord(bytes, ConfigVersion);
    for (const auto* text : {&config.query, &config.sort}) {
        WriteWord(bytes, static_cast<uint32_t>(text->size()));
        bytes.insert(bytes.end(), text->begin(), text->end());
    }
    return bytes;
}

Config DecodeConfiguration(const std::vector<uint8_t>& bytes) {
    if (bytes.size() > 2 * MaxTextBytes + 16) throw ValidationError("configuration", "Configuration exceeds size limit");
    size_t offset = 0;
    if (ReadWord(bytes, offset) != ConfigMagic) throw ValidationError("configuration", "Unknown configuration format");
    if (ReadWord(bytes, offset) != ConfigVersion) throw ValidationError("configuration", "Unknown configuration version");
    Config config;
    config.query = ReadText(bytes, offset);
    config.sort = ReadText(bytes, offset);
    if (offset != bytes.size()) throw ValidationError("configuration", "Unexpected configuration data");
    ValidateText(config.query, "query", true);
    ValidateText(config.sort, "sort");
    return config;
}

ConfigState::ConfigState(Config initial)
    : published_(std::move(initial)), observed_(published_), identity_(NewIdentity()) {
    ValidateText(published_.query, "query", true);
    ValidateText(published_.sort, "sort");
}

Snapshot ConfigState::Current() const { return {published_, identity_ + ":" + std::to_string(revision_)}; }

void ConfigState::BeginSession() {
    identity_ = NewIdentity();
    revision_ = 0;
    baseline_ = false;
}

Snapshot ConfigState::Observe(bool keepSorted, IUpdateHost& host) {
    published_.keepSorted = keepSorted;
    const bool changed = baseline_ && observed_ != published_;
    observed_ = published_;
    baseline_ = true;
    if (changed) ++revision_;
    const auto snapshot = Current();
    if (changed) host.Changed(snapshot);
    return snapshot;
}

Outcome ConfigState::Read(IUpdateHost& host) {
    if (busy_) return OperationFailure("updateInProgress", "read");
    BusyScope scope(busy_);
    try {
        CheckCurrent(host);
        const bool flags = host.ReadKeepSorted();
        CheckCurrent(host);
        return Observe(flags, host);
    } catch (...) {
        return OperationFailure("readFailed", "read");
    }
}

Outcome ConfigState::Update(const Patch& patch, std::string_view expectedRevision, IUpdateHost& host) {
    if (busy_) return OperationFailure("updateInProgress", "validate");
    BusyScope scope(busy_);
    const char* stage = "validate";
    std::optional<Snapshot> before;
    auto settle = [&](Failure failure) -> Outcome {
        if (!before) return failure;
        try {
            CheckCurrent(host);
            const bool flags = host.ReadKeepSorted();
            CheckCurrent(host);
            failure.current = Observe(flags, host);
            failure.state = failure.current->config == before->config ? "unchanged" : "changed";
        } catch (...) {
            failure.current.reset();
            failure.state = "unknown";
        }
        return failure;
    };
    try {
        if (expectedRevision.empty()) throw ValidationError("expectedRevision", "expectedRevision must not be empty");
        // 参数错误在任何宿主调用之前拒绝，候选稍后再与实际 flags 合并。
        (void)ApplyPatch(published_, patch);
        stage = "read";
        CheckCurrent(host);
        const bool initialFlags = host.ReadKeepSorted();
        CheckCurrent(host);
        before = Observe(initialFlags, host);
        if (expectedRevision != before->revision) {
            auto failure = OperationFailure("revisionConflict", "revision");
            failure.current = before;
            failure.state = "unchanged";
            return failure;
        }
        auto candidate = ApplyPatch(before->config, patch);
        if (candidate == before->config) return *before;
        stage = "compile";
        auto compiled = host.Compile(candidate);
        CheckCurrent(host);
        stage = "flags";
        if (candidate.keepSorted != initialFlags) host.SetKeepSorted(candidate.keepSorted);
        CheckCurrent(host);
        stage = "readback";
        const bool confirmedFlags = host.ReadKeepSorted();
        CheckCurrent(host);
        if (confirmedFlags != candidate.keepSorted) throw std::runtime_error("Flags did not match the requested value");
        stage = "publish";
        host.Publish(candidate, std::move(compiled));
        published_ = std::move(candidate);
        stage = "notify";
        host.Notify();
        CheckCurrent(host);
        stage = "readback";
        const bool finalFlags = host.ReadKeepSorted();
        CheckCurrent(host);
        return Observe(finalFlags, host);
    } catch (const ValidationError& error) {
        auto failure = OperationFailure("invalidConfiguration", stage);
        failure.invalidParams = true;
        failure.param = error.param;
        return settle(std::move(failure));
    } catch (...) {
        return settle(OperationFailure("updateFailed", stage));
    }
}
const Config& ConfigState::Published() const noexcept { return published_; }
bool ConfigState::Busy() const noexcept { return busy_; }

} // namespace managed_autoplaylist

#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace managed_autoplaylist {

inline constexpr size_t MaxTextBytes = 65536;

struct Config {
    std::string query;
    std::string sort;
    bool keepSorted = false;
    bool operator==(const Config&) const = default;
};

struct Patch {
    std::optional<std::string> query;
    std::optional<std::string> sort;
    std::optional<bool> keepSorted;
};

struct Snapshot {
    Config config;
    std::string revision;
};

class ValidationError : public std::runtime_error {
public:
    ValidationError(std::string param, const std::string& message);
    std::string param;
};

/** Validate UTF-8 text and byte limits without normalizing the input. */
void ValidateText(std::string_view text, std::string_view param, bool requireNonblank = false);
/** Apply only present fields, including an empty sort or false keepSorted. */
Config ApplyPatch(const Config& current, const Patch& patch);
/** Encode only query and sort; flags and revision belong to the current host session. */
std::vector<uint8_t> EncodeConfiguration(const Config& config);
/** Reject unknown versions, invalid strings and truncated configurations. */
Config DecodeConfiguration(const std::vector<uint8_t>& bytes);

struct CompiledConfig {
    virtual ~CompiledConfig() = default;
};

struct Failure {
    bool invalidParams = false;
    std::string param;
    std::string reason;
    std::string stage;
    std::string state = "unknown";
    std::optional<Snapshot> current;
};
using Outcome = std::variant<Snapshot, Failure>;

/** Host calls may reenter; implementations must pin the playlist and provider identity. */
class IUpdateHost {
public:
    virtual ~IUpdateHost() = default;
    virtual bool IsCurrent() = 0;
    virtual bool ReadKeepSorted() = 0;
    virtual void SetKeepSorted(bool value) = 0;
    virtual std::shared_ptr<const CompiledConfig> Compile(const Config& candidate) = 0;
    virtual void Publish(const Config& candidate, std::shared_ptr<const CompiledConfig> compiled) = 0;
    virtual void Notify() = 0;
    virtual void Changed(const Snapshot& current) = 0;
};

/** Main-thread configuration coordinator shared by native providers and policy tests. */
class ConfigState {
public:
    explicit ConfigState(Config initial);
    /** Invalidate earlier attachment revisions while preserving the published query and sort. */
    void BeginSession();
    Outcome Read(IUpdateHost& host);
    Outcome Update(const Patch& patch, std::string_view expectedRevision, IUpdateHost& host);
    const Config& Published() const noexcept;
    bool Busy() const noexcept;

private:
    Snapshot Current() const;
    Snapshot Observe(bool keepSorted, IUpdateHost& host);
    Config published_;
    Config observed_;
    std::string identity_;
    uint64_t revision_ = 0;
    bool baseline_ = false;
    bool busy_ = false;
};

} // namespace managed_autoplaylist

#pragma once

// ============================================
// JitNativeContext.h - 原生接管的出现账、来源续播位置与恢复计划
//
// 接管时宿主把原生手动队列逐项记成“出现”，即使资源、列表和行完全相同也各占一份；之后按
// 原生事实推进它们的去向，退出时据此只交回未消费的出现，再接上来源的准确后继。这里只做
// 决策与记录，不依赖 foobar2000 SDK：引用和 ID 由调用方生成后传入，原生读写都在调用方。
// 调用方持有 QueueManager 的锁。
// ============================================

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace jit_native {

// 一个手动出现的去向。Pending 与 Queued 可编辑；Restored 是已交还原生队列。
enum class Disposition {
    // 尚未交给原生执行；只预载进影子列表也算
    Pending,
    // 作为唯一的自有 next 留在原生队列里，尚未被取走
    Queued,
    // 已被原生选中或取走，起播尚未确认
    Submitted,
    // 已确认起播；之后重播、失败或停止都不再恢复它
    Consumed,
    // 调用方删除的未消费出现
    Discarded,
    // 提交后缺少足够事实确认结局；不猜作未尝试，不自动恢复
    Indeterminate,
    Restored,
};

// prepared 只有快照、原生仍在执行，只分配来源引用；managed 受管执行；restoring 恢复已开始但未
// 完成；conflicted 因外部修改失效，只能 detach；restored 与 detached 后上下文结束。
enum class Status { Prepared, Managed, Restoring, Restored, Conflicted, Detached };

struct Occurrence {
    std::string id;
    std::string nativeRef;
    Disposition disposition = Disposition::Pending;
};

// 一次手动项编辑：retained 是调用方要保留的出现，顺序即退出后的手动顺序；withdrawals 是被省略
// 的 Queued 项，须先从原生队列物理撤回，才能提交编辑。
struct QueueEdit {
    const void* owner = nullptr;
    std::uint64_t revision = 0;
    std::vector<std::string> retained;
    std::vector<std::string> withdrawals;
};

// 一次恢复：按 occurrences 顺序逐个交还，sourceRow 是随后要排入的来源后继，来源耗尽时为空。
// 来源耗尽且仍有出现要交还时，terminalOccurrence 是最后一个，调用方把它放进专用单曲列表。
struct RestorePlan {
    const void* owner = nullptr;
    std::uint64_t revision = 0;
    std::string recoveryId;
    std::vector<std::string> occurrences;
    std::optional<std::uint64_t> sourceRow;
    std::optional<std::string> terminalOccurrence;
    std::size_t completed = 0;
};

// 计划带上生成它的上下文与版本：账在中途有任何变化，旧计划的后续步骤都被拒绝，调用方须重读
// 事实再规划。
class NativeContext {
public:
    // sourceCount 与 currentRow 取自快照时的来源；manual 是原生手动队列的逐次出现，全部 Pending。
    NativeContext(std::uint64_t sourceCount, std::uint64_t currentRow, std::vector<Occurrence> manual)
        : m_sourceCount(sourceCount), m_sourceNext(currentRow + 1), m_manual(std::move(manual)) {
        if (currentRow >= sourceCount) throw std::invalid_argument("current row outside source");
        std::unordered_set<std::string> ids;
        std::unordered_set<std::string> refs;
        for (const Occurrence& item : m_manual) {
            if (item.id.empty() || item.nativeRef.empty() || !ids.insert(item.id).second ||
                !refs.insert(item.nativeRef).second || item.disposition != Disposition::Pending) {
                throw std::invalid_argument("invalid native occurrences");
            }
            m_order.push_back(item.id);
        }
    }

    Status GetStatus() const { return m_status; }
    std::uint64_t Revision() const { return m_revision; }
    std::uint64_t SourceNext() const { return m_sourceNext; }
    std::uint64_t SourceCount() const { return m_sourceCount; }
    const std::string& RecoveryId() const { return m_recoveryId; }
    const std::vector<Occurrence>& Manual() const { return m_manual; }

    const Occurrence* Find(const std::string& id) const {
        for (const Occurrence& item : m_manual) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    // 待恢复出现的排位，从 0 起；不待恢复时为空。
    std::optional<std::size_t> RestorationRank(const std::string& id) const {
        std::size_t rank = 0;
        for (const std::string& ordered : m_order) {
            const Occurrence* item = Find(ordered);
            if (!item || !Editable(*item)) continue;
            if (ordered == id) return rank;
            ++rank;
        }
        return std::nullopt;
    }

    // 接纳快照时的当前曲目：它就是来源当前行，引用须已登记；同时绑定调用方给的条目 ID。
    bool Adopt(const std::string& currentRef, const std::string& entry) {
        if (m_status != Status::Prepared || SourceRef(m_sourceNext - 1) != currentRef || entry.empty()) return false;
        m_status = Status::Managed;
        m_bindings.emplace(currentRef, entry);
        ++m_revision;
        return true;
    }

    std::optional<std::string> SourceRef(std::uint64_t row) const {
        const auto found = m_sourceRefs.find(row);
        if (found == m_sourceRefs.end()) return std::nullopt;
        return found->second;
    }

    // 给来源行分配引用。同一行重复登记须给同一个引用；读页不改变版本。
    bool RegisterSource(std::uint64_t row, const std::string& ref) {
        if ((m_status != Status::Prepared && m_status != Status::Managed) || row >= m_sourceCount || ref.empty()) return false;
        if (const auto found = m_sourceRefs.find(row); found != m_sourceRefs.end()) return found->second == ref;
        if (KnownRef(ref)) return false;
        m_sourceRefs.emplace(row, ref);
        return true;
    }

    // 引用第一次被供给时绑定条目 ID，之后同一引用的重播沿用它；不同引用不能共用一个条目 ID。
    bool Bind(const std::string& ref, const std::string& entry) {
        if (m_status != Status::Managed || entry.empty() || !KnownRef(ref)) return false;
        if (const auto found = m_bindings.find(ref); found != m_bindings.end()) return found->second == entry;
        if (!EntryAvailableForLocation(entry)) return false;
        m_bindings.emplace(ref, entry);
        ++m_revision;
        return true;
    }

    std::optional<std::string> EntryOf(const std::string& ref) const {
        const auto found = m_bindings.find(ref);
        if (found == m_bindings.end()) return std::nullopt;
        return found->second;
    }

    // 普通资源条目不能冒用已绑定到原生引用的条目 ID。
    bool EntryAvailableForLocation(const std::string& entry) const {
        return std::none_of(m_bindings.begin(), m_bindings.end(),
                            [&](const auto& binding) { return binding.second == entry; });
    }

    // 出现作为自有 next 进了原生队列。同一时刻最多一个。
    bool Queue(const std::string& id) {
        Occurrence* item = Mutable(id);
        if (m_status != Status::Managed || !item || item->disposition != Disposition::Pending ||
            std::any_of(m_manual.begin(), m_manual.end(),
                        [](const Occurrence& row) { return row.disposition == Disposition::Queued; })) {
            return false;
        }
        item->disposition = Disposition::Queued;
        ++m_revision;
        return true;
    }

    std::optional<std::uint64_t> SourceRowOf(const std::string& ref) const {
        for (const auto& [row, sourceRef] : m_sourceRefs) {
            if (sourceRef == ref) return row;
        }
        return std::nullopt;
    }

    // ref 所属的手动出现；来源行或外来引用为空。
    const Occurrence* OccurrenceOfRef(const std::string& ref) const {
        for (const Occurrence& item : m_manual) {
            if (item.nativeRef == ref) return &item;
        }
        return nullptr;
    }

    // 作为自有 next 的出现已确认从原生队列撤回，尚未被取走：回到 Pending。
    bool WithdrawQueued(const std::string& id) {
        return m_status == Status::Managed && Unqueue(id);
    }

    // 原生已选中或取走该出现。
    bool Submit(const std::string& id) {
        Occurrence* item = Mutable(id);
        if (m_status != Status::Managed || !item || !Editable(*item)) return false;
        item->disposition = Disposition::Submitted;
        ++m_revision;
        return true;
    }

    // 确认 ref 实际起播。手动出现记为已消费，冲突后也照记，免得恢复时把它重新加回；来源行
    // 把续播位置移到它之后，可以回退，不取已播过的最大行。返回 ref 是否属于本上下文。
    bool Started(const std::string& ref) {
        if (m_status == Status::Prepared || m_status == Status::Restored || m_status == Status::Detached) return false;
        for (Occurrence& item : m_manual) {
            if (item.nativeRef != ref) continue;
            if (Editable(item) || item.disposition == Disposition::Submitted ||
                item.disposition == Disposition::Indeterminate) {
                item.disposition = Disposition::Consumed;
                ++m_revision;
            }
            return true;
        }
        if (m_status == Status::Conflicted) return false;
        for (const auto& [row, sourceRef] : m_sourceRefs) {
            if (sourceRef != ref) continue;
            m_sourceNext = row + 1;
            ++m_revision;
            return true;
        }
        return false;
    }

    // 已提交的出现等不到起播确认。
    bool Indeterminate(const std::string& id) {
        Occurrence* item = Mutable(id);
        if (!item || item->disposition != Disposition::Submitted) return false;
        item->disposition = Disposition::Indeterminate;
        ++m_revision;
        return true;
    }

    // 校验编辑并列出须先撤回的 Queued 项。重复、外来或已不可编辑的 ID 整体拒绝。
    std::optional<QueueEdit> PlanEdit(const std::vector<std::string>& retained) const {
        if (m_status != Status::Managed) return std::nullopt;
        std::unordered_set<std::string> unique;
        for (const std::string& id : retained) {
            const Occurrence* item = Find(id);
            if (!item || !Editable(*item) || !unique.insert(id).second) return std::nullopt;
        }
        QueueEdit plan{this, m_revision, retained, {}};
        for (const Occurrence& item : m_manual) {
            if (item.disposition == Disposition::Queued && !unique.contains(item.id)) plan.withdrawals.push_back(item.id);
        }
        return plan;
    }

    // 编辑计划里的 Queued 项已确认从原生队列撤回。被原生取走的项不能借撤回改回 Pending。
    bool Withdrawn(QueueEdit& plan, const std::string& id) {
        if (!IsCurrent(plan.owner, plan.revision) ||
            std::find(plan.withdrawals.begin(), plan.withdrawals.end(), id) == plan.withdrawals.end()) {
            return false;
        }
        if (!Unqueue(id)) return false;
        plan.revision = m_revision;
        return true;
    }

    // 撤回全部完成后提交：未保留的 Pending 项记为删除，保留项的顺序成为恢复顺序。
    bool CommitEdit(const QueueEdit& plan) {
        if (!IsCurrent(plan.owner, plan.revision)) return false;
        const auto checked = PlanEdit(plan.retained);
        if (!checked || !checked->withdrawals.empty()) return false;
        const std::unordered_set<std::string> kept(plan.retained.begin(), plan.retained.end());
        for (Occurrence& item : m_manual) {
            if (item.disposition == Disposition::Pending && !kept.contains(item.id)) item.disposition = Disposition::Discarded;
        }
        m_order = plan.retained;
        ++m_revision;
        return true;
    }

    // 规划恢复。还有已提交或结局不明的出现时拒绝：默认恢复不能跳过它们后宣告成功。Queued 项
    // 照常排在原位，轮到它之前须先经 RestoreWithdrawn 确认撤回。同一上下文只有一个
    // recoveryId，部分失败后重试沿用它，已交还的出现不会再出现在新计划里。
    std::optional<RestorePlan> PlanRestore(const std::string& recoveryId) {
        if ((m_status != Status::Managed && m_status != Status::Restoring) || recoveryId.empty() ||
            (!m_recoveryId.empty() && m_recoveryId != recoveryId)) {
            return std::nullopt;
        }
        for (const Occurrence& item : m_manual) {
            if (item.disposition == Disposition::Submitted || item.disposition == Disposition::Indeterminate) {
                return std::nullopt;
            }
        }
        RestorePlan plan{this, m_revision, recoveryId, {}, {}, {}, 0};
        for (const std::string& id : m_order) {
            if (Editable(*Find(id))) plan.occurrences.push_back(id);
        }
        if (m_sourceNext < m_sourceCount) plan.sourceRow = m_sourceNext;
        else if (!plan.occurrences.empty()) plan.terminalOccurrence = plan.occurrences.back();
        if (m_status == Status::Managed) {
            m_recoveryId = recoveryId;
            m_status = Status::Restoring;
            ++m_revision;
            plan.revision = m_revision;
        }
        return plan;
    }

    // 恢复计划里的 Queued 项已确认从原生队列撤回。
    bool RestoreWithdrawn(RestorePlan& plan, const std::string& id) {
        if (!IsCurrentRestore(plan) ||
            std::find(plan.occurrences.begin() + static_cast<std::ptrdiff_t>(plan.completed), plan.occurrences.end(), id) ==
                plan.occurrences.end()) {
            return false;
        }
        if (!Unqueue(id)) return false;
        plan.revision = m_revision;
        return true;
    }

    // 按计划顺序交还了下一个出现。
    bool Restored(RestorePlan& plan, const std::string& id) {
        if (!IsCurrentRestore(plan) || plan.completed >= plan.occurrences.size() ||
            plan.occurrences[plan.completed] != id) {
            return false;
        }
        Occurrence* item = Mutable(id);
        if (!item || item->disposition != Disposition::Pending) return false;
        item->disposition = Disposition::Restored;
        ++plan.completed;
        plan.revision = ++m_revision;
        return true;
    }

    // 出现全部交还、来源后继也已排入后结束恢复。
    bool FinishRestore(const RestorePlan& plan) {
        if (!IsCurrentRestore(plan) || plan.completed != plan.occurrences.size()) return false;
        m_status = Status::Restored;
        ++m_revision;
        return true;
    }

    // 外部修改了来源、队列、设置或改播：未完成的编辑与恢复计划全部作废，之后只能 detach。
    void Conflict() {
        if (m_status == Status::Restored || m_status == Status::Detached || m_status == Status::Conflicted) return;
        m_status = Status::Conflicted;
        ++m_revision;
    }

    // 调用方放弃自动恢复。未交还的出现保持原去向，留在账里供查询。
    bool Detach() {
        if (m_status == Status::Restored || m_status == Status::Detached) return false;
        m_status = Status::Detached;
        ++m_revision;
        return true;
    }

private:
    static bool Editable(const Occurrence& item) {
        return item.disposition == Disposition::Pending || item.disposition == Disposition::Queued;
    }

    bool IsCurrent(const void* owner, std::uint64_t revision) const {
        return m_status == Status::Managed && owner == this && revision == m_revision;
    }

    bool IsCurrentRestore(const RestorePlan& plan) const {
        return m_status == Status::Restoring && plan.owner == this && plan.revision == m_revision &&
               plan.recoveryId == m_recoveryId;
    }

    bool Unqueue(const std::string& id) {
        Occurrence* item = Mutable(id);
        if (!item || item->disposition != Disposition::Queued) return false;
        item->disposition = Disposition::Pending;
        ++m_revision;
        return true;
    }

    Occurrence* Mutable(const std::string& id) {
        for (Occurrence& item : m_manual) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    bool KnownRef(const std::string& ref) const {
        return std::any_of(m_manual.begin(), m_manual.end(), [&](const Occurrence& item) { return item.nativeRef == ref; }) ||
               std::any_of(m_sourceRefs.begin(), m_sourceRefs.end(), [&](const auto& item) { return item.second == ref; });
    }

    std::uint64_t m_sourceCount;
    std::uint64_t m_sourceNext;
    std::uint64_t m_revision = 1;
    Status m_status = Status::Prepared;
    std::string m_recoveryId;
    std::vector<Occurrence> m_manual;
    std::vector<std::string> m_order;
    std::unordered_map<std::uint64_t, std::string> m_sourceRefs;
    std::unordered_map<std::string, std::string> m_bindings;
};

}  // namespace jit_native

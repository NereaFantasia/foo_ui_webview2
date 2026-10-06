// ProbeFailureClassifier.h
//
// metadata.probeBatchAsync 失败分类的唯一定义点。
//
// 探测一条曲目时 SDK 可能抛六类异常，页面只需要三个 failure 值加一个 aborted
// 信号；六到三的映射靠 catch 链的顺序实现，顺序错一步就会把「取消」报成
// read-error，或让 unsupported-format 永不出现。这条链只写在这里：生产端
// （MetadataApi.cpp ProbeOneTrack）用真实 SDK 类型实例化，单测
// （tests/test_async_operation_registry.cpp）用镜像层级实例化，改顺序两边同时变。
//
// 本头不引任何 SDK 头。SDK 的 exception_io.h / abort_callback.h 不自含，而测试
// 工程刻意不拽 SDK 头链（tests/pch.h），所以异常类型只能以模板参数进来。
#pragma once

#include <exception>
#include <type_traits>

namespace probe_failure {

// 页面契约定死的三个 failure 字符串。
inline constexpr const char* kNotFound = "not-found";
inline constexpr const char* kUnsupportedFormat = "unsupported-format";
inline constexpr const char* kReadError = "read-error";

struct Classification {
    bool threw = false;             // false = 探测体正常返回，其余字段无意义
    bool aborted = false;           // 取消不是 failure，与 failure 互斥
    const char* failure = nullptr;  // kNotFound | kUnsupportedFormat | kReadError
};

// 模板参数顺序就是 catch 顺序：具体子类在前、基类在后。static_assert 把
// 「谁是谁的子类」钉死，参数传错位置直接编译失败，而不是运行时静默吞分类。
template <class Aborted, class IoNotFound, class IoUnsupportedFormat, class IoData, class Io>
struct Classifier {
    static_assert(std::is_base_of_v<std::exception, Aborted>,
                  "Aborted must derive from std::exception, otherwise the last catch is not a fallback");
    static_assert(std::is_base_of_v<std::exception, Io>,
                  "Io must derive from std::exception");
    static_assert(std::is_base_of_v<Io, IoNotFound>,
                  "IoNotFound must derive from Io");
    static_assert(std::is_base_of_v<Io, IoData>,
                  "IoData must derive from Io");
    static_assert(std::is_base_of_v<IoData, IoUnsupportedFormat>,
                  "IoUnsupportedFormat must derive from IoData; that is why it is caught first");
    static_assert(!std::is_base_of_v<IoData, IoNotFound>,
                  "IoNotFound is a sibling of IoData, not a child; the chain relies on that");
    static_assert(!std::is_base_of_v<Aborted, Io> && !std::is_base_of_v<Io, Aborted>,
                  "Aborted and Io must be unrelated so that neither catch shadows the other");

    // 执行 fn。正常返回时 threw=false；抛出时按链分类，fn 内部已写入的状态
    // 由调用方自行决定保留还是覆盖。
    template <class Fn>
    static Classification Classify(Fn&& fn) {
        Classification c;
        try {
            fn();
            return c;
        } catch (const Aborted&) {
            c.aborted = true;
        } catch (const IoNotFound&) {
            c.failure = kNotFound;
        } catch (const IoUnsupportedFormat&) {
            c.failure = kUnsupportedFormat;
        } catch (const IoData&) {
            c.failure = kReadError;
        } catch (const Io&) {
            c.failure = kReadError;
        } catch (const std::exception&) {
            c.failure = kReadError;
        }
        c.threw = true;
        return c;
    }
};

}  // namespace probe_failure

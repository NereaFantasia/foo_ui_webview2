// test_path_security_decorator.cpp - Path security wrapping pattern tests
// Tests the decorator that BridgeCore wraps around handlers with PathSecuritySpecs.
// Does NOT test the real PathSecurity validation (Win32-dependent), only the wrapping pattern.
#include "pch.h"
#include "compat/fb2k_types.h"
#include "../src/api/ErrorEnvelope.h"

using json = nlohmann::json;

// ============================================
// Simplified PathSecuritySpec + ValidatePathParam for testing
// Mirrors the wrapping pattern from BridgeCore::RegisterApi(method, handler, specs)
// ============================================
namespace reimpl {

struct PathSecuritySpec {
    std::string paramKey;     // JSON key that contains the path
    bool required;            // Whether the param must be present
};

struct ValidationResult {
    bool success;
    std::string errorMsg;
    bool shapeError = false;  // Mirrors the production field the wrapper dispatches on
};

// Simplified validator: blocks paths containing ".." (traversal) or starting with "\\\\"  (UNC)
static ValidationResult ValidatePathParam(const json& params, const PathSecuritySpec& spec,
                                           const std::string& /*method*/) {
    if (!params.contains(spec.paramKey) || !params[spec.paramKey].is_string()) {
        if (spec.required) {
            // Absent or wrong-typed parameter is a shape failure, not a security refusal
            return {false, "Required path parameter missing: " + spec.paramKey, true};
        }
        return {true, ""};
    }

    std::string path = params[spec.paramKey].get<std::string>();

    // Block directory traversal
    if (path.find("..") != std::string::npos) {
        return {false, "Path traversal detected in " + spec.paramKey};
    }

    // Block UNC paths
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
        return {false, "UNC paths not allowed for " + spec.paramKey};
    }

    return {true, ""};
}

using ApiHandler = std::function<json(const json& params)>;

// Mirrors BridgeCore::RegisterApi wrapping logic
static ApiHandler WrapWithSecurity(ApiHandler inner, std::vector<PathSecuritySpec> specs,
                                    const std::string& method) {
    return [innerHandler = std::move(inner), innerSpecs = std::move(specs),
            methodName = method](const json& params) -> json {
        for (const auto& spec : innerSpecs) {
            auto r = ValidatePathParam(params, spec, methodName);
            if (!r.success) {
                return ApiEnvelope::MakeError(
                    r.errorMsg.c_str(),
                    r.shapeError ? ApiErrorCode::INVALID_PARAMS : ApiErrorCode::PERMISSION_DENIED);
            }
        }
        return innerHandler(params);
    };
}

} // namespace reimpl

// ============================================
// Tests
// ============================================

class PathSecurityDecoratorTest : public ::testing::Test {
protected:
    json echoResult(const json& params) {
        return {{"success", true}, {"receivedPath", params.value("path", "")}};
    }

    reimpl::ApiHandler echoHandler = [this](const json& params) -> json {
        return echoResult(params);
    };
};

TEST_F(PathSecurityDecoratorTest, NoSpecs_HandlerCalledNormally) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler, {}, "test.api");
    json result = wrapped({{"path", "E:\\Music\\song.flac"}});
    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["receivedPath"], "E:\\Music\\song.flac");
}

TEST_F(PathSecurityDecoratorTest, ValidPath_PassesThrough) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler,
        {{"path", true}}, "test.api");
    json result = wrapped({{"path", "E:\\Music\\song.flac"}});
    EXPECT_TRUE(result["success"].get<bool>());
}

TEST_F(PathSecurityDecoratorTest, TraversalPath_Blocked) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler,
        {{"path", true}}, "test.api");
    json result = wrapped({{"path", "E:\\Music\\..\\..\\Windows\\system32\\cmd.exe"}});
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["code"].get<std::string>(), "PERMISSION_DENIED");
}

TEST_F(PathSecurityDecoratorTest, UncPath_Blocked) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler,
        {{"path", true}}, "test.api");
    json result = wrapped({{"path", "\\\\server\\share\\file.mp3"}});
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["code"].get<std::string>(), "PERMISSION_DENIED");
}

TEST_F(PathSecurityDecoratorTest, RequiredParam_Missing_Error) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler,
        {{"path", true}}, "test.api");
    json result = wrapped(json::object());  // no "path" key
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["code"].get<std::string>(), "INVALID_PARAMS");
}

TEST_F(PathSecurityDecoratorTest, OptionalParam_Missing_PassThrough) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler,
        {{"path", false}}, "test.api");
    json result = wrapped(json::object());  // no "path" key, but optional
    EXPECT_TRUE(result["success"].get<bool>());
}

TEST_F(PathSecurityDecoratorTest, NonStringParam_OptionalNotRequired_PassThrough) {
    // Use a handler that doesn't access the "path" key to avoid type_error
    auto safeHandler = [](const json&) -> json {
        return {{"success", true}};
    };
    auto wrapped = reimpl::WrapWithSecurity(safeHandler,
        {{"path", false}}, "test.api");
    json result = wrapped({{"path", 42}});  // path is number, not string — optional, so skipped
    EXPECT_TRUE(result["success"].get<bool>());
}

TEST_F(PathSecurityDecoratorTest, MultipleSpecs_FirstFails_ErrorWithoutCallingHandler) {
    bool handlerCalled = false;
    auto trackingHandler = [&handlerCalled](const json& params) -> json {
        handlerCalled = true;
        return {{"success", true}};
    };
    auto wrapped = reimpl::WrapWithSecurity(trackingHandler,
        {{"sourcePath", true}, {"destPath", true}}, "test.api");
    json result = wrapped({{"sourcePath", "C:\\..\\bad"}, {"destPath", "E:\\ok"}});
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_FALSE(handlerCalled);
}

TEST_F(PathSecurityDecoratorTest, MultipleSpecs_AllPass_HandlerCalled) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler,
        {{"sourcePath", true}, {"destPath", true}}, "test.api");
    json result = wrapped({{"sourcePath", "E:\\src"}, {"destPath", "E:\\dst"}, {"path", "x"}});
    EXPECT_TRUE(result["success"].get<bool>());
}

TEST_F(PathSecurityDecoratorTest, SecondSpec_Fails) {
    auto wrapped = reimpl::WrapWithSecurity(echoHandler,
        {{"sourcePath", true}, {"destPath", true}}, "test.api");
    json result = wrapped({{"sourcePath", "E:\\ok"}, {"destPath", "\\\\server\\hack"}});
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["code"].get<std::string>(), "PERMISSION_DENIED");
}

TEST_F(PathSecurityDecoratorTest, NonStringParam_Required_InvalidParams) {
    auto safeHandler = [](const json&) -> json {
        return {{"success", true}};
    };
    auto wrapped = reimpl::WrapWithSecurity(safeHandler,
        {{"path", true}}, "test.api");
    json result = wrapped({{"path", 42}});  // wrong type, and the spec requires it
    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["code"].get<std::string>(), "INVALID_PARAMS");
}

// Pins the dispatch itself: one wrapped handler, one spec, two inputs that differ
// only in how they fail. A wrapper that collapses both onto one code fails here
// even though every EXPECT_FALSE(success) above would still pass.
TEST_F(PathSecurityDecoratorTest, ShapeAndSecurityFailures_GetDifferentCodes) {
    auto safeHandler = [](const json&) -> json {
        return {{"success", true}};
    };
    auto wrapped = reimpl::WrapWithSecurity(safeHandler,
        {{"path", true}}, "test.api");

    json shapeFailure = wrapped({{"path", 42}});
    json securityFailure = wrapped({{"path", "\\\\server\\share\\file.mp3"}});

    EXPECT_FALSE(shapeFailure["success"].get<bool>());
    EXPECT_FALSE(securityFailure["success"].get<bool>());
    EXPECT_EQ(shapeFailure["code"].get<std::string>(), "INVALID_PARAMS");
    EXPECT_EQ(securityFailure["code"].get<std::string>(), "PERMISSION_DENIED");
    EXPECT_NE(shapeFailure["code"].get<std::string>(),
              securityFailure["code"].get<std::string>());
}

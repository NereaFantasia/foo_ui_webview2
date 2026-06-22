// test_guid_utils.cpp — GUID 序列化测试
#include "pch.h"
#include <Windows.h>
#include "../src/utils/GuidUtils.h"

TEST(GuidUtils, GuidToString_KnownValue) {
    GUID g = { 0x12345678, 0xABCD, 0xEF01, {0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01} };
    auto str = GuidUtils::GuidToString(g);
    EXPECT_EQ(str, "{12345678-ABCD-EF01-2345-6789ABCDEF01}");
}

TEST(GuidUtils, GuidToString_AllZeros) {
    GUID g = {};
    auto str = GuidUtils::GuidToString(g);
    EXPECT_EQ(str, "{00000000-0000-0000-0000-000000000000}");
}

TEST(GuidUtils, StringToGuid_Valid) {
    GUID g;
    bool ok = GuidUtils::StringToGuid("{12345678-ABCD-EF01-2345-6789ABCDEF01}", g);
    EXPECT_TRUE(ok);
    EXPECT_EQ(g.Data1, 0x12345678u);
    EXPECT_EQ(g.Data2, 0xABCDu);
    EXPECT_EQ(g.Data3, 0xEF01u);
    EXPECT_EQ(g.Data4[0], 0x23u);
    EXPECT_EQ(g.Data4[7], 0x01u);
}

TEST(GuidUtils, StringToGuid_Invalid) {
    GUID g;
    EXPECT_FALSE(GuidUtils::StringToGuid("not-a-guid", g));
    EXPECT_FALSE(GuidUtils::StringToGuid("", g));
    EXPECT_FALSE(GuidUtils::StringToGuid("{12345678-ABCD}", g));
}

TEST(GuidUtils, Roundtrip) {
    GUID original = { 0xDEADBEEF, 0xCAFE, 0xBEEF, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08} };
    auto str = GuidUtils::GuidToString(original);
    GUID parsed;
    EXPECT_TRUE(GuidUtils::StringToGuid(str, parsed));
    EXPECT_EQ(original.Data1, parsed.Data1);
    EXPECT_EQ(original.Data2, parsed.Data2);
    EXPECT_EQ(original.Data3, parsed.Data3);
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(original.Data4[i], parsed.Data4[i]);
    }
}

#include <gtest/gtest.h>
#include "slideio/core/metadata.hpp"
#include "slideio/core/metadata_internal.hpp"
#include <nlohmann/json.hpp>

using namespace slideio;

TEST(Metadata, DefaultIsNull)
{
    Metadata m;
    EXPECT_EQ(m.type(), Metadata::Type::Null);
    EXPECT_TRUE(m.isNull());
    EXPECT_FALSE(m.isObject());
    EXPECT_FALSE(m.isArray());
    EXPECT_EQ(m.size(), 0u);
}

TEST(Metadata, ScalarAccessors)
{
    nlohmann::json j = {
        {"i", 42},
        {"d", 3.14},
        {"s", "hello"},
        {"b", true},
    };
    Metadata m = detail::makeMetadataFromJson(std::move(j));

    EXPECT_EQ(m.type(), Metadata::Type::Object);
    EXPECT_EQ(m.size(), 4u);
    EXPECT_EQ(m["i"].asInt(), 42);
    EXPECT_DOUBLE_EQ(m["d"].asDouble(), 3.14);
    EXPECT_EQ(m["s"].asString(), "hello");
    EXPECT_TRUE(m["b"].asBool());
}

TEST(Metadata, ScalarConversionsAcrossTypes)
{
    nlohmann::json j = {{"n", "123"}, {"flag", "true"}};
    Metadata m = detail::makeMetadataFromJson(std::move(j));

    EXPECT_EQ(m["n"].asInt(), 123);
    EXPECT_DOUBLE_EQ(m["n"].asDouble(), 123.0);
    EXPECT_TRUE(m["flag"].asBool());
}

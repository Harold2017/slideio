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

TEST(Metadata, ObjectNavigation)
{
    nlohmann::json j = {
        {"a", { {"b", { {"c", 7} }} }},
        {"arr", {1, 2, 3}}
    };
    Metadata m = detail::makeMetadataFromJson(std::move(j));

    EXPECT_TRUE(m.contains("a"));
    EXPECT_FALSE(m.contains("missing"));
    EXPECT_TRUE(m["missing"].isNull());

    EXPECT_EQ(m["a"]["b"]["c"].asInt(), 7);
    EXPECT_EQ(m["arr"].size(), 3u);
    EXPECT_EQ(m["arr"][1].asInt(), 2);
    EXPECT_TRUE(m["arr"][99].isNull());
}

TEST(Metadata, JsonPointerFind)
{
    nlohmann::json j = nlohmann::json::parse(R"({
        "scenes": [
            {"name": "S0", "channels": ["R","G","B"]},
            {"name": "S1", "channels": ["Y"]}
        ]
    })");
    Metadata m = detail::makeMetadataFromJson(std::move(j));

    EXPECT_EQ(m.find("/scenes/0/name").asString(), "S0");
    EXPECT_EQ(m.find("/scenes/1/channels/0").asString(), "Y");
    EXPECT_TRUE(m.find("/missing/path").isNull());
    EXPECT_TRUE(m.find("not-a-pointer").isNull());
}

TEST(Metadata, KeysAndTypes)
{
    nlohmann::json j = {{"a", 1}, {"b", "x"}, {"c", nullptr}};
    Metadata m = detail::makeMetadataFromJson(std::move(j));
    auto keys = m.keys();
    ASSERT_EQ(keys.size(), 3u);
    // nlohmann::json preserves insertion order
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[1], "b");
    EXPECT_EQ(keys[2], "c");

    EXPECT_EQ(m["a"].type(), Metadata::Type::Int);
    EXPECT_EQ(m["b"].type(), Metadata::Type::String);
    EXPECT_EQ(m["c"].type(), Metadata::Type::Null);
}

TEST(Metadata, ChildOutlivesParentCopy)
{
    Metadata child;
    {
        nlohmann::json j = {{"deep", {{"value", 99}}}};
        Metadata parent = detail::makeMetadataFromJson(std::move(j));
        child = parent["deep"]["value"];
        // parent goes out of scope here; root tree must stay alive via shared_ptr
    }
    EXPECT_EQ(child.asInt(), 99);
}

TEST(Metadata, ToJsonRoundTrip)
{
    nlohmann::json j = {{"k", 1}, {"v", "hello"}};
    Metadata m = detail::makeMetadataFromJson(std::move(j));
    auto round = nlohmann::json::parse(m.toJson());
    EXPECT_EQ(round["k"], 1);
    EXPECT_EQ(round["v"], "hello");
}

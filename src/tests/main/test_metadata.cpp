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

TEST(MetadataXml, ScalarLeaf)
{
    auto j = detail::xmlStringToJson("<Foo>123</Foo>");
    EXPECT_EQ(j["Foo"], "123");
}

TEST(MetadataXml, AttributesAndText)
{
    auto j = detail::xmlStringToJson(R"(<Foo a="1" b="x">hello</Foo>)");
    ASSERT_TRUE(j["Foo"].is_object());
    EXPECT_EQ(j["Foo"]["@a"], "1");
    EXPECT_EQ(j["Foo"]["@b"], "x");
    EXPECT_EQ(j["Foo"]["#text"], "hello");
}

TEST(MetadataXml, NestedChildren)
{
    auto j = detail::xmlStringToJson(R"(<R><A>1</A><B>2</B></R>)");
    EXPECT_EQ(j["R"]["A"], "1");
    EXPECT_EQ(j["R"]["B"], "2");
}

TEST(MetadataXml, RepeatedSiblingsBecomeArray)
{
    auto j = detail::xmlStringToJson(R"(<R><Item>1</Item><Item>2</Item><Item>3</Item></R>)");
    ASSERT_TRUE(j["R"]["Item"].is_array());
    EXPECT_EQ(j["R"]["Item"].size(), 3u);
    EXPECT_EQ(j["R"]["Item"][0], "1");
    EXPECT_EQ(j["R"]["Item"][2], "3");
}

TEST(MetadataXml, MalformedReturnsErrorObject)
{
    auto j = detail::xmlStringToJson("<not <valid xml");
    ASSERT_TRUE(j.is_object());
    EXPECT_TRUE(j.contains("#error"));
}

TEST(MetadataXml, EndToEndViaMetadata)
{
    auto j = detail::xmlStringToJson(R"(<Image><Pixels SizeX="512" SizeY="256"/></Image>)");
    Metadata m = detail::makeMetadataFromJson(std::move(j));
    EXPECT_EQ(m["Image"]["Pixels"]["@SizeX"].asInt(), 512);
    EXPECT_EQ(m["Image"]["Pixels"]["@SizeY"].asInt(), 256);
}

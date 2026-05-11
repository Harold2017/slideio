// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/metadata.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

TEST(MetadataBuilder, DefaultIsNull)
{
    MetadataBuilder b;
    EXPECT_TRUE(b.isNull());

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::Null);
}

TEST(MetadataBuilder, SetStringRoundtrip)
{
    MetadataBuilder b;
    b.set(std::string("hello"));

    Metadata m = b.freeze();
    EXPECT_EQ(m.type(), Metadata::Type::String);
    EXPECT_EQ(m.asString(), "hello");
}

TEST(MetadataBuilder, SetTypedLeafRoundtrip)
{
    {
        MetadataBuilder b;
        b.set(true);
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Bool);
        EXPECT_TRUE(m.asBool());
    }
    {
        MetadataBuilder b;
        b.set(static_cast<int64_t>(488));
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Int);
        EXPECT_EQ(m.asInt(), 488);
    }
    {
        MetadataBuilder b;
        b.set(0.1);
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::Double);
        EXPECT_DOUBLE_EQ(m.asDouble(), 0.1);
    }
    {
        MetadataBuilder b;
        b.set("literal");
        Metadata m = b.freeze();
        EXPECT_EQ(m.type(), Metadata::Type::String);
        EXPECT_EQ(m.asString(), "literal");
    }
}

TEST(MetadataBuilder, SetConstCharNullThrows)
{
    MetadataBuilder b;
    EXPECT_THROW(b.set(static_cast<const char*>(nullptr)), slideio::RuntimeError);
}

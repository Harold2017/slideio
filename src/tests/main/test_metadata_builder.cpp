// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/metadata.hpp"

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

#include <gtest/gtest.h>
#include "slideio/core/metadata.hpp"

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

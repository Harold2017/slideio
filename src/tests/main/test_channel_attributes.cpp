// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/cvscene.hpp"
#include "slideio/base/exceptions.hpp"
#include "tests/testlib/testscene.hpp"
#include <memory>

using namespace slideio;

// Test fixture for channel attributes
class ChannelAttributesTest : public ::testing::Test {
protected:
    void SetUp() override {
        scene = std::make_shared<TestScene>();
        scene->setNumChannels(3);
    }

    std::shared_ptr<TestScene> scene;
};

TEST_F(ChannelAttributesTest, SetAndGetChannelAttribute) {
    scene->setChannelAttribute(0, "wavelength",    "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");
    scene->setChannelAttribute(1, "wavelength",    "561nm");
    scene->setChannelAttribute(1, "exposure_time", "150ms");

    const slideio::Metadata& attrs = scene->getChannelAttributes();
    EXPECT_EQ(attrs[0]["wavelength"].asString(),    "488nm");
    EXPECT_EQ(attrs[0]["exposure_time"].asString(), "100ms");
    EXPECT_EQ(attrs[1]["wavelength"].asString(),    "561nm");
    EXPECT_EQ(attrs[1]["exposure_time"].asString(), "150ms");
}

TEST_F(ChannelAttributesTest, SetAttributeInvalidChannelIndex) {
    EXPECT_THROW(scene->setChannelAttribute(-1, "wavelength", "488nm"), slideio::RuntimeError);
    EXPECT_THROW(scene->setChannelAttribute(3,  "wavelength", "488nm"), slideio::RuntimeError);
}

TEST_F(ChannelAttributesTest, GetAttributeAbsent) {
    scene->setChannelAttribute(0, "wavelength", "488nm");
    const slideio::Metadata& attrs = scene->getChannelAttributes();
    EXPECT_TRUE(attrs[0].contains("wavelength"));
    EXPECT_FALSE(attrs[0].contains("non_existent"));
}

TEST_F(ChannelAttributesTest, GetChannelAttributes) {
    scene->setChannelAttribute(0, "wavelength",    "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");
    scene->setChannelAttribute(0, "gain",          "2.5");

    const slideio::Metadata chan0 = scene->getChannelAttributes()[0];
    EXPECT_EQ(chan0["wavelength"].asString(),    "488nm");
    EXPECT_EQ(chan0["exposure_time"].asString(), "100ms");
    EXPECT_EQ(chan0["gain"].asString(),          "2.5");
    EXPECT_EQ(chan0.size(), 3u);
}

TEST_F(ChannelAttributesTest, MultipleChannelsDifferentAttributes) {
    scene->setChannelAttribute(0, "wavelength",    "488nm");
    scene->setChannelAttribute(0, "exposure_time", "100ms");
    scene->setChannelAttribute(0, "gain",          "2.5");
    scene->setChannelAttribute(1, "wavelength",    "561nm");
    scene->setChannelAttribute(1, "exposure_time", "150ms");
    scene->setChannelAttribute(1, "gain",          "3.0");
    scene->setChannelAttribute(2, "wavelength",    "640nm");
    scene->setChannelAttribute(2, "exposure_time", "200ms");
    scene->setChannelAttribute(2, "gain",          "3.5");

    const slideio::Metadata& attrs = scene->getChannelAttributes();
    EXPECT_EQ(attrs[0]["wavelength"].asString(),    "488nm");
    EXPECT_EQ(attrs[0]["exposure_time"].asString(), "100ms");
    EXPECT_EQ(attrs[0]["gain"].asString(),          "2.5");
    EXPECT_EQ(attrs[1]["wavelength"].asString(),    "561nm");
    EXPECT_EQ(attrs[1]["exposure_time"].asString(), "150ms");
    EXPECT_EQ(attrs[1]["gain"].asString(),          "3.0");
    EXPECT_EQ(attrs[2]["wavelength"].asString(),    "640nm");
    EXPECT_EQ(attrs[2]["exposure_time"].asString(), "200ms");
    EXPECT_EQ(attrs[2]["gain"].asString(),          "3.5");
}

TEST_F(ChannelAttributesTest, OverwriteAttributeValue) {
    scene->setChannelAttribute(0, "wavelength", "488nm");
    EXPECT_EQ(scene->getChannelAttributes()[0]["wavelength"].asString(), "488nm");
    // Re-create the scene to bypass the lazy-build freeze on getChannelAttributes().
    scene = std::make_shared<TestScene>();
    scene->setNumChannels(3);
    scene->setChannelAttribute(0, "wavelength", "561nm");
    EXPECT_EQ(scene->getChannelAttributes()[0]["wavelength"].asString(), "561nm");
}

TEST_F(ChannelAttributesTest, GetChannelAttributesTreeShape) {
    scene->defineChannelAttribute("wavelength");
    scene->defineChannelAttribute("exposure");
    scene->setChannelAttribute(0, "wavelength", "488nm");
    scene->setChannelAttribute(0, "exposure",   "100ms");
    scene->setChannelAttribute(1, "wavelength", "561nm");

    const slideio::Metadata& attrs = scene->getChannelAttributes();
    ASSERT_EQ(attrs.type(), slideio::Metadata::Type::Array);
    ASSERT_EQ(attrs.size(), 3u);                              // numChannels == 3
    EXPECT_EQ(attrs[0].type(), slideio::Metadata::Type::Object);
    EXPECT_EQ(attrs[0]["wavelength"].asString(), "488nm");
    EXPECT_EQ(attrs[0]["exposure"].asString(),   "100ms");
    EXPECT_EQ(attrs[1]["wavelength"].asString(), "561nm");
    EXPECT_FALSE(attrs[1].contains("exposure"));
    EXPECT_EQ(attrs[2].type(), slideio::Metadata::Type::Object);
    EXPECT_EQ(attrs[2].size(), 0u);                           // empty object for unset channel
}

TEST_F(ChannelAttributesTest, GetChannelAttributesTreeShapeNoAttributes) {
    // no defineChannelAttribute / setChannelAttribute calls
    const slideio::Metadata& attrs = scene->getChannelAttributes();
    ASSERT_EQ(attrs.type(), slideio::Metadata::Type::Array);
    ASSERT_EQ(attrs.size(), 3u);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(attrs[i].type(), slideio::Metadata::Type::Object);
        EXPECT_EQ(attrs[i].size(), 0u);
    }
}

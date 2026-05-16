# ZVI File Metadata Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Populate `ZVISlide::getRawMetadata()` and `ZVISlide::getMetadata()` with the full ZVI tag set from the file's Tags streams, serialized as JSON.

**Architecture:** Expand `ZVITAG` enum to cover every spec tag id and add a name lookup. Add a generic `readAllTags` helper to `ZVIUtils`. In `ZVISlide::init()`, open the OLE compound document a second time, read `/Image/Tags/Contents` and the optional root `/Tags` stream, merge tags into a `nlohmann::json` object keyed by spec name, store as `m_rawMetadata` with `MetadataFormat::JSON`, and override `buildMetadataTree()` to expose the same data as a `Metadata` tree.

**Tech Stack:** C++17, OpenCV, pole (OLE compound files), nlohmann_json, GoogleTest, CMake, Conan.

**Spec:** `software-docs/specs/2026-05-16-zvi-file-metadata-design.md`

---

## File Map

**Modify:**
- `src/slideio/drivers/zvi/zvitags.hpp` — extend `ZVITAG` enum with every spec id; declare `getZviTagName`.
- `src/slideio/drivers/zvi/zviutils.hpp` — declare `ZviTagEntry` struct + `readAllTags()`.
- `src/slideio/drivers/zvi/zviutils.cpp` — implement `readAllTags()`; extend `readItem` to handle `VT_BLOB` and `VT_STORED_OBJECT` (skip + return monostate) so unknown blob-valued tags do not throw.
- `src/slideio/drivers/zvi/zvislide.hpp` — add `buildMetadataTree` override declaration.
- `src/slideio/drivers/zvi/zvislide.cpp` — populate `m_rawMetadata` and `m_metadataFormat` in `init()`; implement `buildMetadataTree()`.
- `src/slideio/drivers/zvi/CMakeLists.txt` — add `zvitags.cpp` to the source list.
- `src/tests/main/test_zvi_driver.cpp` — update existing assertion (`None` → `JSON`); add new tests.

**Create:**
- `src/slideio/drivers/zvi/zvitags.cpp` — implementation of `getZviTagName`.

---

## Pre-flight

- [ ] **Step 0: Read the spec** — `software-docs/specs/2026-05-16-zvi-file-metadata-design.md`. Confirm scope: file-level tags only, JSON serialization, Slide-only (not Scene), unknown ids preserved as `Tag_<id>`.

- [ ] **Step 0.1: Confirm a fresh build works**

Run (from `D:/Projects/slideio/slideio`):
```
python3 install.py -a build -c release
```
Expected: clean build with no errors. Resolves any stale Conan/CMake state before edits.

---

## Task 1: Expand `ZVITAG` and add name lookup

**Files:**
- Modify: `src/slideio/drivers/zvi/zvitags.hpp`
- Create: `src/slideio/drivers/zvi/zvitags.cpp`
- Modify: `src/slideio/drivers/zvi/CMakeLists.txt`
- Test: `src/tests/main/test_zviutils.cpp`

### Step 1.1: Write the failing test

- [ ] Add to bottom of `src/tests/main/test_zviutils.cpp`:

```cpp
#include "slideio/drivers/zvi/zvitags.hpp"

TEST(ZVITags, getZviTagName_known)
{
    EXPECT_STREQ(slideio::getZviTagName(1537), "Title");
    EXPECT_STREQ(slideio::getZviTagName(1538), "Author");
    EXPECT_STREQ(slideio::getZviTagName(1553), "Filename");
    EXPECT_STREQ(slideio::getZviTagName(769),  "Scale Factor For X");
}

TEST(ZVITags, getZviTagName_unknown)
{
    EXPECT_EQ(slideio::getZviTagName(99999), nullptr);
    EXPECT_EQ(slideio::getZviTagName(0),     nullptr);
}
```

### Step 1.2: Run and verify it fails

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVITags.*"
```
Expected: compile error — `getZviTagName` undeclared. (Or the test binary fails to link if `zvitags.cpp` is referenced by CMake but doesn't exist yet.)

### Step 1.3: Replace `zvitags.hpp`

- [ ] Replace the body of `src/slideio/drivers/zvi/zvitags.hpp` with:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#ifndef OPENCV_slideio_zvitags_HPP
#define OPENCV_slideio_zvitags_HPP

#include "slideio/drivers/zvi/zvi_api_def.hpp"
#include <cstdint>

namespace slideio
{
    enum class ZVITAG : int32_t
    {
        // Existing values kept identical so ZVIScene::parseImageTags works unchanged.
        ZVITAG_COMPRESSION                     = 222,
        ZVITAG_DATE_MAPPING_TABLE              = 257,
        ZVITAG_BLACK_VALUE                     = 258,
        ZVITAG_WHITE_VALUE                     = 259,
        ZVITAG_IMAGE_DATA_MAPPING_AUTO_RANGE   = 260,
        ZVITAG_IMAGE_THUMBNAIL                 = 261,
        ZVITAG_GAMMA_VALUE                     = 262,
        ZVITAG_IMAGE_OVER_EXPOSURE             = 264,
        ZVITAG_IMAGE_RELATIVE_TIME1            = 265,
        ZVITAG_IMAGE_RELATIVE_TIME2            = 266,
        ZVITAG_IMAGE_RELATIVE_TIME3            = 267,
        ZVITAG_IMAGE_RELATIVE_TIME4            = 268,
        ZVITAG_IMAGE_RELATIVE_TIME             = 300,
        ZVITAG_IMAGE_BASE_TIME_FIRST           = 301,
        ZVITAG_IMAGE_BASE_TIME2                = 302,
        ZVITAG_IMAGE_BASE_TIME3                = 303,
        ZVITAG_IMAGE_BASE_TIME4                = 304,
        ZVITAG_IMAGE_WIDTH                     = 515,
        ZVITAG_IMAGE_HEIGHT                    = 516,
        ZVITAG_IMAGE_COUNT                     = 517,
        ZVITAG_IMAGE_PIXEL_TYPE                = 518,
        ZVITAG_NUMBER_RAW_IMAGES               = 519,
        ZVITAG_IMAGE_SIZE                      = 520,
        ZVITAG_COMPRESSION_FACTOR_FOR_SAVE     = 521,
        ZVITAG_DOCUMENT_SAVE_FLAGS             = 522,
        ZVITAG_ACQUISITION_PAUSE_ANNOTATION    = 523,
        ZVITAG_DOCUMENT_SUBTYPE                = 530,
        ZVITAG_ACQUISITION_BIT_DEPTH           = 531,
        ZVITAG_ZSTACK_SINGLE_REPRESENTATIVE    = 534,
        ZVITAG_SCALE_X                         = 769,
        ZVITAG_SCALE_UNIT_X                    = 770,
        ZVITAG_SCALE_WIDTH                     = 771,
        ZVITAG_SCALE_Y                         = 772,
        ZVITAG_SCALE_UNIT_Y                    = 773,
        ZVITAG_SCALE_HEIGHT                    = 774,
        ZVITAG_SCALE_Z                         = 775,
        ZVITAG_SCALE_UNIT_Z                    = 776,
        ZVITAG_SCALE_DEPTH                     = 777,
        ZVITAG_SCALING_PARENT                  = 778,
        ZVITAG_DATE                            = 1001,
        ZVITAG_CODE                            = 1002,
        ZVITAG_SOURCE                          = 1003,
        ZVITAG_MESSAGE                         = 1004,
        ZVITAG_CAMERA_IMAGE_ACQUISITION_TIME   = 1025,
        ZVITAG_8BIT_ACQUISITION                = 1026,
        ZVITAG_CAMERA_BIT_DEPTH                = 1027,
        ZVITAG_MONO_REFERENCE_LOW              = 1029,
        ZVITAG_MONO_REFERENCE_HIGH             = 1030,
        ZVITAG_RED_REFERENCE_LOW               = 1031,
        ZVITAG_RED_REFERENCE_HIGH              = 1032,
        ZVITAG_GREEN_REFERENCE_LOW             = 1033,
        ZVITAG_GREEN_REFERENCE_HIGH            = 1034,
        ZVITAG_BLUE_REFERENCE_LOW              = 1035,
        ZVITAG_BLUE_REFERENCE_HIGH             = 1036,
        ZVITAG_FRAMEGRABBER_NAME               = 1041,
        ZVITAG_CAMERA                          = 1042,
        ZVITAG_CAMERA_TRIGGER_SIGNAL_TYPE      = 1044,
        ZVITAG_CAMERA_TRIGGER_ENABLE           = 1045,
        ZVITAG_GRABBER_TIMEOUT                 = 1046,
        ZVITAG_MULTICHANNEL_ENABLED            = 1281,
        ZVITAG_MULTICHANNEL_COLOUR             = 1282,
        ZVITAG_MULTICHANNEL_WEIGHT             = 1283,
        ZVITAG_CHANNEL_NAME                    = 1284,
        ZVITAG_DOCUMENT_INFORMATION_GROUP      = 1536,
        ZVITAG_TITLE                           = 1537,
        ZVITAG_AUTHOR                          = 1538,
        ZVITAG_KEYWORDS                        = 1539,
        ZVITAG_COMMENTS                        = 1540,
        ZVITAG_SAMPLE_ID                       = 1541,
        ZVITAG_SUBJECT                         = 1542,
        ZVITAG_REVISION_NUMBER                 = 1543,
        ZVITAG_SAVE_FOLDER                     = 1544,
        ZVITAG_FILE_LINK                       = 1545,
        ZVITAG_DOCUMENT_TYPE                   = 1546,
        ZVITAG_STORAGE_MEDIA                   = 1547,
        ZVITAG_FILE_ID                         = 1548,
        ZVITAG_REFERENCE                       = 1549,
        ZVITAG_FILE_DATE                       = 1550,
        ZVITAG_FILE_SIZE                       = 1551,
        ZVITAG_FILE_NAME                       = 1553,
        ZVITAG_FILE_ATTRIBUTES                 = 1554,
        ZVITAG_PROJECT_GROUP                   = 1792,
        ZVITAG_ACQUISITION_DATE                = 1793,
        ZVITAG_LAST_MODIFIED_BY                = 1794,
        ZVITAG_USER_COMPANY                    = 1795,
        ZVITAG_USER_COMPANY_LOGO               = 1796,
        ZVITAG_USER_IMAGE                      = 1797,
        ZVITAG_USER_ID                         = 1800,
        ZVITAG_USER_NAME                       = 1801,
        ZVITAG_USER_CITY                       = 1802,
        ZVITAG_USER_ADDRESS                    = 1803,
        ZVITAG_USER_COUNTRY                    = 1804,
        ZVITAG_USER_PHONE                      = 1805,
        ZVITAG_USER_FAX                        = 1806,
        ZVITAG_OBJECTIVE_NAME                  = 2049,
        ZVITAG_OPTOVAR                         = 2050,
        ZVITAG_REFLECTOR                       = 2051,
        ZVITAG_CONDENSER_CONTRAST              = 2052,
        ZVITAG_TRANSMITTED_LIGHT_FILTER_1      = 2053,
        ZVITAG_TRANSMITTED_LIGHT_FILTER_2      = 2054,
        ZVITAG_REFLECTED_LIGHT_SHUTTER         = 2055,
        ZVITAG_CONDENSER_FRONT_LENS            = 2056,
        ZVITAG_EXCITATION_FILTER_NAME          = 2057,
        ZVITAG_TRANSMITTED_LIGHT_FIELDSTOP_APERTURE = 2060,
        ZVITAG_REFLECTED_LIGHT_APERTURE        = 2061,
        ZVITAG_CONDENSER_NA                    = 2062,
        ZVITAG_LIGHT_PATH                      = 2063,
        ZVITAG_HALOGEN_LAMP_ON                 = 2064,
        ZVITAG_HALOGEN_LAMP_MODE               = 2065,
        ZVITAG_HALOGEN_LAMP_VOLTAGE            = 2066,
        ZVITAG_FLUORESCENCE_LAMP_LEVEL         = 2068,
        ZVITAG_FLUORESCENCE_LAMP_INTENSITY     = 2069,
        ZVITAG_LIGHT_MANAGER_ENABLED           = 2070,
        ZVITAG_FOCUS_POSITION                  = 2072,
        ZVITAG_STAGE_POSITION_X                = 2073,
        ZVITAG_STAGE_POSITION_Y                = 2074,
        ZVITAG_MICROSCOPE_NAME                 = 2075,
        ZVITAG_MAGNIFICATION                   = 2076,
        ZVITAG_OBJECTIVE_NA                    = 2077,
        ZVITAG_MICROSCOPE_ILLUMINATION         = 2078,
        ZVITAG_EXTERNAL_SHUTTER_1              = 2079,
        ZVITAG_EXTERNAL_SHUTTER_2              = 2080,
        ZVITAG_EXTERNAL_SHUTTER_3              = 2081,
        ZVITAG_EXTERNAL_FILTER_WHEEL_1_NAME    = 2082,
        ZVITAG_EXTERNAL_FILTER_WHEEL_2_NAME    = 2083,
        ZVITAG_PARFOCAL_CORRECTION             = 2084,
        ZVITAG_EXTERNAL_SHUTTER_4              = 2086,
        ZVITAG_EXTERNAL_SHUTTER_5              = 2087,
        ZVITAG_EXTERNAL_SHUTTER_6              = 2088,
        ZVITAG_EXTERNAL_FILTER_WHEEL_3_NAME    = 2089,
        ZVITAG_EXTERNAL_FILTER_WHEEL_4_NAME    = 2090,
        ZVITAG_OBJECTIVE_TURRET_POSITION       = 2103,
        ZVITAG_OBJECTIVE_CONTRAST_METHOD       = 2104,
        ZVITAG_OBJECTIVE_IMMERSION_TYPE        = 2105,
        ZVITAG_REFLECTOR_POSITION              = 2107,
        ZVITAG_TRANSMITTED_LIGHT_FILTER_1_POS  = 2109,
        ZVITAG_TRANSMITTED_LIGHT_FILTER_2_POS  = 2110,
        ZVITAG_EXCITATION_FILTER_POSITION      = 2112,
        ZVITAG_LAMP_MIRROR_POSITION_OLD        = 2113,
        ZVITAG_EXTERNAL_FILTER_WHEEL_1_POS     = 2114,
        ZVITAG_EXTERNAL_FILTER_WHEEL_2_POS     = 2115,
        ZVITAG_EXTERNAL_FILTER_WHEEL_3_POS     = 2116,
        ZVITAG_EXTERNAL_FILTER_WHEEL_4_POS     = 2117,
        ZVITAG_LIGHTMANAGER_MODE               = 2118,
        ZVITAG_HALOGEN_LAMP_CALIBRATION        = 2119,
        ZVITAG_CONDENSER_NA_GO_SPEED           = 2120,
        ZVITAG_TLF_GO_SPEED                    = 2121,
        ZVITAG_OPTOVAR_GO_SPEED                = 2122,
        ZVITAG_FOCUS_CALIBRATED                = 2123,
        ZVITAG_FOCUS_BASIC_POSITION            = 2124,
        ZVITAG_FOCUS_POWER                     = 2125,
        ZVITAG_FOCUS_BACKLASH                  = 2126,
        ZVITAG_FOCUS_MEASUREMENT_ORIGIN        = 2127,
        ZVITAG_FOCUS_MEASUREMENT_DISTANCE      = 2128,
        ZVITAG_FOCUS_SPEED                     = 2129,
        ZVITAG_FOCUS_GO_SPEED                  = 2130,
        ZVITAG_FOCUS_DISTANCE                  = 2131,
        ZVITAG_FOCUS_INIT_POSITION             = 2132,
        ZVITAG_STAGE_CALIBRATED                = 2133,
        ZVITAG_STAGE_POWER                     = 2134,
        ZVITAG_STAGE_X_BACKLASH                = 2135,
        ZVITAG_STAGE_Y_BACKLASH                = 2136,
        ZVITAG_STAGE_SPEED_X                   = 2137,
        ZVITAG_STAGE_SPEED_Y                   = 2138,
        ZVITAG_STAGE_SPEED                     = 2139,
        ZVITAG_STAGE_GO_SPEED_X                = 2140,
        ZVITAG_STAGE_GO_SPEED_Y                = 2141,
        ZVITAG_STAGE_STEP_DISTANCE_X           = 2142,
        ZVITAG_STAGE_STEP_DISTANCE_Y           = 2143,
        ZVITAG_STAGE_INIT_POSITION_X           = 2144,
        ZVITAG_STAGE_INIT_POSITION_Y           = 2145,
        ZVITAG_MICROSCOPE_MAGNIFICATION        = 2146,
        ZVITAG_REFLECTOR_MAGNIFICATION         = 2147,
        ZVITAG_LAMP_MIRROR_POSITION            = 2148,
        ZVITAG_FOCUS_DEPTH                     = 2149,
        ZVITAG_MICROSCOPE_TYPE                 = 2150,
        ZVITAG_OBJECTIVE_WORKING_DISTANCE      = 2151,
        ZVITAG_REFLECTED_LIGHT_APERTURE_GO_SPEED = 2152,
        ZVITAG_EXTERNAL_SHUTTER                = 2153,
        ZVITAG_OBJECTIVE_IMMERSION_STOP        = 2154,
        ZVITAG_FOCUS_START_SPEED               = 2155,
        ZVITAG_FOCUS_ACCELERATION              = 2156,
        ZVITAG_REFLECTED_LIGHT_FIELDSTOP       = 2157,
        ZVITAG_REFLECTED_LIGHT_FIELDSTOP_GO_SPEED = 2158,
        ZVITAG_REFLECTED_LIGHT_FILTER_1        = 2159,
        ZVITAG_REFLECTED_LIGHT_FILTER_2        = 2160,
        ZVITAG_REFLECTED_LIGHT_FILTER_1_POS    = 2161,
        ZVITAG_REFLECTED_LIGHT_FILTER_2_POS    = 2162,
        ZVITAG_TRANSMITTED_LIGHT_ATTENUATOR    = 2163,
        ZVITAG_REFLECTED_LIGHT_ATTENUATOR      = 2164,
        ZVITAG_TRANSMITTED_LIGHT_SHUTTER       = 2165,
        ZVITAG_TLA_GO_SPEED                    = 2166,
        ZVITAG_RLA_GO_SPEED                    = 2167,
        ZVITAG_TLV_FILTER_POSITION             = 2176,
        ZVITAG_TLV_FILTER                      = 2177,
        ZVITAG_RLV_FILTER_POSITION             = 2178,
        ZVITAG_RLV_FILTER                      = 2179,
        ZVITAG_REFLECTED_LIGHT_HALOGEN_MODE    = 2180,
        ZVITAG_REFLECTED_LIGHT_HALOGEN_VOLTAGE = 2181,
        ZVITAG_REFLECTED_LIGHT_HALOGEN_COLOR_TEMP = 2182,
        ZVITAG_CONTRASTMANAGER_MODE            = 2183,
        ZVITAG_DAZZLE_PROTECTION_ACTIVE        = 2184,
        ZVITAG_ZOOM                            = 2195,
        ZVITAG_ZOOM_GO_SPEED                   = 2196,
        ZVITAG_LIGHT_ZOOM                      = 2197,
        ZVITAG_LIGHT_ZOOM_GO_SPEED             = 2198,
        ZVITAG_LIGHTZOOM_COUPLED               = 2199,
        ZVITAG_TRANSMITTED_LIGHT_HALOGEN_MODE  = 2200,
        ZVITAG_TRANSMITTED_LIGHT_HALOGEN_VOLTAGE = 2201,
        ZVITAG_TRANSMITTED_LIGHT_HALOGEN_COLOR_TEMP = 2202,
        ZVITAG_REFLECTED_COLDLIGHT_MODE        = 2203,
        ZVITAG_REFLECTED_COLDLIGHT_INTENSITY   = 2204,
        ZVITAG_REFLECTED_COLDLIGHT_COLOR_TEMP  = 2205,
        ZVITAG_TRANSMITTED_COLDLIGHT_MODE      = 2206,
        ZVITAG_TRANSMITTED_COLDLIGHT_INTENSITY = 2207,
        ZVITAG_TRANSMITTED_COLDLIGHT_COLOR_TEMP = 2208,
        ZVITAG_INFINITYSPACE_PORTCHANGER_POSITION = 2209,
        ZVITAG_BEAMSPLITTER_INFINITY_SPACE     = 2210,
        ZVITAG_TWOTV_VISCAMCHANGER_POSITION    = 2211,
        ZVITAG_BEAMSPLITTER_OCULAR             = 2212,
        ZVITAG_TWOTV_CAMERASCHANGER_POSITION   = 2213,
        ZVITAG_BEAMSPLITTER_CAMERAS            = 2214,
        ZVITAG_OCULAR_SHUTTER                  = 2215,
        ZVITAG_TWOTV_CAMERASCHANGER_CUBE       = 2216,
        ZVITAG_LIGHT_WAVELENGTH                = 2217,
        ZVITAG_OCULAR_MAGNIFICATION            = 2218,
        ZVITAG_CAMERA_ADAPTER_MAGNIFICATION    = 2219,
        ZVITAG_MICROSCOPE_PORT                 = 2220,
        ZVITAG_OCULAR_TOTAL_MAGNIFICATION      = 2221,
        ZVITAG_FIELD_OF_VIEW                   = 2222,
        ZVITAG_OCULAR                          = 2223,
        ZVITAG_CAMERA_ADAPTER                  = 2224,
        ZVITAG_STAGE_JOYSTICK_ENABLED          = 2225,
        ZVITAG_CONTRASTMANAGER_CONTRAST_METHOD = 2226,
        ZVITAG_CAMERASCHANGER_BEAMSPLITTER_TYPE = 2229,
        ZVITAG_REARPORT_SLIDER_POSITION        = 2235,
        ZVITAG_REARPORT_SOURCE                 = 2236,
        ZVITAG_BEAMSPLITTER_TYPE_INFINITY_SPACE = 2237,
        ZVITAG_FLUORESCENCE_ATTENUATOR         = 2238,
        ZVITAG_FLUORESCENCE_ATTENUATOR_POSITION = 2239,
        ZVITAG_CAMERA_FRAMESTART_LEFT          = 2307,
        ZVITAG_CAMERA_FRAMESTART_TOP           = 2308,
        ZVITAG_CAMERA_FRAME_WIDTH              = 2309,
        ZVITAG_CAMERA_FRAME_HEIGHT             = 2310,
        ZVITAG_CAMERA_BINNING                  = 2311,
        ZVITAG_CAMERA_FRAME_FULL               = 2312,
        ZVITAG_CAMERA_FRAME_PIXEL_DISTANCE     = 2313,
        ZVITAG_DATA_FORMAT_USE_SCALING         = 2318,
        ZVITAG_CAMERA_FRAME_IMAGE_ORIENTATION  = 2319,
        ZVITAG_VIDEO_MONOCHROME_SIGNAL_TYPE    = 2320,
        ZVITAG_VIDEO_COLOR_SIGNAL_TYPE         = 2321,
        ZVITAG_METEOR_CHANNEL_INPUT            = 2322,
        ZVITAG_METEOR_CHANNEL_SYNC             = 2323,
        ZVITAG_WHITE_BALANCE_ENABLED           = 2324,
        ZVITAG_CAMERA_WHITE_BALANCE_RED        = 2325,
        ZVITAG_CAMERA_WHITE_BALANCE_GREEN      = 2326,
        ZVITAG_CAMERA_WHITE_BALANCE_BLUE       = 2327,
        ZVITAG_CAMERA_FRAME_SCALING_FACTOR     = 2331,
        ZVITAG_METEOR_CAMERA_TYPE              = 2562,
        ZVITAG_EXPOSURE_TIME                   = 2564,
        ZVITAG_CAMERA_EXPOSURE_TIME_AUTO_CALC  = 2568,
        ZVITAG_METEOR_GAIN_VALUE               = 2569,
        ZVITAG_METEOR_GAIN_AUTOMATIC           = 2571,
        ZVITAG_METEOR_ADJUST_HUE               = 2572,
        ZVITAG_METEOR_ADJUST_SATURATION        = 2573,
        ZVITAG_METEOR_ADJUST_RED_LOW           = 2574,
        ZVITAG_METEOR_ADJUST_GREEN_LOW         = 2575,
        ZVITAG_METEOR_BLUE_LOW                 = 2576,
        ZVITAG_METEOR_ADJUST_RED_HIGH          = 2577,
        ZVITAG_METEOR_ADJUST_GREEN_HIGH        = 2578,
        ZVITAG_METEOR_BLUE_HIGH                = 2579,
        ZVITAG_CAMERA_EXPOSURE_TIME_CALC_CONTROL = 2582,
        ZVITAG_AXIOCAM_FADING_CORRECTION_ENABLE = 2585,
        ZVITAG_CAMERA_LIVE_IMAGE               = 2587,
        ZVITAG_CAMERA_LIVE_ENABLED             = 2588,
        ZVITAG_LIVE_IMAGE_SYNC_OBJECT_NAME     = 2589,
        ZVITAG_CAMERA_LIVE_SPEED               = 2590,
        ZVITAG_CAMERA_IMAGE                    = 2591,
        ZVITAG_CAMERA_IMAGE_WIDTH              = 2592,
        ZVITAG_CAMERA_IMAGE_HEIGHT             = 2593,
        ZVITAG_CAMERA_IMAGE_PIXEL_TYPE         = 2594,
        ZVITAG_CAMERA_IMAGE_SH_MEMORY_NAME     = 2595,
        ZVITAG_CAMERA_LIVE_IMAGE_WIDTH         = 2596,
        ZVITAG_CAMERA_LIVE_IMAGE_HEIGHT        = 2597,
        ZVITAG_CAMERA_LIVE_IMAGE_PIXEL_TYPE    = 2598,
        ZVITAG_CAMERA_LIVE_IMAGE_SH_MEMORY_NAME = 2599,
        ZVITAG_CAMERA_LIVE_MAXIMUM_SPEED       = 2600,
        ZVITAG_CAMERA_LIVE_BINNING             = 2601,
        ZVITAG_CAMERA_LIVE_GAIN_VALUE          = 2602,
        ZVITAG_CAMERA_LIVE_EXPOSURE_TIME_VALUE = 2603,
        ZVITAG_CAMERA_LIVE_SCALING_FACTOR      = 2604,
        ZVITAG_IMAGE_INDEX_U                   = 2817,
        ZVITAG_IMAGE_INDEX_V                   = 2818,
        ZVITAG_IMAGE_INDEX_Z                   = 2819,
        ZVITAG_IMAGE_INDEX_C                   = 2820,
        ZVITAG_IMAGE_INDEX_T                   = 2821,
        ZVITAG_IMAGE_TILE_INDEX                = 2822,
        ZVITAG_IMAGE_ACQUSITION_INDEX          = 2823,
        ZVITAG_IMAGE_COUNT_TILES               = 2824,
        ZVITAG_IMAGE_COUNT_A                   = 2825,
        ZVITAG_IMAGE_INDEX_S                   = 2827,
        ZVITAG_IMAGE_INDEX_RAW                 = 2828,
        ZVITAG_IMAGE_COUNT_Z                   = 2832,
        ZVITAG_IMAGE_COUNT_C                   = 2833,
        ZVITAG_IMAGE_COUNT_T                   = 2834,
        ZVITAG_IMAGE_COUNT_U                   = 2838,
        ZVITAG_IMAGE_COUNT_V                   = 2839,
        ZVITAG_IMAGE_COUNT_S                   = 2840,
        ZVITAG_ORIGINAL_STAGE_POSITION_X       = 2841,
        ZVITAG_ORIGINAL_STAGE_POSITION_Y       = 2842,
        ZVITAG_LAYER_DRAW_FLAGS                = 3088,
        ZVITAG_REMAINING_TIME                  = 3334,
        ZVITAG_USER_FIELD_1                    = 3585,
        ZVITAG_USER_FIELD_2                    = 3586,
        ZVITAG_USER_FIELD_3                    = 3587,
        ZVITAG_USER_FIELD_4                    = 3588,
        ZVITAG_USER_FIELD_5                    = 3589,
        ZVITAG_USER_FIELD_6                    = 3590,
        ZVITAG_USER_FIELD_7                    = 3591,
        ZVITAG_USER_FIELD_8                    = 3592,
        ZVITAG_USER_FIELD_9                    = 3593,
        ZVITAG_USER_FIELD_10                   = 3594,
        ZVITAG_ID                              = 3840,
        ZVITAG_NAME                            = 3841,
        ZVITAG_VALUE                           = 3842,
        ZVITAG_PVCAM_CLOCKING_MODE             = 5501,
        ZVITAG_AUTOFOCUS_STATUS_REPORT         = 8193,
        ZVITAG_AUTOFOCUS_POSITION              = 8194,
        ZVITAG_AUTOFOCUS_POSITION_OFFSET       = 8195,
        ZVITAG_AUTOFOCUS_EMPTY_FIELD_THRESHOLD = 8196,
        ZVITAG_AUTOFOCUS_CALIBRATION_NAME      = 8197,
        ZVITAG_AUTOFOCUS_CURRENT_CALIBRATION_ITEM = 8198,
        ZVITAG_CAMERA_FRAME_FULL_WIDTH         = 65537,
        ZVITAG_CAMERA_FRAME_FULL_HEIGHT        = 65538,
        ZVITAG_AXIOCAM_SHUTTER_SIGNAL          = 65541,
        ZVITAG_AXIOCAM_DELAY_TIME              = 65542,
        ZVITAG_AXIOCAM_SHUTTER_CONTROL         = 65543,
        ZVITAG_AXIOCAM_BLACK_REF_IS_CALCULATED = 65544,
        ZVITAG_AXIOCAM_BLACK_REFERENCE         = 65545,
        ZVITAG_CAMERA_SHADING_CORRECTION       = 65547,
        ZVITAG_AXIOCAM_ENHANCE_COLOR           = 65550,
        ZVITAG_AXIOCAM_NIR_MODE                = 65551,
        ZVITAG_CAMERA_SHUTTER_CLOSE_DELAY      = 65552,
        ZVITAG_CAMERA_WHITE_BALANCE_AUTO_CALC  = 65553,
        ZVITAG_AXIOCAM_NIR_MODE_AVAILABLE      = 65556,
        ZVITAG_AXIOCAM_FADING_CORRECTION_AVAILABLE = 65557,
        ZVITAG_AXIOCAM_ENHANCE_COLOR_AVAILABLE = 65559,
        ZVITAG_METEOR_VIDEO_NORM               = 65565,
        ZVITAG_METEOR_ADJUST_WHITE_REFERENCE   = 65566,
        ZVITAG_METEOR_BLACK_REFERENCE          = 65567,
        ZVITAG_METEOR_CHANNEL_INPUT_COUNT_MONO = 65568,
        ZVITAG_METEOR_CHANNEL_INPUT_COUNT_RGB  = 65570,
        ZVITAG_METEOR_ENABLE_VCR               = 65571,
        ZVITAG_METEOR_BRIGHTNESS               = 65572,
        ZVITAG_METEOR_CONTRAST                 = 65573,
        ZVITAG_AXIOCAM_SELECTOR                = 65575,
        ZVITAG_AXIOCAM_TYPE                    = 65576,
        ZVITAG_AXIOCAM_INFO                    = 65577,
        ZVITAG_AXIOCAM_RESOLUTION              = 65580,
        ZVITAG_AXIOCAM_COLOUR_MODEL            = 65581,
        ZVITAG_AXIOCAM_MICROSCANNING           = 65582,
        ZVITAG_AMPLIFICATION_INDEX             = 65585,
        ZVITAG_DEVICE_COMMAND                  = 65586,
        ZVITAG_BEAM_LOCATION                   = 65587,
        ZVITAG_COMPONENT_TYPE                  = 65588,
        ZVITAG_CONTROLLER_TYPE                 = 65589,
        ZVITAG_CAMERA_WB_CALC_RED_PAINT        = 65590,
        ZVITAG_CAMERA_WB_CALC_BLUE_PAINT       = 65591,
        ZVITAG_CAMERA_WB_SET_RED               = 65592,
        ZVITAG_CAMERA_WB_SET_GREEN             = 65593,
        ZVITAG_CAMERA_WB_SET_BLUE              = 65594,
        ZVITAG_CAMERA_WB_SET_TARGET_RED        = 65595,
        ZVITAG_CAMERA_WB_SET_TARGET_GREEN      = 65596,
        ZVITAG_CAMERA_WB_SET_TARGET_BLUE       = 65597,
        ZVITAG_APOTOMECAM_CALIBRATION_MODE     = 65598,
        ZVITAG_APOTOME_GRID_POSITION           = 65599,
        ZVITAG_APOTOMECAM_SCANNER_POSITION     = 65600,
        ZVITAG_APOTOME_FULL_PHASE_SHIFT        = 65601,
        ZVITAG_APOTOME_GRID_NAME               = 65602,
        ZVITAG_APOTOME_STAINING                = 65603,
        ZVITAG_APOTOME_PROCESSING_MODE         = 65604,
        ZVITAG_APOTOMECAM_LIVE_COMBINE_MODE    = 65605,
        ZVITAG_APOTOME_FILTER_NAME             = 65606,
        ZVITAG_APOTOME_FILTER_STRENGTH         = 65607,
        ZVITAG_APOTOMECAM_FILTER_HARMONICS     = 65608,
        ZVITAG_APOTOME_GRATING_PERIOD          = 65609,
        ZVITAG_APOTOME_AUTO_SHUTTER_USED       = 65610,
        ZVITAG_APOTOMECAM_STATUS               = 65611,
        ZVITAG_APOTOMECAM_NORMALIZE            = 65612,
        ZVITAG_APOTOMECAM_SETTINGS_MANAGER     = 65613,
        ZVITAG_DEEPVIEWCAM_SUPERVISOR_MODE     = 65614,
        ZVITAG_DEEPVIEW_PROCESSING             = 65615,
        ZVITAG_DEEPVIEWCAM_FILTER_NAME         = 65616,
        ZVITAG_DEEPVIEWCAM_STATUS              = 65617,
        ZVITAG_DEEPVIEWCAM_SETTINGS_MANAGER    = 65618,
        ZVITAG_DEVICE_SCALING_NAME             = 65619,
        ZVITAG_CAMERA_SHADING_IS_CALCULATED    = 65620,
        ZVITAG_CAMERA_SHADING_CALCULATION_NAME = 65621,
        ZVITAG_CAMERA_SHADING_AUTO_CALCULATE   = 65622,
        ZVITAG_CAMERA_TRIGGER_AVAILABLE        = 65623,
        ZVITAG_CAMERA_SHUTTER_AVAILABLE        = 65626,
        ZVITAG_AXIOCAM_SHUTTER_MICROSCAN_ENABLE = 65627,
        ZVITAG_APOTOMECAM_LIVE_FOCUS           = 65628,
        ZVITAG_DEVICE_INIT_STATUS              = 65629,
        ZVITAG_DEVICE_ERROR_STATUS             = 65630,
        ZVITAG_APOTOMECAM_SLIDER_IN_GRID_POSITION = 65631,
        ZVITAG_ORCA_NIR_MODE_USED              = 65632,
        ZVITAG_ORCA_ANALOG_GAIN                = 65633,
        ZVITAG_ORCA_ANALOG_OFFSET              = 65634,
        ZVITAG_ORCA_BINNING                    = 65635,
        ZVITAG_ORCA_BIT_DEPTH                  = 65636,
        ZVITAG_APOTOME_AVERAGING_COUNT         = 65637,
        ZVITAG_DEEPVIEW_DOF                    = 65638,
        ZVITAG_DEEPVIEW_EDOF                   = 65639,
        ZVITAG_DEEPVIEW_SLIDER_NAME            = 65643,
    };

    // Returns the spec name (Section 3.4 of ZVI Format Spec V 2.0.4) for a known
    // tag id, or nullptr if the id is not in the table.
    SLIDEIO_ZVI_EXPORTS const char* getZviTagName(int32_t id);
}

#endif
```

### Step 1.4: Create `zvitags.cpp`

- [ ] Create `src/slideio/drivers/zvi/zvitags.cpp` with one big `switch` returning the spec name for each id. The pattern repeats for every enumerator above:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvitags.hpp"

namespace slideio {

const char* getZviTagName(int32_t id)
{
    switch (static_cast<ZVITAG>(id)) {
    case ZVITAG::ZVITAG_COMPRESSION:                       return "Compression";
    case ZVITAG::ZVITAG_DATE_MAPPING_TABLE:                return "DateMappingTable";
    case ZVITAG::ZVITAG_BLACK_VALUE:                       return "Black Value";
    case ZVITAG::ZVITAG_WHITE_VALUE:                       return "White value";
    case ZVITAG::ZVITAG_IMAGE_DATA_MAPPING_AUTO_RANGE:     return "ImageDataMappingAutoRange";
    case ZVITAG::ZVITAG_IMAGE_THUMBNAIL:                   return "Image Thumbnail";
    case ZVITAG::ZVITAG_GAMMA_VALUE:                       return "Gamma Value";
    case ZVITAG::ZVITAG_IMAGE_OVER_EXPOSURE:               return "ImageOverExposure";
    case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME1:              return "ImageRelativeTime1";
    case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME2:              return "ImageRelativeTime2";
    case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME3:              return "ImageRelativeTime3";
    case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME4:              return "ImageRelativeTime4";
    case ZVITAG::ZVITAG_IMAGE_RELATIVE_TIME:               return "ImageRelativeTime";
    case ZVITAG::ZVITAG_IMAGE_BASE_TIME_FIRST:             return "ImageBaseTimeFirst";
    case ZVITAG::ZVITAG_IMAGE_BASE_TIME2:                  return "ImageBaseTime2";
    case ZVITAG::ZVITAG_IMAGE_BASE_TIME3:                  return "ImageBaseTime3";
    case ZVITAG::ZVITAG_IMAGE_BASE_TIME4:                  return "ImageBaseTime4";
    case ZVITAG::ZVITAG_IMAGE_WIDTH:                       return "Image Width (Pixel)";
    case ZVITAG::ZVITAG_IMAGE_HEIGHT:                      return "Image Height (Pixel)";
    case ZVITAG::ZVITAG_IMAGE_COUNT:                       return "ImageCountRaw";
    case ZVITAG::ZVITAG_IMAGE_PIXEL_TYPE:                  return "Pixel Type";
    case ZVITAG::ZVITAG_NUMBER_RAW_IMAGES:                 return "Number Raw Images";
    case ZVITAG::ZVITAG_IMAGE_SIZE:                        return "Image Size";
    case ZVITAG::ZVITAG_COMPRESSION_FACTOR_FOR_SAVE:       return "CompressionFactorForSave";
    case ZVITAG::ZVITAG_DOCUMENT_SAVE_FLAGS:               return "DocumentSaveFlags";
    case ZVITAG::ZVITAG_ACQUISITION_PAUSE_ANNOTATION:      return "Acquisition pause annotation";
    case ZVITAG::ZVITAG_DOCUMENT_SUBTYPE:                  return "Document Subtype";
    case ZVITAG::ZVITAG_ACQUISITION_BIT_DEPTH:             return "Acquisition Bit Depth";
    case ZVITAG::ZVITAG_ZSTACK_SINGLE_REPRESENTATIVE:      return "Z-Stack single representative";
    case ZVITAG::ZVITAG_SCALE_X:                           return "Scale Factor For X";
    case ZVITAG::ZVITAG_SCALE_UNIT_X:                      return "Scale Unit for X";
    case ZVITAG::ZVITAG_SCALE_WIDTH:                       return "Scale Width";
    case ZVITAG::ZVITAG_SCALE_Y:                           return "Scale Factor For Y";
    case ZVITAG::ZVITAG_SCALE_UNIT_Y:                      return "Scale Unit for Y";
    case ZVITAG::ZVITAG_SCALE_HEIGHT:                      return "Scale Height";
    case ZVITAG::ZVITAG_SCALE_Z:                           return "Scale Factor For Z";
    case ZVITAG::ZVITAG_SCALE_UNIT_Z:                      return "Scale Unit for Z";
    case ZVITAG::ZVITAG_SCALE_DEPTH:                       return "Scale Depth";
    case ZVITAG::ZVITAG_SCALING_PARENT:                    return "Scaling Parent";
    case ZVITAG::ZVITAG_DATE:                              return "Date";
    case ZVITAG::ZVITAG_CODE:                              return "Code";
    case ZVITAG::ZVITAG_SOURCE:                            return "Source";
    case ZVITAG::ZVITAG_MESSAGE:                           return "Message";
    case ZVITAG::ZVITAG_CAMERA_IMAGE_ACQUISITION_TIME:     return "CameraImageAcquisitionTime";
    case ZVITAG::ZVITAG_8BIT_ACQUISITION:                  return "8-bit Acquisition";
    case ZVITAG::ZVITAG_CAMERA_BIT_DEPTH:                  return "Camera Bit Depth";
    case ZVITAG::ZVITAG_MONO_REFERENCE_LOW:                return "MonoReferenceLow";
    case ZVITAG::ZVITAG_MONO_REFERENCE_HIGH:               return "MonoReferenceHigh";
    case ZVITAG::ZVITAG_RED_REFERENCE_LOW:                 return "RedReferenceLow";
    case ZVITAG::ZVITAG_RED_REFERENCE_HIGH:                return "RedReferenceHigh";
    case ZVITAG::ZVITAG_GREEN_REFERENCE_LOW:               return "GreenReferenceLow";
    case ZVITAG::ZVITAG_GREEN_REFERENCE_HIGH:              return "GreenReferenceHigh";
    case ZVITAG::ZVITAG_BLUE_REFERENCE_LOW:                return "BlueReferenceLow";
    case ZVITAG::ZVITAG_BLUE_REFERENCE_HIGH:               return "BlueReferenceHigh";
    case ZVITAG::ZVITAG_FRAMEGRABBER_NAME:                 return "Framegrabber Name";
    case ZVITAG::ZVITAG_CAMERA:                            return "Camera";
    case ZVITAG::ZVITAG_CAMERA_TRIGGER_SIGNAL_TYPE:        return "CameraTriggerSignalType";
    case ZVITAG::ZVITAG_CAMERA_TRIGGER_ENABLE:             return "CameraTriggerEnable";
    case ZVITAG::ZVITAG_GRABBER_TIMEOUT:                   return "GrabberTimeout";
    case ZVITAG::ZVITAG_MULTICHANNEL_ENABLED:              return "MultiChannelEnabled";
    case ZVITAG::ZVITAG_MULTICHANNEL_COLOUR:               return "Multichannel Colour";
    case ZVITAG::ZVITAG_MULTICHANNEL_WEIGHT:               return "Multichannel Weight";
    case ZVITAG::ZVITAG_CHANNEL_NAME:                      return "Channel Name";
    case ZVITAG::ZVITAG_DOCUMENT_INFORMATION_GROUP:        return "DocumentInformationGroup";
    case ZVITAG::ZVITAG_TITLE:                             return "Title";
    case ZVITAG::ZVITAG_AUTHOR:                            return "Author";
    case ZVITAG::ZVITAG_KEYWORDS:                          return "Keywords";
    case ZVITAG::ZVITAG_COMMENTS:                          return "Comments";
    case ZVITAG::ZVITAG_SAMPLE_ID:                         return "Sample ID";
    case ZVITAG::ZVITAG_SUBJECT:                           return "Subject";
    case ZVITAG::ZVITAG_REVISION_NUMBER:                   return "RevisionNumber";
    case ZVITAG::ZVITAG_SAVE_FOLDER:                       return "Save Folder";
    case ZVITAG::ZVITAG_FILE_LINK:                         return "FileLink";
    case ZVITAG::ZVITAG_DOCUMENT_TYPE:                     return "Document Type";
    case ZVITAG::ZVITAG_STORAGE_MEDIA:                     return "Storage Media";
    case ZVITAG::ZVITAG_FILE_ID:                           return "File ID";
    case ZVITAG::ZVITAG_REFERENCE:                         return "Reference";
    case ZVITAG::ZVITAG_FILE_DATE:                         return "File Date";
    case ZVITAG::ZVITAG_FILE_SIZE:                         return "File Size";
    case ZVITAG::ZVITAG_FILE_NAME:                         return "Filename";
    case ZVITAG::ZVITAG_FILE_ATTRIBUTES:                   return "FileAttributes";
    case ZVITAG::ZVITAG_PROJECT_GROUP:                     return "ProjectGroup";
    case ZVITAG::ZVITAG_ACQUISITION_DATE:                  return "Acquisition Date";
    case ZVITAG::ZVITAG_LAST_MODIFIED_BY:                  return "Last modified by";
    case ZVITAG::ZVITAG_USER_COMPANY:                      return "User Company";
    case ZVITAG::ZVITAG_USER_COMPANY_LOGO:                 return "User Company Logo";
    case ZVITAG::ZVITAG_USER_IMAGE:                        return "Image";
    case ZVITAG::ZVITAG_USER_ID:                           return "User ID";
    case ZVITAG::ZVITAG_USER_NAME:                         return "User Name";
    case ZVITAG::ZVITAG_USER_CITY:                         return "User City";
    case ZVITAG::ZVITAG_USER_ADDRESS:                      return "User Address";
    case ZVITAG::ZVITAG_USER_COUNTRY:                      return "User Country";
    case ZVITAG::ZVITAG_USER_PHONE:                        return "User Phone";
    case ZVITAG::ZVITAG_USER_FAX:                          return "User Fax";
    case ZVITAG::ZVITAG_OBJECTIVE_NAME:                    return "Objective Name";
    case ZVITAG::ZVITAG_OPTOVAR:                           return "Optovar";
    case ZVITAG::ZVITAG_REFLECTOR:                         return "Reflector";
    case ZVITAG::ZVITAG_CONDENSER_CONTRAST:                return "Condenser Contrast";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_1:        return "Transmitted Light Filter 1";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_2:        return "Transmitted Light Filter 2";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_SHUTTER:           return "Reflected Light Shutter";
    case ZVITAG::ZVITAG_CONDENSER_FRONT_LENS:              return "Condenser Front Lens";
    case ZVITAG::ZVITAG_EXCITATION_FILTER_NAME:            return "Excitation Filer Name";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FIELDSTOP_APERTURE: return "Transmitted Light Fieldstop Aperture";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_APERTURE:          return "Reflected Light Aperture";
    case ZVITAG::ZVITAG_CONDENSER_NA:                      return "Condenser N.A.";
    case ZVITAG::ZVITAG_LIGHT_PATH:                        return "Light Path";
    case ZVITAG::ZVITAG_HALOGEN_LAMP_ON:                   return "HalogenLampOn";
    case ZVITAG::ZVITAG_HALOGEN_LAMP_MODE:                 return "Halogen Lamp Mode";
    case ZVITAG::ZVITAG_HALOGEN_LAMP_VOLTAGE:              return "Halogen Lamp Voltage";
    case ZVITAG::ZVITAG_FLUORESCENCE_LAMP_LEVEL:           return "Fluorescence Lamp Level";
    case ZVITAG::ZVITAG_FLUORESCENCE_LAMP_INTENSITY:       return "Fluorescence Lamp Intensity";
    case ZVITAG::ZVITAG_LIGHT_MANAGER_ENABLED:             return "Light Manager is Enabled";
    case ZVITAG::ZVITAG_FOCUS_POSITION:                    return "Focus Position";
    case ZVITAG::ZVITAG_STAGE_POSITION_X:                  return "Stage Position X";
    case ZVITAG::ZVITAG_STAGE_POSITION_Y:                  return "Stage Position Y";
    case ZVITAG::ZVITAG_MICROSCOPE_NAME:                   return "Microscope Name";
    case ZVITAG::ZVITAG_MAGNIFICATION:                     return "Objective Magnification";
    case ZVITAG::ZVITAG_OBJECTIVE_NA:                      return "Objective N.A.";
    case ZVITAG::ZVITAG_MICROSCOPE_ILLUMINATION:           return "Microscope Illumination";
    case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_1:                return "External Shutter 1";
    case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_2:                return "External Shutter 2";
    case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_3:                return "External Shutter 3";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_1_NAME:      return "External Filter Wheel 1 Name";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_2_NAME:      return "External Filter Wheel 2 Name";
    case ZVITAG::ZVITAG_PARFOCAL_CORRECTION:               return "Parfocal Correction";
    case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_4:                return "External Shutter 4";
    case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_5:                return "External Shutter 5";
    case ZVITAG::ZVITAG_EXTERNAL_SHUTTER_6:                return "External Shutter 6";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_3_NAME:      return "External Filter Wheel 3 Name";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_4_NAME:      return "External Filter Wheel 4 Name";
    case ZVITAG::ZVITAG_OBJECTIVE_TURRET_POSITION:         return "Objective Turret Position";
    case ZVITAG::ZVITAG_OBJECTIVE_CONTRAST_METHOD:         return "Objective Contrast Method";
    case ZVITAG::ZVITAG_OBJECTIVE_IMMERSION_TYPE:          return "Objective Immersion Type";
    case ZVITAG::ZVITAG_REFLECTOR_POSITION:                return "Reflector Position";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_1_POS:    return "Transmitted Light Filter 1 Position";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_FILTER_2_POS:    return "Transmitted Light Filter 2 Position";
    case ZVITAG::ZVITAG_EXCITATION_FILTER_POSITION:        return "Excitation Filter Position";
    case ZVITAG::ZVITAG_LAMP_MIRROR_POSITION_OLD:          return "Lamp Mirror Position (ERSETZT DURCH 241!)";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_1_POS:       return "External Filter Wheel 1 Position";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_2_POS:       return "External Filter Wheel 2 Position";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_3_POS:       return "External Filter Wheel 3 Position";
    case ZVITAG::ZVITAG_EXTERNAL_FILTER_WHEEL_4_POS:       return "External Filter Wheel 4 Position";
    case ZVITAG::ZVITAG_LIGHTMANAGER_MODE:                 return "Lightmanager Mode";
    case ZVITAG::ZVITAG_HALOGEN_LAMP_CALIBRATION:          return "Halogen Lamp Calibration";
    case ZVITAG::ZVITAG_CONDENSER_NA_GO_SPEED:             return "CondenserNAGoSpeed";
    case ZVITAG::ZVITAG_TLF_GO_SPEED:                      return "TransmittedLightFieldstopGoSpeed";
    case ZVITAG::ZVITAG_OPTOVAR_GO_SPEED:                  return "OptovarGoSpeed";
    case ZVITAG::ZVITAG_FOCUS_CALIBRATED:                  return "Focus calibrated";
    case ZVITAG::ZVITAG_FOCUS_BASIC_POSITION:              return "FocusBasicPosition";
    case ZVITAG::ZVITAG_FOCUS_POWER:                       return "FocusPower";
    case ZVITAG::ZVITAG_FOCUS_BACKLASH:                    return "FocusBacklash";
    case ZVITAG::ZVITAG_FOCUS_MEASUREMENT_ORIGIN:          return "FocusMeasurementOrigin";
    case ZVITAG::ZVITAG_FOCUS_MEASUREMENT_DISTANCE:        return "FocusMeasurementDistance";
    case ZVITAG::ZVITAG_FOCUS_SPEED:                       return "FocusSpeed";
    case ZVITAG::ZVITAG_FOCUS_GO_SPEED:                    return "FocusGoSpeed";
    case ZVITAG::ZVITAG_FOCUS_DISTANCE:                    return "Focus Distance";
    case ZVITAG::ZVITAG_FOCUS_INIT_POSITION:               return "FocusInitPosition";
    case ZVITAG::ZVITAG_STAGE_CALIBRATED:                  return "Stage calibrated";
    case ZVITAG::ZVITAG_STAGE_POWER:                       return "StagePower";
    case ZVITAG::ZVITAG_STAGE_X_BACKLASH:                  return "StageXBacklash";
    case ZVITAG::ZVITAG_STAGE_Y_BACKLASH:                  return "StageYBacklash";
    case ZVITAG::ZVITAG_STAGE_SPEED_X:                     return "Stage Speed X";
    case ZVITAG::ZVITAG_STAGE_SPEED_Y:                     return "Stage Speed Y";
    case ZVITAG::ZVITAG_STAGE_SPEED:                       return "Stage Speed";
    case ZVITAG::ZVITAG_STAGE_GO_SPEED_X:                  return "Stage Go Speed X";
    case ZVITAG::ZVITAG_STAGE_GO_SPEED_Y:                  return "Stage Go Speed Y";
    case ZVITAG::ZVITAG_STAGE_STEP_DISTANCE_X:             return "Stage Step Distance X";
    case ZVITAG::ZVITAG_STAGE_STEP_DISTANCE_Y:             return "Stage Step Distance Y";
    case ZVITAG::ZVITAG_STAGE_INIT_POSITION_X:             return "Stage Initialisation Position X";
    case ZVITAG::ZVITAG_STAGE_INIT_POSITION_Y:             return "Stage Initialisation Position Y";
    case ZVITAG::ZVITAG_MICROSCOPE_MAGNIFICATION:          return "MicroscopeMagnification";
    case ZVITAG::ZVITAG_REFLECTOR_MAGNIFICATION:           return "Reflector Magnification";
    case ZVITAG::ZVITAG_LAMP_MIRROR_POSITION:              return "Lamp Mirror Position";
    case ZVITAG::ZVITAG_FOCUS_DEPTH:                       return "FocusDepth";
    case ZVITAG::ZVITAG_MICROSCOPE_TYPE:                   return "Microscope Type";
    case ZVITAG::ZVITAG_OBJECTIVE_WORKING_DISTANCE:        return "Objective Working Distance";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_APERTURE_GO_SPEED: return "ReflectedLightApertureGoSpeed";
    case ZVITAG::ZVITAG_EXTERNAL_SHUTTER:                  return "External Shutter";
    case ZVITAG::ZVITAG_OBJECTIVE_IMMERSION_STOP:          return "Objective Immersion Stop";
    case ZVITAG::ZVITAG_FOCUS_START_SPEED:                 return "Focus Start Speed";
    case ZVITAG::ZVITAG_FOCUS_ACCELERATION:                return "Focus Acceleration";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_FIELDSTOP:         return "Reflected Light Fieldstop";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_FIELDSTOP_GO_SPEED: return "ReflectedLightFieldstopGoSpeed";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_1:          return "Reflected Light Filter 1";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_2:          return "Reflected Light Filter 2";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_1_POS:      return "Reflected Light Filter 1 Position";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_FILTER_2_POS:      return "Reflected Light Filter 2 Position";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_ATTENUATOR:      return "Transmitted Light Attenuator";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_ATTENUATOR:        return "Reflected Light Attenuator";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_SHUTTER:         return "Transmitted Light Shutter";
    case ZVITAG::ZVITAG_TLA_GO_SPEED:                      return "TransmittedLightAttenuatorGoSpeed";
    case ZVITAG::ZVITAG_RLA_GO_SPEED:                      return "ReflectedLightAttenuatorGoSpeed";
    case ZVITAG::ZVITAG_TLV_FILTER_POSITION:               return "TransmittedLightVirtualFilterPosition";
    case ZVITAG::ZVITAG_TLV_FILTER:                        return "Transmitted Light Virtual Filter";
    case ZVITAG::ZVITAG_RLV_FILTER_POSITION:               return "ReflectedLightVirtualFilterPosition";
    case ZVITAG::ZVITAG_RLV_FILTER:                        return "Reflected Light Virtual Filter";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_HALOGEN_MODE:      return "Reflected Light Halogen Lamp Mode";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_HALOGEN_VOLTAGE:   return "Reflected Light Halogen Lamp Voltage";
    case ZVITAG::ZVITAG_REFLECTED_LIGHT_HALOGEN_COLOR_TEMP: return "Reflected Light Halogen Lamp Colour Temperature";
    case ZVITAG::ZVITAG_CONTRASTMANAGER_MODE:              return "Contrastmanager Mode";
    case ZVITAG::ZVITAG_DAZZLE_PROTECTION_ACTIVE:          return "Dazzle Protection Active";
    case ZVITAG::ZVITAG_ZOOM:                              return "Zoom";
    case ZVITAG::ZVITAG_ZOOM_GO_SPEED:                     return "ZoomGoSpeed";
    case ZVITAG::ZVITAG_LIGHT_ZOOM:                        return "Light Zoom";
    case ZVITAG::ZVITAG_LIGHT_ZOOM_GO_SPEED:               return "LightZoomGoSpeed";
    case ZVITAG::ZVITAG_LIGHTZOOM_COUPLED:                 return "Lightzoom Coupled";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_HALOGEN_MODE:    return "Transmitted Light Halogen Lamp Mode";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_HALOGEN_VOLTAGE: return "Transmitted Light Halogen Lamp Voltage";
    case ZVITAG::ZVITAG_TRANSMITTED_LIGHT_HALOGEN_COLOR_TEMP: return "Transmitted Light Halogen Lamp Colour Temperature";
    case ZVITAG::ZVITAG_REFLECTED_COLDLIGHT_MODE:          return "Reflected Coldlight Mode";
    case ZVITAG::ZVITAG_REFLECTED_COLDLIGHT_INTENSITY:     return "Reflected Coldlight Intensity";
    case ZVITAG::ZVITAG_REFLECTED_COLDLIGHT_COLOR_TEMP:    return "Reflected Coldlight Colour Temperature";
    case ZVITAG::ZVITAG_TRANSMITTED_COLDLIGHT_MODE:        return "Transmitted Coldlight Mode";
    case ZVITAG::ZVITAG_TRANSMITTED_COLDLIGHT_INTENSITY:   return "Transmitted Coldlight Intensity";
    case ZVITAG::ZVITAG_TRANSMITTED_COLDLIGHT_COLOR_TEMP:  return "Transmitted Coldlight Colour Temperature";
    case ZVITAG::ZVITAG_INFINITYSPACE_PORTCHANGER_POSITION: return "Infinityspace Portchanger Position";
    case ZVITAG::ZVITAG_BEAMSPLITTER_INFINITY_SPACE:       return "Beamsplitter Infinity Space";
    case ZVITAG::ZVITAG_TWOTV_VISCAMCHANGER_POSITION:      return "TwoTv VisCamChanger Position";
    case ZVITAG::ZVITAG_BEAMSPLITTER_OCULAR:               return "Beamsplitter Ocular";
    case ZVITAG::ZVITAG_TWOTV_CAMERASCHANGER_POSITION:     return "TwoTv CamerasChanger Position";
    case ZVITAG::ZVITAG_BEAMSPLITTER_CAMERAS:              return "Beamsplitter Cameras";
    case ZVITAG::ZVITAG_OCULAR_SHUTTER:                    return "Ocular Shutter";
    case ZVITAG::ZVITAG_TWOTV_CAMERASCHANGER_CUBE:         return "TwoTv CamerasChanger Cube";
    case ZVITAG::ZVITAG_LIGHT_WAVELENGTH:                  return "LightWaveLength";
    case ZVITAG::ZVITAG_OCULAR_MAGNIFICATION:              return "Ocular Magnification";
    case ZVITAG::ZVITAG_CAMERA_ADAPTER_MAGNIFICATION:      return "Camera Adapter Magnification";
    case ZVITAG::ZVITAG_MICROSCOPE_PORT:                   return "Microscope Port";
    case ZVITAG::ZVITAG_OCULAR_TOTAL_MAGNIFICATION:        return "Ocular Total Magnification";
    case ZVITAG::ZVITAG_FIELD_OF_VIEW:                     return "Field of View";
    case ZVITAG::ZVITAG_OCULAR:                            return "Ocular";
    case ZVITAG::ZVITAG_CAMERA_ADAPTER:                    return "CameraAdapter";
    case ZVITAG::ZVITAG_STAGE_JOYSTICK_ENABLED:            return "StageJoystickEnabled";
    case ZVITAG::ZVITAG_CONTRASTMANAGER_CONTRAST_METHOD:   return "ContrastmanagerContrastMethod";
    case ZVITAG::ZVITAG_CAMERASCHANGER_BEAMSPLITTER_TYPE:  return "CamerasChanger BeamSplitter Type";
    case ZVITAG::ZVITAG_REARPORT_SLIDER_POSITION:          return "Rearport Slider Position";
    case ZVITAG::ZVITAG_REARPORT_SOURCE:                   return "Rearport Source";
    case ZVITAG::ZVITAG_BEAMSPLITTER_TYPE_INFINITY_SPACE:  return "Beamsplitter Type Infinity Space";
    case ZVITAG::ZVITAG_FLUORESCENCE_ATTENUATOR:           return "Fluorescence Attenuator";
    case ZVITAG::ZVITAG_FLUORESCENCE_ATTENUATOR_POSITION:  return "Fluorescence Attenuator Position";
    case ZVITAG::ZVITAG_CAMERA_FRAMESTART_LEFT:            return "Camera Framestart Left";
    case ZVITAG::ZVITAG_CAMERA_FRAMESTART_TOP:             return "Camera Framestart Top";
    case ZVITAG::ZVITAG_CAMERA_FRAME_WIDTH:                return "Camera Frame Width";
    case ZVITAG::ZVITAG_CAMERA_FRAME_HEIGHT:               return "Camera Frame Height";
    case ZVITAG::ZVITAG_CAMERA_BINNING:                    return "Camera Binning";
    case ZVITAG::ZVITAG_CAMERA_FRAME_FULL:                 return "CameraFrameFull";
    case ZVITAG::ZVITAG_CAMERA_FRAME_PIXEL_DISTANCE:       return "CameraFramePixelDistance";
    case ZVITAG::ZVITAG_DATA_FORMAT_USE_SCALING:           return "DataFormatUseScaling";
    case ZVITAG::ZVITAG_CAMERA_FRAME_IMAGE_ORIENTATION:    return "CameraFrameImageOrientation";
    case ZVITAG::ZVITAG_VIDEO_MONOCHROME_SIGNAL_TYPE:      return "VideoMonochromeSignalType";
    case ZVITAG::ZVITAG_VIDEO_COLOR_SIGNAL_TYPE:           return "VideoColorSignalType";
    case ZVITAG::ZVITAG_METEOR_CHANNEL_INPUT:              return "MeteorChannelInput";
    case ZVITAG::ZVITAG_METEOR_CHANNEL_SYNC:               return "MeteorChannelSync";
    case ZVITAG::ZVITAG_WHITE_BALANCE_ENABLED:             return "WhiteBalanceEnabled";
    case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_RED:          return "CameraWhiteBalanceRed";
    case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_GREEN:        return "CameraWhiteBalanceGreen";
    case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_BLUE:         return "CameraWhiteBalanceBlue";
    case ZVITAG::ZVITAG_CAMERA_FRAME_SCALING_FACTOR:       return "CameraFrameScalingFactor";
    case ZVITAG::ZVITAG_METEOR_CAMERA_TYPE:                return "Meteor Camera Type";
    case ZVITAG::ZVITAG_EXPOSURE_TIME:                     return "Exposure Time [ms]";
    case ZVITAG::ZVITAG_CAMERA_EXPOSURE_TIME_AUTO_CALC:    return "CameraExposureTimeAutoCalculate";
    case ZVITAG::ZVITAG_METEOR_GAIN_VALUE:                 return "Meteor Gain Value";
    case ZVITAG::ZVITAG_METEOR_GAIN_AUTOMATIC:             return "Meteor Gain Automatic";
    case ZVITAG::ZVITAG_METEOR_ADJUST_HUE:                 return "MeteorAdjustHue";
    case ZVITAG::ZVITAG_METEOR_ADJUST_SATURATION:          return "MeteorAdjustSaturation";
    case ZVITAG::ZVITAG_METEOR_ADJUST_RED_LOW:             return "MeteorAdjustRedLow";
    case ZVITAG::ZVITAG_METEOR_ADJUST_GREEN_LOW:           return "MeteorAdjustGreenLow";
    case ZVITAG::ZVITAG_METEOR_BLUE_LOW:                   return "Meteor Blue Low";
    case ZVITAG::ZVITAG_METEOR_ADJUST_RED_HIGH:            return "MeteorAdjustRedHigh";
    case ZVITAG::ZVITAG_METEOR_ADJUST_GREEN_HIGH:          return "MeteorAdjustGreenHigh";
    case ZVITAG::ZVITAG_METEOR_BLUE_HIGH:                  return "Meteor Blue High";
    case ZVITAG::ZVITAG_CAMERA_EXPOSURE_TIME_CALC_CONTROL: return "CameraExposureTimeCalculationControl";
    case ZVITAG::ZVITAG_AXIOCAM_FADING_CORRECTION_ENABLE:  return "AxioCamFadingCorrectionEnable";
    case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE:                 return "CameraLiveImage";
    case ZVITAG::ZVITAG_CAMERA_LIVE_ENABLED:               return "CameraLiveEnabled";
    case ZVITAG::ZVITAG_LIVE_IMAGE_SYNC_OBJECT_NAME:       return "LiveImageSyncObjectName";
    case ZVITAG::ZVITAG_CAMERA_LIVE_SPEED:                 return "CameraLiveSpeed";
    case ZVITAG::ZVITAG_CAMERA_IMAGE:                      return "CameraImage";
    case ZVITAG::ZVITAG_CAMERA_IMAGE_WIDTH:                return "CameraImageWidth";
    case ZVITAG::ZVITAG_CAMERA_IMAGE_HEIGHT:               return "CameraImageHeight";
    case ZVITAG::ZVITAG_CAMERA_IMAGE_PIXEL_TYPE:           return "CameraImagePixelType";
    case ZVITAG::ZVITAG_CAMERA_IMAGE_SH_MEMORY_NAME:       return "CameraImageShMemoryName";
    case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_WIDTH:           return "CameraLiveImageWidth";
    case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_HEIGHT:          return "CameraLiveImageHeight";
    case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_PIXEL_TYPE:      return "CameraLiveImagePixelType";
    case ZVITAG::ZVITAG_CAMERA_LIVE_IMAGE_SH_MEMORY_NAME:  return "CameraLiveImageShMemoryName";
    case ZVITAG::ZVITAG_CAMERA_LIVE_MAXIMUM_SPEED:         return "CameraLiveMaximumSpeed";
    case ZVITAG::ZVITAG_CAMERA_LIVE_BINNING:               return "CameraLiveBinning";
    case ZVITAG::ZVITAG_CAMERA_LIVE_GAIN_VALUE:            return "CameraLiveGainValue";
    case ZVITAG::ZVITAG_CAMERA_LIVE_EXPOSURE_TIME_VALUE:   return "CameraLiveExposureTimeValue";
    case ZVITAG::ZVITAG_CAMERA_LIVE_SCALING_FACTOR:        return "CameraLiveScalingFactor";
    case ZVITAG::ZVITAG_IMAGE_INDEX_U:                     return "Image Index U";
    case ZVITAG::ZVITAG_IMAGE_INDEX_V:                     return "Image Index V";
    case ZVITAG::ZVITAG_IMAGE_INDEX_Z:                     return "Image Index Z";
    case ZVITAG::ZVITAG_IMAGE_INDEX_C:                     return "ImageIndex C";
    case ZVITAG::ZVITAG_IMAGE_INDEX_T:                     return "Image Index T";
    case ZVITAG::ZVITAG_IMAGE_TILE_INDEX:                  return "Image Tile Index";
    case ZVITAG::ZVITAG_IMAGE_ACQUSITION_INDEX:            return "Image acquisition Index";
    case ZVITAG::ZVITAG_IMAGE_COUNT_TILES:                 return "ImageCount Tiles";
    case ZVITAG::ZVITAG_IMAGE_COUNT_A:                     return "ImageCount A";
    case ZVITAG::ZVITAG_IMAGE_INDEX_S:                     return "ImageIndex S";
    case ZVITAG::ZVITAG_IMAGE_INDEX_RAW:                   return "Image Index Raw";
    case ZVITAG::ZVITAG_IMAGE_COUNT_Z:                     return "Image Count Z";
    case ZVITAG::ZVITAG_IMAGE_COUNT_C:                     return "Image Count C";
    case ZVITAG::ZVITAG_IMAGE_COUNT_T:                     return "Image Count T";
    case ZVITAG::ZVITAG_IMAGE_COUNT_U:                     return "Image Count U";
    case ZVITAG::ZVITAG_IMAGE_COUNT_V:                     return "Image Count V";
    case ZVITAG::ZVITAG_IMAGE_COUNT_S:                     return "Image Count S";
    case ZVITAG::ZVITAG_ORIGINAL_STAGE_POSITION_X:         return "Original Stage Position X";
    case ZVITAG::ZVITAG_ORIGINAL_STAGE_POSITION_Y:         return "Original Stage Position Y";
    case ZVITAG::ZVITAG_LAYER_DRAW_FLAGS:                  return "LayerDrawFlags";
    case ZVITAG::ZVITAG_REMAINING_TIME:                    return "Remaining Time";
    case ZVITAG::ZVITAG_USER_FIELD_1:                      return "User Field 1";
    case ZVITAG::ZVITAG_USER_FIELD_2:                      return "User Field 2";
    case ZVITAG::ZVITAG_USER_FIELD_3:                      return "User Field 3";
    case ZVITAG::ZVITAG_USER_FIELD_4:                      return "User Field 4";
    case ZVITAG::ZVITAG_USER_FIELD_5:                      return "User Field 5";
    case ZVITAG::ZVITAG_USER_FIELD_6:                      return "User Field 6";
    case ZVITAG::ZVITAG_USER_FIELD_7:                      return "User Field 7";
    case ZVITAG::ZVITAG_USER_FIELD_8:                      return "User Field 8";
    case ZVITAG::ZVITAG_USER_FIELD_9:                      return "User Field 9";
    case ZVITAG::ZVITAG_USER_FIELD_10:                     return "User Field 10";
    case ZVITAG::ZVITAG_ID:                                return "ID";
    case ZVITAG::ZVITAG_NAME:                              return "Name";
    case ZVITAG::ZVITAG_VALUE:                             return "Value";
    case ZVITAG::ZVITAG_PVCAM_CLOCKING_MODE:               return "PvCamClockingMode";
    case ZVITAG::ZVITAG_AUTOFOCUS_STATUS_REPORT:           return "Autofocus Status Report";
    case ZVITAG::ZVITAG_AUTOFOCUS_POSITION:                return "Autofocus Position";
    case ZVITAG::ZVITAG_AUTOFOCUS_POSITION_OFFSET:         return "Autofocus Position Offset";
    case ZVITAG::ZVITAG_AUTOFOCUS_EMPTY_FIELD_THRESHOLD:   return "Autofocus Empty Field Threshold";
    case ZVITAG::ZVITAG_AUTOFOCUS_CALIBRATION_NAME:        return "Autofocus Calibration Name";
    case ZVITAG::ZVITAG_AUTOFOCUS_CURRENT_CALIBRATION_ITEM: return "Autofocus Current Calibration Item";
    case ZVITAG::ZVITAG_CAMERA_FRAME_FULL_WIDTH:           return "CameraFrameFullWidth";
    case ZVITAG::ZVITAG_CAMERA_FRAME_FULL_HEIGHT:          return "CameraFrameFullHeight";
    case ZVITAG::ZVITAG_AXIOCAM_SHUTTER_SIGNAL:            return "AxioCam Shutter Signal";
    case ZVITAG::ZVITAG_AXIOCAM_DELAY_TIME:                return "AxioCam Delay Time";
    case ZVITAG::ZVITAG_AXIOCAM_SHUTTER_CONTROL:           return "AxioCam Shutter Control";
    case ZVITAG::ZVITAG_AXIOCAM_BLACK_REF_IS_CALCULATED:   return "AxioCamBlackRefIsCalculated";
    case ZVITAG::ZVITAG_AXIOCAM_BLACK_REFERENCE:           return "AxioCam Black Reference";
    case ZVITAG::ZVITAG_CAMERA_SHADING_CORRECTION:         return "Camera Shading Correction";
    case ZVITAG::ZVITAG_AXIOCAM_ENHANCE_COLOR:             return "AxioCam Enhance Color";
    case ZVITAG::ZVITAG_AXIOCAM_NIR_MODE:                  return "AxioCam NIR Mode";
    case ZVITAG::ZVITAG_CAMERA_SHUTTER_CLOSE_DELAY:        return "CameraShutterCloseDelay";
    case ZVITAG::ZVITAG_CAMERA_WHITE_BALANCE_AUTO_CALC:    return "CameraWhiteBalanceAutoCalculate";
    case ZVITAG::ZVITAG_AXIOCAM_NIR_MODE_AVAILABLE:        return "AxioCamNIRModeAvailable";
    case ZVITAG::ZVITAG_AXIOCAM_FADING_CORRECTION_AVAILABLE: return "AxioCamFadingCorrectionAvailable";
    case ZVITAG::ZVITAG_AXIOCAM_ENHANCE_COLOR_AVAILABLE:   return "AxioCamEnhanceColorAvailable";
    case ZVITAG::ZVITAG_METEOR_VIDEO_NORM:                 return "MeteorVideoNorm";
    case ZVITAG::ZVITAG_METEOR_ADJUST_WHITE_REFERENCE:     return "MeteorAdjustWhiteReference";
    case ZVITAG::ZVITAG_METEOR_BLACK_REFERENCE:            return "Meteor Black Reference";
    case ZVITAG::ZVITAG_METEOR_CHANNEL_INPUT_COUNT_MONO:   return "MeteorChannelInputCountMono";
    case ZVITAG::ZVITAG_METEOR_CHANNEL_INPUT_COUNT_RGB:    return "MeteorChannelInputCountRGB";
    case ZVITAG::ZVITAG_METEOR_ENABLE_VCR:                 return "MeteorEnableVCR";
    case ZVITAG::ZVITAG_METEOR_BRIGHTNESS:                 return "Meteor Brightness";
    case ZVITAG::ZVITAG_METEOR_CONTRAST:                   return "Meteor Contrast";
    case ZVITAG::ZVITAG_AXIOCAM_SELECTOR:                  return "AxioCamSelector";
    case ZVITAG::ZVITAG_AXIOCAM_TYPE:                      return "AxioCam Type";
    case ZVITAG::ZVITAG_AXIOCAM_INFO:                      return "AxioCamInfo";
    case ZVITAG::ZVITAG_AXIOCAM_RESOLUTION:                return "AxioCam Resolution";
    case ZVITAG::ZVITAG_AXIOCAM_COLOUR_MODEL:              return "AxioCam Colour Model";
    case ZVITAG::ZVITAG_AXIOCAM_MICROSCANNING:             return "AxioCamMicroScanning";
    case ZVITAG::ZVITAG_AMPLIFICATION_INDEX:               return "Amplification Index";
    case ZVITAG::ZVITAG_DEVICE_COMMAND:                    return "DeviceCommand";
    case ZVITAG::ZVITAG_BEAM_LOCATION:                     return "BeamLocation";
    case ZVITAG::ZVITAG_COMPONENT_TYPE:                    return "ComponentType";
    case ZVITAG::ZVITAG_CONTROLLER_TYPE:                   return "ControllerType";
    case ZVITAG::ZVITAG_CAMERA_WB_CALC_RED_PAINT:          return "CameraWhiteBalanceCalculationRedPaint";
    case ZVITAG::ZVITAG_CAMERA_WB_CALC_BLUE_PAINT:         return "CameraWhiteBalanceCalculationBluePaint";
    case ZVITAG::ZVITAG_CAMERA_WB_SET_RED:                 return "CameraWhiteBalanceSetRed";
    case ZVITAG::ZVITAG_CAMERA_WB_SET_GREEN:               return "CameraWhiteBalanceSetGreen";
    case ZVITAG::ZVITAG_CAMERA_WB_SET_BLUE:                return "CameraWhiteBalanceSetBlue";
    case ZVITAG::ZVITAG_CAMERA_WB_SET_TARGET_RED:          return "CameraWhiteBalanceSetTargetRed";
    case ZVITAG::ZVITAG_CAMERA_WB_SET_TARGET_GREEN:        return "CameraWhiteBalanceSetTargetGreen";
    case ZVITAG::ZVITAG_CAMERA_WB_SET_TARGET_BLUE:         return "CameraWhiteBalanceSetTargetBlue";
    case ZVITAG::ZVITAG_APOTOMECAM_CALIBRATION_MODE:       return "ApotomeCamCalibrationMode";
    case ZVITAG::ZVITAG_APOTOME_GRID_POSITION:             return "ApoTome Grid Position";
    case ZVITAG::ZVITAG_APOTOMECAM_SCANNER_POSITION:       return "ApotomeCamScannerPosition";
    case ZVITAG::ZVITAG_APOTOME_FULL_PHASE_SHIFT:          return "ApoTome Full Phase Shift";
    case ZVITAG::ZVITAG_APOTOME_GRID_NAME:                 return "ApoTome Grid Name";
    case ZVITAG::ZVITAG_APOTOME_STAINING:                  return "ApoTome Staining";
    case ZVITAG::ZVITAG_APOTOME_PROCESSING_MODE:           return "ApoTome Processing Mode";
    case ZVITAG::ZVITAG_APOTOMECAM_LIVE_COMBINE_MODE:      return "ApotomeCamLiveCombineMode";
    case ZVITAG::ZVITAG_APOTOME_FILTER_NAME:               return "ApoTome Filter Name";
    case ZVITAG::ZVITAG_APOTOME_FILTER_STRENGTH:           return "Apotome Filter Strength";
    case ZVITAG::ZVITAG_APOTOMECAM_FILTER_HARMONICS:       return "ApotomeCamFilterHarmonics";
    case ZVITAG::ZVITAG_APOTOME_GRATING_PERIOD:            return "ApoTome Grating Period";
    case ZVITAG::ZVITAG_APOTOME_AUTO_SHUTTER_USED:         return "ApoTome Auto Shutter Used";
    case ZVITAG::ZVITAG_APOTOMECAM_STATUS:                 return "ApotomeCamStatus";
    case ZVITAG::ZVITAG_APOTOMECAM_NORMALIZE:              return "ApotomeCamNormalize";
    case ZVITAG::ZVITAG_APOTOMECAM_SETTINGS_MANAGER:       return "ApotomeCamSettingsManager";
    case ZVITAG::ZVITAG_DEEPVIEWCAM_SUPERVISOR_MODE:       return "DeepviewCamSupervisorMode";
    case ZVITAG::ZVITAG_DEEPVIEW_PROCESSING:               return "DeepView Processing";
    case ZVITAG::ZVITAG_DEEPVIEWCAM_FILTER_NAME:           return "DeepviewCamFilterName";
    case ZVITAG::ZVITAG_DEEPVIEWCAM_STATUS:                return "DeepviewCamStatus";
    case ZVITAG::ZVITAG_DEEPVIEWCAM_SETTINGS_MANAGER:      return "DeepviewCamSettingsManager";
    case ZVITAG::ZVITAG_DEVICE_SCALING_NAME:               return "DeviceScalingName";
    case ZVITAG::ZVITAG_CAMERA_SHADING_IS_CALCULATED:      return "CameraShadingIsCalculated";
    case ZVITAG::ZVITAG_CAMERA_SHADING_CALCULATION_NAME:   return "CameraShadingCalculationName";
    case ZVITAG::ZVITAG_CAMERA_SHADING_AUTO_CALCULATE:     return "CameraShadingAutoCalculate";
    case ZVITAG::ZVITAG_CAMERA_TRIGGER_AVAILABLE:          return "CameraTriggerAvailable";
    case ZVITAG::ZVITAG_CAMERA_SHUTTER_AVAILABLE:          return "CameraShutterAvailable";
    case ZVITAG::ZVITAG_AXIOCAM_SHUTTER_MICROSCAN_ENABLE:  return "AxioCamShutterMicroScanningEnable";
    case ZVITAG::ZVITAG_APOTOMECAM_LIVE_FOCUS:             return "ApotomeCamLiveFocus";
    case ZVITAG::ZVITAG_DEVICE_INIT_STATUS:                return "DeviceInitStatus";
    case ZVITAG::ZVITAG_DEVICE_ERROR_STATUS:               return "DeviceErrorStatus";
    case ZVITAG::ZVITAG_APOTOMECAM_SLIDER_IN_GRID_POSITION: return "ApotomeCamSliderInGridPosition";
    case ZVITAG::ZVITAG_ORCA_NIR_MODE_USED:                return "Orca NIR Mode Used";
    case ZVITAG::ZVITAG_ORCA_ANALOG_GAIN:                  return "Orca Analog Gain";
    case ZVITAG::ZVITAG_ORCA_ANALOG_OFFSET:                return "Orca Analog Offset";
    case ZVITAG::ZVITAG_ORCA_BINNING:                      return "Orca Binning";
    case ZVITAG::ZVITAG_ORCA_BIT_DEPTH:                    return "Orca Bit Depth";
    case ZVITAG::ZVITAG_APOTOME_AVERAGING_COUNT:           return "ApoTome Averaging Count";
    case ZVITAG::ZVITAG_DEEPVIEW_DOF:                      return "DeepView DoF";
    case ZVITAG::ZVITAG_DEEPVIEW_EDOF:                     return "DeepView EDoF";
    case ZVITAG::ZVITAG_DEEPVIEW_SLIDER_NAME:              return "DeepView Slider Name";
    }
    return nullptr;
}

} // namespace slideio
```

The single `return nullptr;` after the closing brace handles every id that does not match an enumerator (i.e., unknown ids), without needing a `default:` label (so the compiler still warns on a missing enumerator if the enum is later extended).

### Step 1.5: Add the new file to CMake

- [ ] Edit `src/slideio/drivers/zvi/CMakeLists.txt`. In the `SOURCE_FILES` list, after the `zvitags.hpp` line (line 15 in the current file), add a line for `zvitags.cpp`:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/zvitags.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvitags.cpp
```

### Step 1.6: Run the test and verify it passes

- [ ] Re-run conan/cmake configure if needed, then build:
```
python3 install.py -a build -c release
```
- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVITags.*"
```
Expected: 2 tests, both PASS.

### Step 1.7: Commit

- [ ] Stage and commit:
```
git add src/slideio/drivers/zvi/zvitags.hpp \
        src/slideio/drivers/zvi/zvitags.cpp \
        src/slideio/drivers/zvi/CMakeLists.txt \
        src/tests/main/test_zviutils.cpp
git commit -m "$(cat <<'EOF'
zvi: expand ZVITAG enum and add getZviTagName lookup

Add every tag id from the ZVI 2009 spec Section 3.4 to the ZVITAG enum
and a name-lookup function used by the upcoming metadata serialization.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Add `readAllTags` helper + harden `readItem`

**Files:**
- Modify: `src/slideio/drivers/zvi/zviutils.hpp`
- Modify: `src/slideio/drivers/zvi/zviutils.cpp`
- Test: `src/tests/main/test_zviutils.cpp`

### Step 2.1: Write the failing test

- [ ] Append to `src/tests/main/test_zviutils.cpp`:

```cpp
TEST(ZVIUtils, readAllTags_imageTagsContents)
{
    std::string file_path = TestTools::getTestImagePath("zvi","Zeiss-1-Merged.zvi");
    ole::compound_document doc(file_path);
    ASSERT_TRUE(doc.good());
    ZVIUtils::StreamKeeper stream(doc, "/Image/Tags/Contents");
    std::vector<ZVIUtils::ZviTagEntry> entries =
        ZVIUtils::readAllTags(stream, /*hasClsidHeader=*/false);
    ASSERT_FALSE(entries.empty());

    bool hasFilename = false;
    for (const auto& e : entries) {
        if (e.id == static_cast<int32_t>(ZVITAG::ZVITAG_FILE_NAME)) {
            ASSERT_EQ(e.value.index(), 7u); // std::string is index 7 in the Variant
            EXPECT_FALSE(std::get<std::string>(e.value).empty());
            hasFilename = true;
            break;
        }
    }
    EXPECT_TRUE(hasFilename);
}
```

(The test file already includes `zviutils.hpp` and `pole_lib.hpp`. Make sure `zvitags.hpp` is included from Task 1.)

### Step 2.2: Run and verify it fails

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIUtils.readAllTags_imageTagsContents"
```
Expected: compile error — `ZviTagEntry` and `readAllTags` undeclared.

### Step 2.3: Declare `ZviTagEntry` and `readAllTags`

- [ ] In `src/slideio/drivers/zvi/zviutils.hpp`, inside `namespace slideio::ZVIUtils`, just below the `Variant` typedef (line 70), add:

```cpp
        struct ZviTagEntry {
            int32_t id;
            Variant value;
        };

        // Reads a ZVI Tags stream:
        //   - hasClsidHeader=true:  16 raw bytes (CLSID) precede {Version}{Count}.
        //                           Use this for the root-level <Tags> stream.
        //   - hasClsidHeader=false: stream starts directly with {Version}{Count}.
        //                           Use this for [Image]/[Tags]/<Contents> and
        //                           [Item(n)]/[Tags]/<Contents>.
        // Returns one ZviTagEntry per (Value, TagID, Attribute) triple. Entries
        // whose Value variant is monostate (VT_EMPTY / unsupported types) are
        // dropped. {Attribute} is consumed and discarded.
        std::vector<ZviTagEntry> SLIDEIO_ZVI_EXPORTS readAllTags(
            ole::basic_stream& stream, bool hasClsidHeader);
```

### Step 2.4: Implement `readAllTags` and harden `readItem`

- [ ] In `src/slideio/drivers/zvi/zviutils.cpp`:

**Harden `readItem`.** In the `switch` inside `ZVIUtils::readItem` (around line 199–214), before the `default:` label, add:

```cpp
    case VT_BLOB:
        stream.read((char*)&offset, 4);
        offset = Endian::fromLittleEndianToNative(offset);
        break;
    case VT_STORED_OBJECT:
        // {Streamed BSTR}: 16-bit length followed by 'length' wchar_t units.
        {
            uint16_t len16 = 0;
            stream.read((char*)&len16, 2);
            len16 = Endian::fromLittleEndianToNative(len16);
            offset = static_cast<uint32_t>(len16) * 2u;
        }
        break;
```

These match the byte-skip behavior already implemented in `ZVIUtils::skipItem`, so blob-valued or stored-object-valued tags don't blow up `readItem` and instead return `std::monostate`.

**Add `readAllTags` at the end of the file:**

```cpp
std::vector<ZVIUtils::ZviTagEntry> ZVIUtils::readAllTags(
    ole::basic_stream& stream, bool hasClsidHeader)
{
    if (hasClsidHeader) {
        // 128-bit CLSID — raw, not a typed token.
        stream.seek(16, std::ios::cur);
    }
    const int32_t version = readIntItem(stream);
    (void)version; // not used; spec value is informational.
    const int32_t count = readIntItem(stream);
    std::vector<ZviTagEntry> entries;
    entries.reserve(count > 0 ? static_cast<size_t>(count) : 0u);
    for (int32_t i = 0; i < count; ++i) {
        Variant value = readItem(stream);
        const int32_t id = readIntItem(stream);
        skipItem(stream); // {Attribute} — no longer used per spec.
        if (value.index() == 0) {
            continue; // std::monostate: VT_EMPTY/VT_NULL/blob/object — drop.
        }
        entries.push_back(ZviTagEntry{ id, std::move(value) });
    }
    return entries;
}
```

### Step 2.5: Run the test and verify it passes

- [ ] Build and run:
```
python3 install.py -a build -c release
./build/release/bin/slideio_tests --gtest_filter="ZVIUtils.readAllTags_imageTagsContents"
```
Expected: PASS.

- [ ] Also run the full `ZVIUtils.*` and `ZVITags.*` suites to make sure the `readItem` change didn't regress anything:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIUtils.*:ZVITags.*"
```
Expected: all PASS.

### Step 2.6: Commit

- [ ] Stage and commit:
```
git add src/slideio/drivers/zvi/zviutils.hpp \
        src/slideio/drivers/zvi/zviutils.cpp \
        src/tests/main/test_zviutils.cpp
git commit -m "$(cat <<'EOF'
zvi: add readAllTags helper and handle VT_BLOB/VT_STORED_OBJECT

readAllTags consumes a complete ZVI Tags stream (with or without leading
CLSID) and returns id/value pairs. readItem now skips VT_BLOB and
VT_STORED_OBJECT tokens instead of throwing, matching skipItem's coverage.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Populate `ZVISlide` metadata + override `buildMetadataTree`

**Files:**
- Modify: `src/slideio/drivers/zvi/zvislide.hpp`
- Modify: `src/slideio/drivers/zvi/zvislide.cpp`
- Test: `src/tests/main/test_zvi_driver.cpp`

### Step 3.1: Write the failing tests (and update the stale one)

- [ ] In `src/tests/main/test_zvi_driver.cpp`, find the line that currently reads:

```cpp
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::None);
```

(in the `TEST(ZVIImageDriver, openSlide2D)` block, around line 48). Replace it with:

```cpp
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::JSON);
    EXPECT_FALSE(slide->getRawMetadata().empty());
    const slideio::Metadata& meta = slide->getMetadata();
    EXPECT_EQ(meta["Filename"].asString(),
              std::string("RQ26033_04310292C0004S_Calu3_amplified_100x_21Jun2012 ic zsm.zvi"));
    EXPECT_EQ(meta["Image Width (Pixel)"].asInt(),  1480);
    EXPECT_EQ(meta["Image Height (Pixel)"].asInt(), 1132);
```

- [ ] At the top of the file, add includes if not already present:

```cpp
#include "slideio/core/metadata.hpp"
```

Do **not** modify the existing line that checks `scene->getMetadataFormat() == None` — Scene metadata is intentionally untouched.

### Step 3.2: Run and verify it fails

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIImageDriver.openSlide2D"
```
Expected: FAIL — `slide->getMetadataFormat()` is still `None`.

### Step 3.3: Declare `buildMetadataTree` on `ZVISlide`

- [ ] In `src/slideio/drivers/zvi/zvislide.hpp`, inside the `class ZVISlide : public CVSlide` body, add a `protected:` (or after the existing public methods) override:

```cpp
    protected:
        MetadataBuilder buildMetadataTree() const override;
```

Put it adjacent to the existing `init()` private method or after the existing `getTFrameResolution` public declaration — either placement works; keep it `protected` to mirror `CVSlide::buildMetadataTree`.

### Step 3.4: Implement metadata population

- [ ] Replace the contents of `src/slideio/drivers/zvi/zvislide.cpp` with:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/core/tools/tools.hpp"
#include <nlohmann/json.hpp>
#include <pole/storage.hpp>

using namespace slideio;

ZVISlide::ZVISlide(const std::string& filePath, const std::string& driverId) : m_filePath(filePath)
{
	setDriverId(driverId);
	init();
}

int ZVISlide::getNumScenes() const
{
	return 1;
}

std::string ZVISlide::getFilePath() const
{
	return "";
}

std::shared_ptr<CVScene> ZVISlide::getScene(int index) const
{
	if(index!=0)
	{
		throw std::runtime_error("ZVIImageDriver: Invalid scene index");
	}
	return m_scene;
}

double ZVISlide::getMagnification() const
{
	return 0;
}

Resolution ZVISlide::getResolution() const
{
	return Resolution();
}

double ZVISlide::getZSliceResolution() const
{
	return 0;
}

double ZVISlide::getTFrameResolution() const
{
	return 0;
}

namespace {

// Converts a ZVIUtils::Variant into an nlohmann::json scalar.
// Monostates are filtered out by readAllTags before reaching this, so the
// 0-index branch is defensive only.
nlohmann::json variantToJson(const ZVIUtils::Variant& v)
{
    switch (v.index()) {
    case 1: return std::get<bool>(v);
    case 2: return std::get<int32_t>(v);
    case 3: return static_cast<int64_t>(std::get<uint32_t>(v));
    case 4: return static_cast<int64_t>(std::get<uint64_t>(v));
    case 5: return std::get<int64_t>(v);
    case 6: return std::get<double>(v);
    case 7: return std::get<std::string>(v);
    default: return nullptr;
    }
}

void mergeTags(const std::vector<ZVIUtils::ZviTagEntry>& entries,
               nlohmann::json& out)
{
    for (const auto& e : entries) {
        const char* name = getZviTagName(e.id);
        const std::string key = name ? std::string(name)
                                     : "Tag_" + std::to_string(e.id);
        out[key] = variantToJson(e.value);
    }
}

} // namespace

void ZVISlide::init()
{
	m_scene.reset(new ZVIScene(m_filePath, getDriverId()));

	// Read tag metadata. Open the OLE document a second time; ZVIScene
	// owns its own copy privately, and a second open of the same OLE
	// structure is cheap.
#if defined(WIN32)
	ole::compound_document doc(Tools::toWstring(m_filePath));
#else
	ole::compound_document doc(m_filePath);
#endif
	if (!doc.good()) {
		// Defer to ZVIScene's diagnostics; metadata is best-effort.
		return;
	}

	nlohmann::json root = nlohmann::json::object();

	// /Image/Tags/Contents — present on every ZVI file. If reading throws
	// (corrupted stream, unexpected variant type), drop the partially built
	// metadata rather than failing slide construction.
	try {
		ZVIUtils::StreamKeeper s(doc, "/Image/Tags/Contents");
		mergeTags(ZVIUtils::readAllTags(s, /*hasClsidHeader=*/false), root);
	} catch (const std::exception&) {
		// best-effort
	}

	// Optional root-level <Tags> stream. Standardized location per spec §2,
	// so its entries take precedence on duplicate keys (overwrite).
	try {
		ZVIUtils::StreamKeeper s(doc, "/Tags");
		mergeTags(ZVIUtils::readAllTags(s, /*hasClsidHeader=*/true), root);
	} catch (const std::exception&) {
		// Missing stream is normal for older files; not an error.
	}

	if (!root.empty()) {
		m_rawMetadata = root.dump(2);
		m_metadataFormat = MetadataFormat::JSON;
	}
}

MetadataBuilder ZVISlide::buildMetadataTree() const
{
	if (m_rawMetadata.empty()) {
		return CVSlide::buildMetadataTree();
	}
	return detail::builderFromJson(nlohmann::json::parse(m_rawMetadata));
}
```

### Step 3.5: Verify nlohmann/json is reachable from the zvi driver

- [ ] The zvi driver already links `pole::pole` which transitively depends on nlohmann_json in this project. If linkage fails:
  - Check `src/slideio/drivers/zvi/conanfile.txt` for `nlohmann_json` (other drivers like svs already list it).
  - Add `find_package(nlohmann_json)` and `nlohmann_json::nlohmann_json` to `target_link_libraries` in `CMakeLists.txt` if not transitively provided.
  - Reference `src/slideio/drivers/svs/CMakeLists.txt` and `conanfile.txt` for the exact incantation.

Build expected to succeed:
```
python3 install.py -a build -c release
```

### Step 3.6: Run the test and verify it passes

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIImageDriver.openSlide2D"
```
Expected: PASS.

- [ ] Run the full ZVI driver suite to make sure nothing regressed:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIImageDriver.*"
```
Expected: all PASS (including `openSlide3D` and others — the Scene-level `MetadataFormat::None` assertion for the 3D test remains valid).

### Step 3.7: Commit

- [ ] Stage and commit:
```
git add src/slideio/drivers/zvi/zvislide.hpp \
        src/slideio/drivers/zvi/zvislide.cpp \
        src/tests/main/test_zvi_driver.cpp
git commit -m "$(cat <<'EOF'
zvi: expose file metadata as JSON via ZVISlide

ZVISlide::init() now reads /Image/Tags/Contents and the optional root
/Tags stream, serializing every tag as a JSON object keyed by spec name
(or Tag_<id> for unknown ids). MetadataFormat is reported as JSON and
buildMetadataTree exposes the same data through Metadata.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Verify no regressions across the full ZVI test suite

**Files:** none.

### Step 4.1: Run all ZVI-related tests

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVI*"
```
Expected: all PASS.

### Step 4.2: Run the main test binary

- [ ] Run:
```
./build/release/bin/slideio_tests
```
Expected: same pass/fail set as on `master` plus the new ZVI tests passing. If anything outside ZVI regresses, investigate — the only files touched outside the zvi driver are zvi-specific tests, so cross-driver regressions would be surprising.

### Step 4.3: (Optional) Quick eyeball of the JSON output

- [ ] In a quick scratch test (or interactively), dump the metadata for `Zeiss-1-Merged.zvi`:
```cpp
auto slide = driver.openFile(filePath);
std::cout << slide->getRawMetadata() << std::endl;
```
Confirm there are no garbled strings (UTF-8) and no obviously broken fields. Skip if you've manually inspected enough integration output already.

### Step 4.4: No commit

- [ ] Nothing to commit from this task. If you discovered a problem and fixed it, commit that fix separately.

---

## Self-Review (already performed by author)

- **Spec coverage:**
  - Spec §4.1 → Task 1 (enum + getZviTagName).
  - Spec §4.2 → Task 2 (readAllTags + readItem hardening).
  - Spec §4.3 → Task 3 step 3.4 (init populates m_rawMetadata).
  - Spec §4.4 → Task 3 steps 3.3 + 3.4 (buildMetadataTree override).
  - Spec §5 (option A: re-open doc) → Task 3 step 3.4 (separate `ole::compound_document doc(...)` in `ZVISlide::init`).
  - Spec §6.1 (update existing assertion) → Task 3 step 3.1.
  - Spec §6.2 (new test cases) → Task 3 step 3.1 (Filename + width + height).
  - Spec §7 (risks) → Task 2 handles VT_BLOB; VT_DATE is acceptably dropped via existing readItem skip; thumbnail (VT_BLOB) flows through `mergeTags` as monostate (skipped).

- **Placeholder scan:** no TBD/TODO/"similar to". Every code-changing step shows actual code.

- **Type consistency:** `ZviTagEntry { int32_t id; Variant value; }` declared in zviutils.hpp (Task 2.3), consumed in zvislide.cpp (Task 3.4 via `mergeTags`). `getZviTagName(int32_t)` signature declared in zvitags.hpp (Task 1.3), defined in zvitags.cpp (Task 1.4), consumed in zvislide.cpp (Task 3.4). Variant index→type mapping in `variantToJson` matches the declaration order `monostate, bool, int32_t, uint32_t, uint64_t, int64_t, double, string` in zviutils.hpp (lines 70 of current file).

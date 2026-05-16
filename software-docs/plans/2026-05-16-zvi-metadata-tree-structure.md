# ZVI Metadata Tree Structure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the flat-JSON output from the initial pass (commits `5cbfd40..7812c1d`) with a hierarchical tree that surfaces per-channel and per-z/t-slice metadata, with repeated-id arrays and bottom-up hoisting.

**Architecture:** Add a narrow `ZVIScene::getImageItems()` accessor for sparse item indexing. Rewrite the anonymous-namespace helpers in `zvislide.cpp` to: (a) convert one tag stream to a JSON object with repeat-collapse, (b) hoist invariant tags up one level. `ZVISlide::init` reads file-level tags into root, iterates `m_scene->getImageItems()` reading each item's tag stream, buckets results by (C,Z,T), runs t-hoist then z-hoist, and emits a `"Channels"` array (with conditional `ZSlices`/`TFrames` nesting).

**Tech Stack:** C++17, nlohmann_json (already linked), pole OLE, GoogleTest, CMake.

**Spec:** `software-docs/specs/2026-05-16-zvi-file-metadata-design.md` §10.

---

## File Map

**Modify:**
- `src/slideio/drivers/zvi/zviscene.hpp` — add one public const accessor for `m_ImageItems`.
- `src/slideio/drivers/zvi/zvislide.cpp` — replace the `mergeTags`/`variantToJson`/`init()`/`buildMetadataTree()` block with the tree-building implementation. `variantToJson` stays (still useful for per-leaf conversion). Adds `tagsToJsonObject` (repeat-collapse) and `hoistInvariantTags`.
- `src/tests/main/test_zvi_driver.cpp` — update `openSlide2D` to the new shape; add `metadataChannelsArray`, `metadata3DHasZSlices`, and a hoist-verification assertion.

**No changes:**
- `src/slideio/drivers/zvi/zvitags.{hpp,cpp}` — unchanged.
- `src/slideio/drivers/zvi/zviutils.{hpp,cpp}` — unchanged.
- `src/slideio/drivers/zvi/zvislide.hpp` — unchanged (the override is already declared).
- `src/slideio/drivers/zvi/zviimageitem.{hpp,cpp}` — unchanged.

---

## Pre-flight

- [ ] **Step 0.1: Read the spec §10** at `software-docs/specs/2026-05-16-zvi-file-metadata-design.md`. The tree shape, hoisting rules, and repeat-collapse semantics are defined there.

- [ ] **Step 0.2: Confirm baseline build/tests pass.** Build:
```
python3 install.py -a build -c release
```
Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIImageDriver.*"
```
Expected: all PASS at `c310de7`.

---

## Task 1: Add `ZVIScene::getImageItems()` accessor

**Files:**
- Modify: `src/slideio/drivers/zvi/zviscene.hpp`

### Step 1.1: Add the accessor

- [ ] In `src/slideio/drivers/zvi/zviscene.hpp`, find the existing public methods section (between line 30 `ZVIScene(...)` and line 60 `initializeBlock`). Add this one method just after `getCompression()`:

```cpp
        const std::vector<ZVIImageItem>& getImageItems() const { return m_ImageItems; }
```

The placement matters because the accessor needs to be public; everything else about `m_ImageItems` stays private.

### Step 1.2: Build

- [ ] Run:
```
python3 install.py -a build -c release
```
Expected: clean build. No test change yet — this accessor is plumbing consumed by Task 2.

### Step 1.3: Commit

- [ ] Stage and commit:
```
git add src/slideio/drivers/zvi/zviscene.hpp
git commit -m "$(cat <<'EOF'
zvi: expose image items via ZVIScene::getImageItems()

Narrow const accessor for the existing private m_ImageItems vector,
consumed by ZVISlide's upcoming tree-building metadata pass. Does not
touch the scene's public metadata API.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Rewrite `ZVISlide` metadata builder for tree shape

**Files:**
- Modify: `src/slideio/drivers/zvi/zvislide.cpp`
- Test: `src/tests/main/test_zvi_driver.cpp`

### Step 2.1: Write the failing test (TDD red)

- [ ] In `src/tests/main/test_zvi_driver.cpp`, find the `TEST(ZVIImageDriver, openSlide2D)` block. Replace the metadata assertions block (the lines that currently check `MetadataFormat::JSON`, `getRawMetadata` non-empty, and the three flat `meta["..."]` checks) with:

```cpp
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::JSON);
    EXPECT_FALSE(slide->getRawMetadata().empty());
    const slideio::Metadata& meta = slide->getMetadata();

    // File-level tags remain at the root.
    EXPECT_EQ(meta["Image Width (Pixel)"].asInt(),  1480);
    EXPECT_EQ(meta["Image Height (Pixel)"].asInt(), 1132);

    // Per-item tags appear under Channels[].
    const slideio::Metadata& channels = meta["Channels"];
    ASSERT_TRUE(channels.isArray());
    ASSERT_EQ(channels.size(), 3u);

    EXPECT_EQ(channels[0]["Channel Name"].asString(), std::string("Hoechst 33342"));
    EXPECT_EQ(channels[1]["Channel Name"].asString(), std::string("Cy3"));
    EXPECT_EQ(channels[2]["Channel Name"].asString(), std::string("FITC"));

    // 2D image: no ZSlices nesting.
    EXPECT_FALSE(channels[0].contains("ZSlices"));
```

Note: leave the `EXPECT_EQ(meta["Filename"]...)` line **removed** — `Filename` is a file-level tag that's still at root, but the existing assertion overlaps with the existing `scene->getName()` check at line 67 and is redundant given the new Channels checks. (If you find it cleaner to keep, keep it — but the spec doesn't require it.)

- [ ] In the same file, find `TEST(ZVIImageDriver, openSlide3D)`. After the existing assertions about scene dimensions, add:

```cpp
    EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::JSON);
    const slideio::Metadata& meta = slide->getMetadata();
    const slideio::Metadata& channels = meta["Channels"];
    ASSERT_TRUE(channels.isArray());
    // Zeiss-1-Stacked.zvi has one channel and multiple z-slices.
    ASSERT_GE(channels.size(), 1u);

    const slideio::Metadata& ch0 = channels[0];
    ASSERT_TRUE(ch0.contains("ZSlices"));
    const slideio::Metadata& zSlices = ch0["ZSlices"];
    ASSERT_TRUE(zSlices.isArray());
    EXPECT_GT(zSlices.size(), 1u);

    // Channel Name (if present) is hoisted to the channel object, not duplicated per ZSlice.
    if (ch0.contains("Channel Name")) {
        EXPECT_FALSE(zSlices[0].contains("Channel Name"));
    }
```

Include the metadata header at the top of the file if not already there:
```cpp
#include "slideio/core/metadata.hpp"
```

### Step 2.2: Run and verify it fails

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIImageDriver.openSlide2D:ZVIImageDriver.openSlide3D"
```
Expected: FAIL — current flat-JSON output has no `"Channels"` key, so `channels.isArray()` fails.

### Step 2.3: Replace `zvislide.cpp`

- [ ] Replace the entire contents of `src/slideio/drivers/zvi/zvislide.cpp` with:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zviimageitem.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/base/log.hpp"
#include <nlohmann/json.hpp>
#include <pole/storage.hpp>
#include <map>
#include <type_traits>
#include <variant>

using namespace slideio;

ZVISlide::ZVISlide(const std::string& filePath, const std::string& driverId) : m_filePath(filePath)
{
	setDriverId(driverId);
	init();
}

int ZVISlide::getNumScenes() const { return 1; }
std::string ZVISlide::getFilePath() const { return ""; }

std::shared_ptr<CVScene> ZVISlide::getScene(int index) const
{
	if (index != 0) {
		throw std::runtime_error("ZVIImageDriver: Invalid scene index");
	}
	return m_scene;
}

double ZVISlide::getMagnification() const { return 0; }
Resolution ZVISlide::getResolution() const { return Resolution(); }
double ZVISlide::getZSliceResolution() const { return 0; }
double ZVISlide::getTFrameResolution() const { return 0; }

namespace {

nlohmann::json variantToJson(const ZVIUtils::Variant& v)
{
    return std::visit([](const auto& x) -> nlohmann::json {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;
        } else if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>) {
            return static_cast<int64_t>(x);
        } else {
            return x;
        }
    }, v);
}

// Convert a tag-entry vector into a JSON object. Repeated tag ids are
// collapsed into a JSON array in encounter order: first occurrence is
// scalar, second turns it into an array of [first, second], subsequent
// occurrences append.
nlohmann::json tagsToJsonObject(const std::vector<ZVIUtils::ZviTagEntry>& entries)
{
    nlohmann::json obj = nlohmann::json::object();
    for (const auto& e : entries) {
        const char* name = getZviTagName(e.id);
        const std::string key = name ? std::string(name)
                                     : "Tag_" + std::to_string(e.id);
        nlohmann::json value = variantToJson(e.value);

        auto it = obj.find(key);
        if (it == obj.end()) {
            obj[key] = std::move(value);
        } else if (it->is_array()) {
            it->push_back(std::move(value));
        } else {
            nlohmann::json arr = nlohmann::json::array();
            arr.push_back(std::move(*it));
            arr.push_back(std::move(value));
            *it = std::move(arr);
        }
    }
    return obj;
}

// For every key present in all `children` with identical values, remove
// the key from each child and place it on `parent`. Mutates both.
void hoistInvariantTags(nlohmann::json& parent,
                        std::vector<nlohmann::json*>& children)
{
    if (children.empty()) return;

    // Collect candidate keys from the first child.
    nlohmann::json& first = *children.front();
    std::vector<std::string> candidates;
    candidates.reserve(first.size());
    for (auto it = first.begin(); it != first.end(); ++it) {
        candidates.push_back(it.key());
    }

    for (const std::string& key : candidates) {
        const nlohmann::json& firstVal = first.at(key);
        bool invariant = true;
        for (size_t i = 1; i < children.size(); ++i) {
            auto it = children[i]->find(key);
            if (it == children[i]->end() || *it != firstVal) {
                invariant = false;
                break;
            }
        }
        if (invariant) {
            parent[key] = firstVal;
            for (auto* child : children) {
                child->erase(key);
            }
        }
    }
}

// Tries to read a Tags stream from the open OLE document at `path`.
// Returns std::nullopt on any failure (missing stream, parse error).
std::optional<nlohmann::json> tryReadTagsObject(ole::compound_document& doc,
                                                const std::string& path,
                                                bool hasClsidHeader)
{
    try {
        ZVIUtils::StreamKeeper s(doc, path);
        return tagsToJsonObject(ZVIUtils::readAllTags(s, hasClsidHeader));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

void ZVISlide::init()
{
	m_scene.reset(new ZVIScene(m_filePath, getDriverId()));
	auto scene = std::dynamic_pointer_cast<ZVIScene>(m_scene);
	if (!scene) {
		return;
	}

#if defined(WIN32)
	ole::compound_document doc(Tools::toWstring(m_filePath));
#else
	ole::compound_document doc(m_filePath);
#endif
	if (!doc.good()) {
		return;
	}

	nlohmann::json root = nlohmann::json::object();

	// File-level /Image/Tags/Contents.
	try {
		ZVIUtils::StreamKeeper s(doc, "/Image/Tags/Contents");
		root = tagsToJsonObject(ZVIUtils::readAllTags(s, false));
	} catch (const std::exception& e) {
		SLIDEIO_LOG(WARNING) << "ZVIImageDriver: failed to read /Image/Tags/Contents: " << e.what();
	}

	// Optional root-level <Tags>. Silent on failure (normal for older files).
	if (auto rootTags = tryReadTagsObject(doc, "/Tags", true)) {
		for (auto it = rootTags->begin(); it != rootTags->end(); ++it) {
			root[it.key()] = it.value();
		}
	}

	// Per-item streams, bucketed by (C, Z, T).
	const int numChannels = scene->getNumChannels();
	const int numZSlices  = scene->getNumZSlices();
	const int numTFrames  = scene->getNumTFrames();

	// items[c][z][t] = parsed per-item tag object.
	std::map<int, std::map<int, std::map<int, nlohmann::json>>> items;

	for (const ZVIImageItem& item : scene->getImageItems()) {
		const int c = item.getCIndex();
		const int z = item.getZIndex();
		const int t = item.getTIndex();
		const std::string path =
			"/Image/Item(" + std::to_string(item.getItemIndex()) + ")/Tags/Contents";
		auto obj = tryReadTagsObject(doc, path, false);
		if (obj) {
			items[c][z][t] = std::move(*obj);
		} else {
			items[c][z][t] = nlohmann::json::object();
		}
	}

	if (!items.empty()) {
		nlohmann::json channelsArr = nlohmann::json::array();

		for (int c = 0; c < numChannels; ++c) {
			nlohmann::json channelObj = nlohmann::json::object();
			auto channelIt = items.find(c);

			if (channelIt == items.end()) {
				channelsArr.push_back(channelObj);
				continue;
			}

			if (numZSlices > 1) {
				nlohmann::json zArr = nlohmann::json::array();
				for (int z = 0; z < numZSlices; ++z) {
					nlohmann::json zObj = nlohmann::json::object();
					auto zIt = channelIt->second.find(z);

					if (zIt != channelIt->second.end()) {
						if (numTFrames > 1) {
							nlohmann::json tArr = nlohmann::json::array();
							std::vector<nlohmann::json*> tPtrs;
							for (int t = 0; t < numTFrames; ++t) {
								auto tIt = zIt->second.find(t);
								tArr.push_back(tIt != zIt->second.end()
								                ? std::move(tIt->second)
								                : nlohmann::json::object());
							}
							for (auto& tElem : tArr) tPtrs.push_back(&tElem);
							hoistInvariantTags(zObj, tPtrs);
							zObj["TFrames"] = std::move(tArr);
						} else {
							// Single t-frame: lift its tags directly onto zObj.
							auto tIt = zIt->second.find(0);
							if (tIt != zIt->second.end()) {
								zObj = std::move(tIt->second);
							}
						}
					}
					zArr.push_back(std::move(zObj));
				}
				std::vector<nlohmann::json*> zPtrs;
				for (auto& zElem : zArr) zPtrs.push_back(&zElem);
				hoistInvariantTags(channelObj, zPtrs);
				channelObj["ZSlices"] = std::move(zArr);
			} else {
				// Single z-slice.
				auto zIt = channelIt->second.find(0);
				if (zIt != channelIt->second.end()) {
					if (numTFrames > 1) {
						nlohmann::json tArr = nlohmann::json::array();
						std::vector<nlohmann::json*> tPtrs;
						for (int t = 0; t < numTFrames; ++t) {
							auto tIt = zIt->second.find(t);
							tArr.push_back(tIt != zIt->second.end()
							                ? std::move(tIt->second)
							                : nlohmann::json::object());
						}
						for (auto& tElem : tArr) tPtrs.push_back(&tElem);
						hoistInvariantTags(channelObj, tPtrs);
						channelObj["TFrames"] = std::move(tArr);
					} else {
						auto tIt = zIt->second.find(0);
						if (tIt != zIt->second.end()) {
							channelObj = std::move(tIt->second);
						}
					}
				}
			}

			channelsArr.push_back(std::move(channelObj));
		}

		root["Channels"] = std::move(channelsArr);
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

### Step 2.4: Build and run the failing tests

- [ ] Build:
```
python3 install.py -a build -c release
```
Expected: clean build. If `<optional>` isn't already pulled in via existing includes, add `#include <optional>` to the includes block.

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVIImageDriver.openSlide2D:ZVIImageDriver.openSlide3D"
```
Expected: PASS.

- [ ] Run the full ZVI suite:
```
./build/release/bin/slideio_tests --gtest_filter="ZVI*"
```
Expected: all PASS.

### Step 2.5: Commit

- [ ] Stage and commit:
```
git add src/slideio/drivers/zvi/zvislide.cpp src/tests/main/test_zvi_driver.cpp
git commit -m "$(cat <<'EOF'
zvi: serialize metadata as Channels/ZSlices/TFrames tree

Per-item tag streams (/Image/Item(n)/Tags/Contents) are now read for
every item and bucketed by (channel, z-slice, t-frame). Repeated tag
ids within any single stream collapse to JSON arrays. Tags whose value
is invariant across all t-frames in a z-slice are hoisted to the
z-slice object; tags invariant across z-slices of a channel are
hoisted to the channel object. The Channels array is always present;
ZSlices and TFrames are nested only when their dimension > 1.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Verify no regressions

**Files:** none.

### Step 3.1: Full ZVI suite

- [ ] Run:
```
./build/release/bin/slideio_tests --gtest_filter="ZVI*"
```
Expected: all PASS.

### Step 3.2: Full test binary

- [ ] Run:
```
./build/release/bin/slideio_tests
```
Expected: same pass/fail set as before the addendum (the pre-existing `AFIDriverFileTest.openFile` failure is unrelated and should still be the only failure).

### Step 3.3: Eyeball the JSON

- [ ] Optional: open `Zeiss-1-Merged.zvi` in a quick interactive session and dump `slide->getRawMetadata()`. Confirm the `Channels` array has 3 entries, each with a recognizable `Channel Name`. Confirm a 3D fixture (`Zeiss-1-Stacked.zvi`) has `Channels[0].ZSlices` with > 1 entry and that `Channel Name` is at the channel level, not duplicated inside ZSlices.

### Step 3.4: No commit

- [ ] Nothing to commit from Task 3.

---

## Self-Review

**Spec coverage:**

- §10.1 (scope additions) → Task 2 reads per-item streams.
- §10.2 (JSON shape: 2D, 3D, 4D) → Task 2 emits Channels always; ZSlices/TFrames nested conditionally on dimension count.
- §10.3 (repeat collapse) → Task 2 `tagsToJsonObject` implements first-scalar / second-becomes-array logic.
- §10.4 (hoisting algorithm) → Task 2 `hoistInvariantTags` plus the call ordering: t-hoist before constructing TFrames, then z-hoist before constructing ZSlices. No C-hoist (correct).
- §10.5 (index extraction via getImageItems) → Task 1 adds the accessor; Task 2 consumes it.
- §10.6 (assembly procedure) → Task 2 init() matches the seven steps.
- §10.7 (code organization) → Task 2 keeps `variantToJson`, replaces `mergeTags` with `tagsToJsonObject`, adds `hoistInvariantTags`. `tryReadTagsObject` is added as a small adapter; the spec didn't name it but it's a localized convenience.
- §10.8 (tests) → Task 2 covers `metadataHasChannelsArray` (channel count, channel names), `metadata3DHasZSlices` (ZSlices presence, hoist verification). The "no specific exposure values" test was dropped — the existing channel-name test already proves per-item tags reach the leaf.
- §10.9 (sparse items) → Task 2 fills missing slots with `nlohmann::json::object()` (empty `{}`).
- §10.10 (non-changes) → only `zviscene.hpp` gets the narrow accessor; `parseImageTags`, `readTags`, the enum, and `readAllTags` are untouched.

**Placeholder scan:** none. All steps contain runnable code or specific commands.

**Type consistency:** `tagsToJsonObject` returns `nlohmann::json`. `hoistInvariantTags` takes `nlohmann::json& parent` and `std::vector<nlohmann::json*>& children`. `tryReadTagsObject` returns `std::optional<nlohmann::json>`. `ZVIScene::getImageItems()` returns `const std::vector<ZVIImageItem>&`. The bucket map is `std::map<int, std::map<int, std::map<int, nlohmann::json>>>`. Items extract `getCIndex/getZIndex/getTIndex/getItemIndex` via existing public methods on `ZVIImageItem`. All matches.

**Edge cases handled in implementation:**

- `m_ImageItems` empty (degenerate file): bucket map empty, `Channels` array is not emitted, root contains only file-level tags. The `if (!items.empty())` guard.
- A channel missing entirely from items: `channelIt == items.end()` → empty channel object pushed.
- A z-slice missing in a channel that has others: empty z-slice object pushed.
- Single z-slice and single t-frame: tags lifted directly onto the channel object (no ZSlices/TFrames nesting).
- A per-item stream fails to read: `tryReadTagsObject` returns nullopt; the slot becomes `{}` instead of crashing.
- The `dynamic_pointer_cast<ZVIScene>` guard handles the (impossible-in-practice) case where `m_scene` somehow isn't a `ZVIScene`.

**One known cost:** the OLE document is opened twice per ZVI file open (once by `ZVIScene::init`, once by `ZVISlide::init`). This was already the case in the initial pass; the addendum doesn't change it.

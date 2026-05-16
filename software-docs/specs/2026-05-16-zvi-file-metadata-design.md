# ZVI File Metadata — Design

**Status:** Approved 2026-05-16 (initial flat-JSON pass). Tree-structure
addendum approved 2026-05-16 — see §10.
**Driver:** `src/slideio/drivers/zvi`
**Spec source:** `documents/image_formats/zvi/ZVI_Format_2009.pdf` (V 2.0.4, June 2009)

## 1. Goal

Expose the metadata embedded in ZVI files through the existing public metadata
API on `ZVISlide`:

- `ZVISlide::getRawMetadata()` returns a JSON-formatted string covering every
  tag found in the file.
- `ZVISlide::getMetadata()` returns the same data as a navigable
  `slideio::Metadata` tree.
- `ZVISlide::getMetadataFormat()` returns `MetadataFormat::JSON`.

Scene-level metadata (`ZVIScene::getRawMetadata`,
`ZVIScene::getMetadata`) is intentionally untouched and remains
`MetadataFormat::None`.

## 2. Current state

`ZVIScene::parseImageTags` reads the `/Image/Tags/Contents` token stream and
consumes only the dimension/scale/filename tags it needs; every other tag
token is read off the stream and discarded. The `ZVITAG` enum in
`zvitags.hpp` lists ~30 of the ~250 tag ids defined by the spec. `ZVISlide`
never populates `m_rawMetadata` or `m_metadataFormat` and does not override
`buildMetadataTree()`.

## 3. Scope

In scope:

- All tag ids documented in Section 3.4 of the ZVI 2009 spec.
- Unknown tag ids encountered at runtime (forward-compat: AxioVision has
  added ids since 2009).
- Tag sources read: the root `<Tags>` stream and the
  `/Image/Tags/Contents` stream of the `[Image]` storage.

Out of scope:

- OLE `\SummaryInformation` / `\DocumentSummaryInformation` property streams.
- Per-`[Item(n)]` tag metadata (channel/z/t-specific). The existing
  per-item tag scan in `ZVIImageItem::readTags` is unchanged.
- Thumbnail extraction.
- Scene-level metadata population.

## 4. Design

### 4.1 Expand `ZVITAG` enum and add a name lookup

File: `src/slideio/drivers/zvi/zvitags.hpp` (+ new `.cpp`).

- Add every tag id from spec Section 3.4 to the `ZVITAG` enum. Existing
  enumerators keep their values, so `ZVIScene::parseImageTags`'s `switch`
  statement remains correct.
- Add a free function:
  ```cpp
  const char* getZviTagName(int32_t id);   // returns nullptr if unknown
  ```
  Implemented as a `switch` over the enum so the compiler catches a missing
  entry once `-Wswitch` is enabled (it already is on clang in this driver).

### 4.2 Generic tag-list reader

File: `src/slideio/drivers/zvi/zviutils.{hpp,cpp}`.

New helper:

```cpp
struct ZviTagEntry {
    int32_t id;
    Variant value;
};

std::vector<ZviTagEntry> readAllTags(ole::basic_stream& stream,
                                     bool hasClsidHeader);
```

Behaviour:

1. If `hasClsidHeader`, skip 16 bytes (128-bit CLSID at the start of the
   root `<Tags>` stream; the `[Image]/[Tags]/<Contents>` stream has no
   CLSID).
2. Read `{Version}` (VT_I4) and `{Count}` (VT_I4) using existing helpers.
3. Loop `Count` times: read `{Value}` via `ZVIUtils::readItem` (already
   variant-typed), read `{TagID}` via `readIntItem`, skip `{Attribute}` via
   `skipItem`. Append `{id, value}`. Skip entries where the variant is
   `std::monostate` (EMPTY/NULL/unsupported types).

The helper is intentionally format-agnostic; callers decide what to do with
the variants. The existing `parseImageTags` is *not* rewritten on top of
this helper in this change — it stays as-is to avoid touching working
dimension-parsing code.

### 4.3 Populate metadata on `ZVISlide`

File: `src/slideio/drivers/zvi/zvislide.{hpp,cpp}`.

In `ZVISlide::init()`, after `m_scene.reset(...)`:

1. Open the `m_scene`'s compound document (re-opened locally — the
   document handle in `ZVIScene` is private, and a second open is cheap
   for an in-memory OLE structure; alternatively expose a getter, decided
   below in §5).
2. Read `/Image/Tags/Contents` via `readAllTags(stream, false)`.
3. If `/Tags` (root-level stream) exists, read it via
   `readAllTags(stream, true)`. Missing stream is not an error.
4. Build an `nlohmann::json` object:
   - Key: `getZviTagName(id)` if non-null, else `"Tag_<id>"` where `<id>`
     is the decimal id.
   - Value mapping from `Variant`:
     - `bool` → JSON bool
     - `int32_t`, `uint32_t`, `int64_t`, `uint64_t` → JSON integer
     - `double` → JSON number
     - `std::string` → JSON string
     - `std::monostate` entries are dropped before this point.
   - On duplicate keys, the root `<Tags>` stream wins (it is the
     standardized location per spec §2). Document this in a code comment.
5. Assign:
   ```cpp
   m_rawMetadata = json.dump(2);
   m_metadataFormat = MetadataFormat::JSON;
   ```

### 4.4 Override `buildMetadataTree`

```cpp
// zvislide.hpp
MetadataBuilder buildMetadataTree() const override;

// zvislide.cpp
MetadataBuilder ZVISlide::buildMetadataTree() const {
    return detail::builderFromJson(nlohmann::json::parse(m_rawMetadata));
}
```

`slideio/core/metadata_internal.hpp` already exports `builderFromJson`.

## 5. Open implementation detail: re-reading the OLE document

`ZVIScene::m_Doc` is private. Three options:

- **A. Open the OLE document a second time inside `ZVISlide::init()`.**
  Simplest, slight cost (one extra header parse). Recommended.
- B. Add `ZVIScene::getDocument()` accessor. Touches the scene's
  encapsulation for a slide-level concern.
- C. Move metadata reading into `ZVIScene::init()` and have
  `ZVISlide::init()` copy the result over. Couples Scene to Slide
  metadata, which we explicitly want to avoid.

**Decision: A.**

## 6. Tests

File: `src/tests/main/test_zvi_driver.cpp`.

### 6.1 Update existing assertion

Line ~48 of `test_zvi_driver.cpp` currently asserts:

```cpp
EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::None);
```

Change to:

```cpp
EXPECT_EQ(slide->getMetadataFormat(), slideio::MetadataFormat::JSON);
```

Line ~69 (`scene->getMetadataFormat() == None`) is unchanged — Scene
metadata stays off.

### 6.2 New test cases

Against the existing `Zeiss-1-Merged.zvi` fixture (already used by
`openSlide2D`):

- `ZVIImageDriver_metadataJsonNotEmpty` — `getRawMetadata()` is non-empty
  and parses as a JSON object.
- `ZVIImageDriver_metadataKnownTags` — `getMetadata()["Filename"]
  .asString()` equals the expected filename
  (`RQ26033_04310292C0004S_Calu3_amplified_100x_21Jun2012 ic zsm.zvi` — the
  same string the existing test asserts as `scene->getName()`).
- `ZVIImageDriver_metadataImageWidth` — `getMetadata()["Image Width
  (Pixel)"].asInt()` equals `1480` (matches `getRect().width` in the
  existing test).

Other fixtures (`Zeiss-1-Stacked.zvi`, etc.) get an analogous Filename
check to cover the 3D code path.

### 6.3 Unknown-tag round-trip

Only added if one of the existing fixtures actually contains an id not in
the spec table. Verified during implementation by logging unknown ids
once. If present, an assertion is added that the corresponding
`Tag_<id>` key exists in `getMetadata()`. If no fixture contains an
unknown id, this test is skipped (no synthetic fixtures created for this
change).

## 7. Risks and edge cases

- **String encoding.** ZVI strings are UTF-16; `ZVIUtils::readStringItem`
  already converts to UTF-8. JSON output uses UTF-8 directly — no extra
  handling needed.
- **VT_DATE values** come out of `readItem` as `double` (OLE date as days
  since 1899-12-30). They are serialized as numbers; conversion to a
  human-readable ISO date is out of scope.
- **Very large `Image Thumbnail` (id 261) blobs.** `readItem` skips
  unsupported variant types (currently it skips arrays/blobs); confirm
  during implementation that the thumbnail token does not bloat the JSON.
  If it does, add an explicit skip for id 261.
- **Backwards compat.** External callers that previously got `None` from
  `slide->getMetadataFormat()` on ZVI files will now get `JSON`. This is
  a deliberate behavior change.

## 8. Non-changes

- `ZVIScene::parseImageTags` is unchanged.
- `ZVIImageItem` is unchanged.
- No new Conan dependencies; `nlohmann_json` is already linked.

## 9. References

- `documents/image_formats/zvi/ZVI_Format_2009.pdf`
- `src/slideio/core/metadata.hpp`, `metadata_internal.hpp`
- `src/slideio/drivers/svs/svsslide.cpp::buildMetadataTree` — existing
  precedent for `builderFromJson` use.

---

## 10. Addendum — Tree structure (approved 2026-05-16)

The initial implementation (commits `5cbfd40..7812c1d`) merged every tag id
into a single flat JSON object. That loses per-channel and per-z/t-slice
detail: ZVI puts per-item metadata in `[Image]/[Item(n)]/[Tags]/<Contents>`
streams (one stream per (channel, z, t, scene) item), and even within the
file-level `/Image/Tags/Contents` a tag id can repeat (e.g., one
`ZVITAG_CHANNEL_NAME` per channel). A flat overwrite keeps only the last
value. This addendum extends the design to surface that structure.

### 10.1 Scope additions

In scope, new in this addendum:

- Per-`Item(n)` tag streams (`/Image/Item(n)/Tags/Contents`).
- Hierarchical assembly under `"Channels" → "ZSlices" → "TFrames"`.
- Hoisting of dimension-invariant tags up one level.
- Repeated-id collapse to JSON array within a single stream parse.

Still out of scope (unchanged):

- OLE `\SummaryInformation` / `\DocumentSummaryInformation`.
- Thumbnail extraction.
- Scene-level metadata population.

### 10.2 JSON shape

The shape adapts to scene dimensions:

**2D multi-channel** (e.g., `Zeiss-1-Merged.zvi`, 3 channels, 1 z, 1 t):

```json
{
  "Title": "...",
  "Image Width (Pixel)": 1480,
  ...file-level tags...
  "Channels": [
    { "Image Index C": 0, "Channel Name": "Hoechst 33342", "Exposure Time [ms]": 50.0, ... },
    { "Image Index C": 1, "Channel Name": "Cy3", "Exposure Time [ms]": 100.0, ... },
    { "Image Index C": 2, "Channel Name": "FITC", "Exposure Time [ms]": 200.0, ... }
  ]
}
```

**3D single-channel** (z > 1):

```json
{
  "Title": "...",
  "Channels": [
    {
      "Channel Name": "Hoechst 33342",      // hoisted: same across z-slices
      "Multichannel Colour": 4278255615,    // hoisted
      "ZSlices": [
        { "Image Index Z": 0, "Exposure Time [ms]": 50.0 },
        { "Image Index Z": 1, "Exposure Time [ms]": 50.0 }
      ]
    }
  ]
}
```

**4D multi-channel** (z > 1, t > 1):

```json
{
  "Channels": [
    {
      "Channel Name": "...",                 // hoisted
      "ZSlices": [
        {
          "Exposure Time [ms]": 50.0,        // hoisted: same across t-frames
          "TFrames": [
            { "Image Index T": 0, ... },
            { "Image Index T": 1, ... }
          ]
        }
      ]
    }
  ]
}
```

Rules:

- `"Channels"` is always present and always an array. Length == number of
  channels.
- `"ZSlices"` is present inside a channel object **only if** the scene has
  more than one z-slice (`scene->getNumZSlices() > 1`).
- `"TFrames"` is present inside a z-slice object **only if** the scene has
  more than one t-frame.
- Index of each array entry corresponds to the `ZVITAG_IMAGE_INDEX_*`
  value, not insertion order. Missing items (sparse files) leave gaps as
  empty objects `{}`.

### 10.3 Repeated-id collapse

Within a single tag stream, if the same tag id appears N > 1 times, the
JSON value becomes an array of those values in encounter order:

```text
First occurrence:  "Channel Name": "Hoechst 33342"
After second:      "Channel Name": ["Hoechst 33342", "Cy3"]
After third:       "Channel Name": ["Hoechst 33342", "Cy3", "FITC"]
```

This rule applies per-stream (file-level stream is one parse;
each item's stream is one parse). It is the only mechanism for capturing
parallel arrays inside a single stream.

### 10.4 Hoisting algorithm

After per-item tags are bucketed by (C, Z, T), hoist invariant tags
bottom-up:

1. **T-hoist:** For each (c, z) bucket with multiple t-frames, for each
   tag key present in all t-frames with bytewise-identical JSON values:
   remove the key from each t-frame and place it on the parent z-slice
   object.
2. **Z-hoist:** For each c bucket with multiple z-slices (after t-hoist),
   for each tag key present in all z-slices with bytewise-identical JSON
   values: remove the key from each z-slice and place it on the parent
   channel object.
3. **No C-hoist.** Tags that happen to be identical across all channels
   are not promoted to the root — file-level tags already live there, and
   moving values out of `"Channels"` would conflate two different
   sources (per-item vs file-level).

"Identical" means the JSON values compare equal via `nlohmann::json::operator==`
(strings, numbers, bools, arrays all supported). Missing-vs-present
counts as not-identical (no hoist).

### 10.5 Index extraction

The `ZVIScene` already populates each `ZVIImageItem` with its (C, Z, T)
indices (via `ZVIImageItem::readContents`, which parses the index BLOB in
`/Image/Item(n)/Contents`). `ZVISlide::init` can re-read the same data
independently, but to avoid duplicating the binary BLOB parser the design
adds a narrow accessor on `ZVIScene`:

```cpp
// zviscene.hpp (public)
const std::vector<ZVIImageItem>& getImageItems() const { return m_ImageItems; }
```

This is internal plumbing, not a metadata API change. `ZVIScene`'s own
`getRawMetadata`/`getMetadata`/`getMetadataFormat` remain untouched.

`ZVIImageItem` already exposes `getCIndex()`, `getZIndex()`, `getTIndex()`,
and `getItemIndex()` publicly, so no new methods are needed on it.

### 10.6 Tree assembly procedure

In `ZVISlide::init`, replacing the current flat-merge:

1. Read `/Image/Tags/Contents` via `readAllTags`, convert to a JSON
   object with repeat-collapse → assign to root.
2. (Optional) Read root `/Tags` via `readAllTags` if present, merge into
   root with overwrite. (Unchanged from initial design.)
3. For each `ZVIImageItem` in `scene->getImageItems()`:
   - Open `/Image/Item(n)/Tags/Contents` via `StreamKeeper`.
   - Read tags via `readAllTags(stream, false)`.
   - Convert to a JSON object with repeat-collapse.
   - Bucket under `(item.getCIndex(), item.getZIndex(), item.getTIndex())`.
     Use a sparse 3-level `std::map`.
   - Wrap reads in try/catch — corrupt per-item streams degrade
     gracefully (item gets `{}` instead of crashing the slide).
4. Run t-hoist, then z-hoist (per §10.4).
5. Emit final tree under `"Channels"` per the shape rules in §10.2.

### 10.7 Code organization

Two new translation-unit-local helpers in `zvislide.cpp`:

```cpp
// Convert a tag-entry vector into a JSON object with repeat-collapse.
nlohmann::json tagsToJsonObject(const std::vector<ZVIUtils::ZviTagEntry>& entries);

// Hoist tags present with identical values in every child up to parent.
// Mutates both parent and children.
void hoistInvariantTags(nlohmann::json& parent,
                        std::vector<nlohmann::json>& children);
```

The existing anonymous-namespace `variantToJson` and `mergeTags` are
replaced (mergeTags's "later writes win" semantics is no longer right for
per-stream parses — repeat-collapse replaces it).

### 10.8 Tests

Add:

- `ZVIImageDriver_metadataHasChannelsArray` — `Zeiss-1-Merged.zvi`:
  assert `getMetadata()["Channels"]` is an array of length 3, and each
  entry's `"Channel Name"` matches the expected name (`Hoechst 33342`,
  `Cy3`, `FITC`).
- `ZVIImageDriver_metadataChannelExposure` — same fixture: assert each
  channel has an `"Exposure Time [ms]"` field (don't pin specific values;
  just non-null).
- `ZVIImageDriver_metadata3DHasZSlices` — `Zeiss-1-Stacked.zvi`: assert
  `getMetadata()["Channels"][0]["ZSlices"]` is an array of expected
  length (numZSlices), and that the `Channel Name` tag is hoisted to the
  channel object (i.e., NOT present in each ZSlice entry).
- Update existing `openSlide2D` assertions to reflect the new shape (the
  three flat-keyed asserts on `Filename` and `Image Width (Pixel)` stay
  at root — those are file-level tags and don't move).

### 10.9 Risks and edge cases

- **Sparse items.** If a file has items only for some (c, z, t) tuples,
  the corresponding array slots contain `{}`. Consumers must handle that.
- **Hoisting on degenerate buckets.** If a channel has only one z-slice
  (and we therefore don't emit a `"ZSlices"` array), z-hoist is a no-op
  for that channel and per-item tags land directly on the channel object
  — this is exactly the 2D shape in §10.2.
- **`nlohmann::json::operator==` cost.** For hoisting, we compare every
  key's value across N children. With ~50 tags per item and N ≈ 10
  z-slices, that's ~500 comparisons of small JSON values. Negligible.
- **Memory.** The sparse bucket map plus the final tree double-holds tags
  briefly. For typical ZVI files (≤ a few hundred items) this is well
  under a megabyte.
- **Backwards compat within this branch.** The flat-JSON shape from the
  initial design (`5cbfd40..7812c1d`) is itself a behavior change vs
  master; the addendum changes the shape again before the v2.8.1 tag
  ships. External consumers should not have written code against the
  flat shape yet.

### 10.10 Non-changes

- `ZVIScene::parseImageTags` is still unchanged.
- `ZVIImageItem::readTags` is still unchanged.
- The `ZVITAG` enum and `getZviTagName` are unchanged.
- `ZVIUtils::readAllTags` is unchanged — it already returns the
  `ZviTagEntry` list this addendum needs.
- One narrow addition to `ZVIScene`: a single const accessor
  (`getImageItems()`) for the existing private `m_ImageItems` vector.

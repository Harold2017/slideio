# ZVI File Metadata — Design

**Status:** Approved 2026-05-16
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

# Metadata Tree API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose `Slide` and `Scene` metadata as a navigable, read-only tree via a new `slideio::Metadata` class, while keeping the existing `getRawMetadata()` API unchanged.

**Architecture:** A thin `Metadata` facade wraps `nlohmann::json` via pimpl, so the json library never appears in installed public headers (ABI-safe). Each `Metadata` is a *view* into a shared root JSON tree — children share the root's lifetime via a `std::shared_ptr<const nlohmann::json>` plus a raw sub-node pointer, so navigation is O(1) and never copies subtrees. `CVScene` and `CVSlide` gain a virtual `buildMetadataTree(json&)` hook plus a `std::call_once`-cached `getMetadata()`. The default implementation converts `MetadataFormat::{None,Text,JSON,XML}` to JSON; drivers may override to emit semantic structure later. An XML→JSON walker (tinyxml2-based) handles the XML case using the convention `@attr` for attributes, `#text` for text content, and arrays for repeated sibling tags.

**Tech Stack:** C++17, `nlohmann::json` (already a Conan dep, used in tests), `tinyxml2` (already linked into `slideio-core`), Google Test.

---

## File Structure

**Create:**
- `src/slideio/core/metadata.hpp` — public facade class `slideio::Metadata` (installed header, exported with `SLIDEIO_CORE_EXPORTS`).
- `src/slideio/core/metadata.cpp` — facade implementation and `detail::makeMetadataFromJson` factory.
- `src/slideio/core/metadata_internal.hpp` — non-installed header exposing internal factories (`detail::makeMetadataFromJson`, `detail::xmlStringToJson`) to other translation units inside `slideio-core`.
- `src/slideio/core/metadata_xml.cpp` — XML→JSON walker using tinyxml2.
- `src/tests/main/test_metadata.cpp` — unit tests for the facade, the XML walker, and the lazy-cache wiring.

**Modify:**
- `src/slideio/core/CMakeLists.txt` — add `find_package(nlohmann_json)` and link `nlohmann_json::nlohmann_json` (PRIVATE), add the four new source files.
- `src/slideio/core/cvscene.hpp` / `cvscene.cpp` — add `getMetadata()`, virtual `buildMetadataTree()`, members.
- `src/slideio/core/cvslide.hpp` / `cvslide.cpp` — same as `cvscene`.
- `src/slideio/slideio/scene.hpp` / `scene.cpp` — public delegation `getMetadata()`.
- `src/slideio/slideio/slide.hpp` / `slide.cpp` — public delegation `getMetadata()`.
- `src/tests/main/CMakeLists.txt` — add `test_metadata.cpp` to `TEST_SOURCES`.

---

## Conventions To Honor

- Library uses `SLIDEIO_CORE_EXPORTS` for `slideio-core` symbols, `SLIDEIO_EXPORTS` for the main `slideio` lib.
- Existing files use `#pragma once` (slideio module) and `#ifndef` guards (core module) — match the surrounding style.
- Tests use Google Test (`TEST()` / `EXPECT_*` / `ASSERT_*`); existing main suite uses GTest globals (no fixtures required for these tests).
- Build/run for tests on Windows (this dev's environment): `python install.py -a build -c release`, then `build\release\bin\slideio_tests.exe --gtest_filter=Metadata.*`.

---

## Task 1: Skeleton facade header (compiles, returns Null only)

**Files:**
- Create: `src/slideio/core/metadata.hpp`
- Modify: `src/slideio/core/CMakeLists.txt`
- Test: `src/tests/main/test_metadata.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

- [ ] **Step 1: Create the header skeleton**

`src/slideio/core/metadata.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_metadata_HPP
#define OPENCV_slideio_metadata_HPP

#include "slideio/core/slideio_core_def.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief Read-only tree view over slide/scene metadata.
     *
     * Built lazily by CVScene / CVSlide on first call to getMetadata(). Children
     * returned by operator[] / find() are lightweight views into the same root
     * tree and share its lifetime.
     */
    class SLIDEIO_CORE_EXPORTS Metadata
    {
    public:
        enum class Type { Null, Bool, Int, Double, String, Array, Object };

        Metadata();                                       // Null node
        ~Metadata();
        Metadata(const Metadata&);
        Metadata(Metadata&&) noexcept;
        Metadata& operator=(const Metadata&);
        Metadata& operator=(Metadata&&) noexcept;

        Type type() const;
        bool isNull()   const { return type() == Type::Null;   }
        bool isObject() const { return type() == Type::Object; }
        bool isArray()  const { return type() == Type::Array;  }

        bool        asBool()   const;
        int64_t     asInt()    const;
        double      asDouble() const;
        std::string asString() const;

        size_t   size() const;
        bool     contains(const std::string& key) const;
        Metadata operator[](const std::string& key) const;
        Metadata operator[](size_t index) const;
        Metadata find(const std::string& jsonPointer) const;
        std::vector<std::string> keys() const;

        std::string toJson(int indent = -1) const;

        struct Impl;
        static Metadata fromImpl(std::shared_ptr<const Impl> impl);

    private:
        explicit Metadata(std::shared_ptr<const Impl> impl);
        std::shared_ptr<const Impl> m_impl;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
```

- [ ] **Step 2: Create the implementation file with stubs**

`src/slideio/core/metadata.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/metadata.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace slideio
{
    struct Metadata::Impl
    {
        std::shared_ptr<const nlohmann::json> root;
        const nlohmann::json*                 view = nullptr;
    };

    namespace
    {
        const nlohmann::json& view(const std::shared_ptr<const Metadata::Impl>& p)
        {
            static const nlohmann::json kNull;
            return (p && p->view) ? *p->view : kNull;
        }
    }

    Metadata::Metadata() = default;
    Metadata::~Metadata() = default;
    Metadata::Metadata(const Metadata&) = default;
    Metadata::Metadata(Metadata&&) noexcept = default;
    Metadata& Metadata::operator=(const Metadata&) = default;
    Metadata& Metadata::operator=(Metadata&&) noexcept = default;
    Metadata::Metadata(std::shared_ptr<const Impl> impl) : m_impl(std::move(impl)) {}
    Metadata Metadata::fromImpl(std::shared_ptr<const Impl> impl)
    {
        return Metadata(std::move(impl));
    }

    Metadata::Type Metadata::type() const
    {
        using J = nlohmann::json;
        switch (view(m_impl).type())
        {
        case J::value_t::null:            return Type::Null;
        case J::value_t::object:          return Type::Object;
        case J::value_t::array:           return Type::Array;
        case J::value_t::string:          return Type::String;
        case J::value_t::boolean:         return Type::Bool;
        case J::value_t::number_integer:
        case J::value_t::number_unsigned: return Type::Int;
        case J::value_t::number_float:    return Type::Double;
        default:                          return Type::Null;
        }
    }

    bool        Metadata::asBool()   const { throw std::runtime_error("not implemented"); }
    int64_t     Metadata::asInt()    const { throw std::runtime_error("not implemented"); }
    double      Metadata::asDouble() const { throw std::runtime_error("not implemented"); }
    std::string Metadata::asString() const { throw std::runtime_error("not implemented"); }

    size_t   Metadata::size() const { return 0; }
    bool     Metadata::contains(const std::string&) const { return false; }
    Metadata Metadata::operator[](const std::string&) const { return Metadata(); }
    Metadata Metadata::operator[](size_t) const { return Metadata(); }
    Metadata Metadata::find(const std::string&) const { return Metadata(); }
    std::vector<std::string> Metadata::keys() const { return {}; }
    std::string Metadata::toJson(int indent) const { return view(m_impl).dump(indent); }
}
```

- [ ] **Step 3: Wire CMake — add nlohmann_json + new files to slideio-core**

Modify `src/slideio/core/CMakeLists.txt`. After line 19 (the `add_library` call) the file calls `find_package` for several deps; add `nlohmann_json` near them, and add `nlohmann_json::nlohmann_json` to the `target_link_libraries(... PRIVATE ...)` block. Also add the two new source files to `SOURCE_FILES`.

Specifically, in the `set(SOURCE_FILES ...)` block, append before the closing `)`:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata.cpp
```

Add after the existing `find_package(Tinyxml2 REQUIRED)` line:

```cmake
find_package(nlohmann_json REQUIRED)
```

Add to the `target_link_libraries(${LIBRARY_NAME} PRIVATE ...)` calls:

```cmake
target_link_libraries(${LIBRARY_NAME} PRIVATE nlohmann_json::nlohmann_json)
```

- [ ] **Step 4: Write a failing smoke test**

Create `src/tests/main/test_metadata.cpp`:

```cpp
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
```

- [ ] **Step 5: Add the test to the test executable**

Modify `src/tests/main/CMakeLists.txt`. In the `set(TEST_SOURCES ...)` block, add:

```cmake
  test_metadata.cpp
```

(Placement next to `test_dimensions.cpp` is fine.)

- [ ] **Step 6: Build and run the test**

```bash
python install.py -a build -c release
build\release\bin\slideio_tests.exe --gtest_filter=Metadata.DefaultIsNull
```

Expected: PASS (`[  PASSED  ] 1 test.`)

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/metadata.hpp src/slideio/core/metadata.cpp \
        src/slideio/core/CMakeLists.txt \
        src/tests/main/test_metadata.cpp src/tests/main/CMakeLists.txt
git commit -m "core: add Metadata facade skeleton over nlohmann::json"
```

---

## Task 2: Internal factory + scalar accessors

**Files:**
- Create: `src/slideio/core/metadata_internal.hpp`
- Modify: `src/slideio/core/metadata.cpp`
- Modify: `src/slideio/core/CMakeLists.txt`
- Test: `src/tests/main/test_metadata.cpp`

- [ ] **Step 1: Add a failing test for scalar accessors**

Append to `src/tests/main/test_metadata.cpp`:

```cpp
#include "slideio/core/metadata_internal.hpp"
#include <nlohmann/json.hpp>

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
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
build\release\bin\slideio_tests.exe --gtest_filter=Metadata.ScalarAccessors:Metadata.ScalarConversionsAcrossTypes
```

Expected: build failure (`metadata_internal.hpp` not found, `detail::makeMetadataFromJson` undefined).

- [ ] **Step 3: Create the internal header**

Create `src/slideio/core/metadata_internal.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_metadata_internal_HPP
#define OPENCV_slideio_metadata_internal_HPP

#include "slideio/core/metadata.hpp"
#include <nlohmann/json.hpp>

namespace slideio { namespace detail {

    SLIDEIO_CORE_EXPORTS Metadata    makeMetadataFromJson(nlohmann::json root);
    SLIDEIO_CORE_EXPORTS nlohmann::json xmlStringToJson(const std::string& xml);

}}

#endif
```

- [ ] **Step 4: Implement scalars + factory + navigation in metadata.cpp**

Replace the stub bodies of `asBool/asInt/asDouble/asString/size/contains/operator[]/find/keys` and add the factory. The full file becomes:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/metadata.hpp"
#include "slideio/core/metadata_internal.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace slideio
{
    struct Metadata::Impl
    {
        std::shared_ptr<const nlohmann::json> root;
        const nlohmann::json*                 view = nullptr;
    };

    namespace
    {
        const nlohmann::json& view(const std::shared_ptr<const Metadata::Impl>& p)
        {
            static const nlohmann::json kNull;
            return (p && p->view) ? *p->view : kNull;
        }
        Metadata makeChild(const std::shared_ptr<const Metadata::Impl>& parent,
                           const nlohmann::json& child)
        {
            auto impl = std::make_shared<Metadata::Impl>();
            impl->root = parent->root;
            impl->view = &child;
            return Metadata::fromImpl(impl);
        }
    }

    Metadata::Metadata() = default;
    Metadata::~Metadata() = default;
    Metadata::Metadata(const Metadata&) = default;
    Metadata::Metadata(Metadata&&) noexcept = default;
    Metadata& Metadata::operator=(const Metadata&) = default;
    Metadata& Metadata::operator=(Metadata&&) noexcept = default;
    Metadata::Metadata(std::shared_ptr<const Impl> impl) : m_impl(std::move(impl)) {}
    Metadata Metadata::fromImpl(std::shared_ptr<const Impl> impl)
    {
        return Metadata(std::move(impl));
    }

    Metadata::Type Metadata::type() const
    {
        using J = nlohmann::json;
        switch (view(m_impl).type())
        {
        case J::value_t::null:            return Type::Null;
        case J::value_t::object:          return Type::Object;
        case J::value_t::array:           return Type::Array;
        case J::value_t::string:          return Type::String;
        case J::value_t::boolean:         return Type::Bool;
        case J::value_t::number_integer:
        case J::value_t::number_unsigned: return Type::Int;
        case J::value_t::number_float:    return Type::Double;
        default:                          return Type::Null;
        }
    }

    bool Metadata::asBool() const
    {
        const auto& n = view(m_impl);
        if (n.is_boolean()) return n.get<bool>();
        if (n.is_number())  return n.get<double>() != 0.0;
        if (n.is_string())  { auto s = n.get<std::string>(); return s == "true" || s == "1"; }
        throw std::runtime_error("Metadata: not convertible to bool");
    }
    int64_t Metadata::asInt() const
    {
        const auto& n = view(m_impl);
        if (n.is_number_integer())  return n.get<int64_t>();
        if (n.is_number_unsigned()) return static_cast<int64_t>(n.get<uint64_t>());
        if (n.is_number_float())    return static_cast<int64_t>(n.get<double>());
        if (n.is_boolean())         return n.get<bool>() ? 1 : 0;
        if (n.is_string())          return std::stoll(n.get<std::string>());
        throw std::runtime_error("Metadata: not convertible to int");
    }
    double Metadata::asDouble() const
    {
        const auto& n = view(m_impl);
        if (n.is_number())  return n.get<double>();
        if (n.is_boolean()) return n.get<bool>() ? 1.0 : 0.0;
        if (n.is_string())  return std::stod(n.get<std::string>());
        throw std::runtime_error("Metadata: not convertible to double");
    }
    std::string Metadata::asString() const
    {
        const auto& n = view(m_impl);
        if (n.is_string()) return n.get<std::string>();
        if (n.is_null())   return {};
        return n.dump();
    }

    size_t Metadata::size() const
    {
        const auto& n = view(m_impl);
        return (n.is_object() || n.is_array()) ? n.size() : 0;
    }
    bool Metadata::contains(const std::string& key) const
    {
        const auto& n = view(m_impl);
        return n.is_object() && n.contains(key);
    }
    Metadata Metadata::operator[](const std::string& key) const
    {
        const auto& n = view(m_impl);
        if (!m_impl || !n.is_object() || !n.contains(key)) return Metadata();
        return makeChild(m_impl, n.at(key));
    }
    Metadata Metadata::operator[](size_t i) const
    {
        const auto& n = view(m_impl);
        if (!m_impl || !n.is_array() || i >= n.size()) return Metadata();
        return makeChild(m_impl, n.at(i));
    }
    Metadata Metadata::find(const std::string& pointer) const
    {
        if (!m_impl) return Metadata();
        try
        {
            nlohmann::json::json_pointer ptr(pointer);
            const auto& target = view(m_impl).at(ptr);
            return makeChild(m_impl, target);
        }
        catch (...) { return Metadata(); }
    }
    std::vector<std::string> Metadata::keys() const
    {
        const auto& n = view(m_impl);
        std::vector<std::string> out;
        if (n.is_object())
        {
            out.reserve(n.size());
            for (auto it = n.begin(); it != n.end(); ++it) out.push_back(it.key());
        }
        return out;
    }
    std::string Metadata::toJson(int indent) const { return view(m_impl).dump(indent); }

    namespace detail {
        Metadata makeMetadataFromJson(nlohmann::json root)
        {
            auto impl    = std::make_shared<Metadata::Impl>();
            auto rootPtr = std::make_shared<const nlohmann::json>(std::move(root));
            impl->root   = rootPtr;
            impl->view   = rootPtr.get();
            return Metadata::fromImpl(impl);
        }
    }
}
```

- [ ] **Step 5: Add the new header to CMake source list**

Modify `src/slideio/core/CMakeLists.txt`. In `set(SOURCE_FILES ...)`, add:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata_internal.hpp
```

- [ ] **Step 6: Build and verify tests pass**

```bash
python install.py -a build -c release
build\release\bin\slideio_tests.exe --gtest_filter=Metadata.*
```

Expected: 3 tests pass (`DefaultIsNull`, `ScalarAccessors`, `ScalarConversionsAcrossTypes`).

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/metadata.hpp src/slideio/core/metadata.cpp \
        src/slideio/core/metadata_internal.hpp \
        src/slideio/core/CMakeLists.txt \
        src/tests/main/test_metadata.cpp
git commit -m "core: implement Metadata scalar accessors and JSON factory"
```

---

## Task 3: Navigation, json_pointer, and lifetime

**Files:**
- Test: `src/tests/main/test_metadata.cpp`

- [ ] **Step 1: Add failing tests for navigation, find(), keys(), and lifetime**

Append to `src/tests/main/test_metadata.cpp`:

```cpp
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
```

- [ ] **Step 2: Run tests to verify they pass (no impl changes needed — Task 2 already implemented these paths)**

```bash
build\release\bin\slideio_tests.exe --gtest_filter=Metadata.*
```

Expected: all Metadata tests pass.

- [ ] **Step 3: Commit**

```bash
git add src/tests/main/test_metadata.cpp
git commit -m "core: add Metadata navigation, json_pointer, and lifetime tests"
```

---

## Task 4: XML → JSON walker

**Files:**
- Create: `src/slideio/core/metadata_xml.cpp`
- Modify: `src/slideio/core/CMakeLists.txt`
- Test: `src/tests/main/test_metadata.cpp`

- [ ] **Step 1: Add failing tests for the walker**

Append to `src/tests/main/test_metadata.cpp`:

```cpp
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
```

- [ ] **Step 2: Run to verify failure**

```bash
build\release\bin\slideio_tests.exe --gtest_filter=MetadataXml.*
```

Expected: link error (`detail::xmlStringToJson` undefined).

- [ ] **Step 3: Implement the walker**

Create `src/slideio/core/metadata_xml.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/metadata_internal.hpp"
#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#include <string>

namespace slideio { namespace detail {

using nlohmann::json;
using tinyxml2::XMLElement;

namespace
{
    json elementToJson(const XMLElement* el);

    void addChild(json& parent, const std::string& key, json child)
    {
        auto it = parent.find(key);
        if (it == parent.end())
        {
            parent[key] = std::move(child);
            return;
        }
        if (it->is_array())
        {
            it->push_back(std::move(child));
            return;
        }
        json arr = json::array();
        arr.push_back(std::move(*it));
        arr.push_back(std::move(child));
        *it = std::move(arr);
    }

    json elementToJson(const XMLElement* el)
    {
        json node = json::object();

        for (const auto* a = el->FirstAttribute(); a; a = a->Next())
        {
            node[std::string("@") + a->Name()] = a->Value();
        }

        bool hasElementChild = false;
        for (const XMLElement* c = el->FirstChildElement(); c; c = c->NextSiblingElement())
        {
            hasElementChild = true;
            addChild(node, c->Name(), elementToJson(c));
        }

        const char* txt = el->GetText();
        if (txt && *txt)
        {
            if (!hasElementChild && node.empty())
            {
                return json(txt);
            }
            node["#text"] = txt;
        }
        return node;
    }
}

json xmlStringToJson(const std::string& xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS)
    {
        const char* err = doc.ErrorStr();
        return json{{"#error", err ? err : "xml parse error"}};
    }
    const XMLElement* root = doc.RootElement();
    if (!root) return json::object();
    json out = json::object();
    out[root->Name()] = elementToJson(root);
    return out;
}

}}
```

- [ ] **Step 4: Add the new file to CMake**

Modify `src/slideio/core/CMakeLists.txt`. In `set(SOURCE_FILES ...)`, add:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata_xml.cpp
```

- [ ] **Step 5: Build and verify**

```bash
python install.py -a build -c release
build\release\bin\slideio_tests.exe --gtest_filter=MetadataXml.*
```

Expected: 6 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/metadata_xml.cpp src/slideio/core/CMakeLists.txt \
        src/tests/main/test_metadata.cpp
git commit -m "core: add XML to JSON walker for metadata trees"
```

---

## Task 5: CVScene::getMetadata() with default buildMetadataTree()

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`
- Modify: `src/slideio/core/cvscene.cpp`
- Test: `src/tests/main/test_metadata.cpp`

- [ ] **Step 1: Write failing tests using a minimal subclass of CVScene**

The simplest path is to use the existing `CVSmallScene` or to create a tiny test-only subclass. `CVSmallScene` already exists in core but may have additional state — for unit testing the metadata path, define a local subclass in the test file.

Append to `src/tests/main/test_metadata.cpp`:

```cpp
#include "slideio/core/cvscene.hpp"

namespace
{
    class TestScene : public slideio::CVScene
    {
    public:
        TestScene(const std::string& raw, slideio::MetadataFormat fmt)
        {
            // Requires m_rawMetadata / m_metadataFormat to be protected,
            // which is done in Step 6.
            m_rawMetadata    = raw;
            m_metadataFormat = fmt;
        }
        // CVScene is abstract; provide minimal stubs.
        std::string getName() const override { return "test"; }
        cv::Rect    getRect() const override { return {0,0,1,1}; }
        int         getNumChannels() const override { return 1; }
        slideio::DataType getChannelDataType(int) const override
        { return slideio::DataType::DT_Byte; }
        slideio::Resolution getResolution() const override { return {0,0}; }
        double      getMagnification() const override { return 0; }
        slideio::Compression getCompression() const override
        { return slideio::Compression::Uncompressed; }
        void readResampledBlockChannelsEx(const cv::Rect&, const cv::Size&,
                                          const std::vector<int>&,
                                          cv::OutputArray, const std::list<std::string>&) override {}
    };
}

TEST(MetadataScene, FormatNoneGivesEmptyObject)
{
    TestScene s("", slideio::MetadataFormat::None);
    const auto& m = s.getMetadata();
    EXPECT_TRUE(m.isObject());
    EXPECT_EQ(m.size(), 0u);
}

TEST(MetadataScene, FormatTextWrapsInTextField)
{
    TestScene s("hello world", slideio::MetadataFormat::Text);
    const auto& m = s.getMetadata();
    EXPECT_EQ(m["text"].asString(), "hello world");
}

TEST(MetadataScene, FormatJsonParses)
{
    TestScene s(R"({"k":42})", slideio::MetadataFormat::JSON);
    const auto& m = s.getMetadata();
    EXPECT_EQ(m["k"].asInt(), 42);
}

TEST(MetadataScene, FormatJsonMalformedIncludesErrorAndRaw)
{
    TestScene s("{not json", slideio::MetadataFormat::JSON);
    const auto& m = s.getMetadata();
    EXPECT_TRUE(m.contains("#error"));
    EXPECT_EQ(m["raw"].asString(), "{not json");
}

TEST(MetadataScene, FormatXmlGoesThroughWalker)
{
    TestScene s(R"(<R a="1"><Item>x</Item></R>)", slideio::MetadataFormat::XML);
    const auto& m = s.getMetadata();
    EXPECT_EQ(m["R"]["@a"].asString(), "1");
    EXPECT_EQ(m["R"]["Item"].asString(), "x");
}

TEST(MetadataScene, GetMetadataIsCached)
{
    TestScene s(R"({"k":1})", slideio::MetadataFormat::JSON);
    const auto& a = s.getMetadata();
    const auto& b = s.getMetadata();
    EXPECT_EQ(&a, &b);  // same reference => cached
}
```

> **Note for the engineer:** The exact pure-virtual signatures of `CVScene` you must override may differ. Read `src/slideio/core/cvscene.hpp` and override exactly what the compiler reports as missing. Keep the test stubs minimal.

- [ ] **Step 2: Run to confirm failure**

```bash
build\release\bin\slideio_tests.exe --gtest_filter=MetadataScene.*
```

Expected: build error — `getMetadata()` not found, `m_rawMetadata`/`m_metadataFormat` may be private.

- [ ] **Step 3: Modify cvscene.hpp — add public getMetadata, virtual buildMetadataTree, members**

Open `src/slideio/core/cvscene.hpp`. Locate the existing public block ending with `getRawMetadata` (around line 195) and add right after the metadata-related public methods:

```cpp
        /**@brief returns metadata as a navigable tree. Built lazily on first call. */
        const Metadata& getMetadata() const;
```

Add the include near the top (after the existing core/base includes):

```cpp
#include "slideio/core/metadata.hpp"
```

In the `protected:` section (or add one if absent), add:

```cpp
    protected:
        /**@brief Driver hook that converts m_rawMetadata into a JSON tree.
         * Default implementation handles MetadataFormat::{None,Text,JSON,XML}. */
        virtual void buildMetadataTree(class JsonTreeRoot& root) const;
```

Wait — exposing `nlohmann::json` in cvscene.hpp would re-introduce the dependency we wanted to avoid. To keep nlohmann out of the installed header, declare the hook with an opaque proxy. Replace the protected block above with:

```cpp
    protected:
        /**@brief Driver hook that converts m_rawMetadata into a JSON tree.
         * Default implementation handles MetadataFormat::{None,Text,JSON,XML}.
         * `rootHandle` is a type-erased pointer to nlohmann::json, valid only
         * for the duration of the call. Use detail::asJson(rootHandle) inside
         * a .cpp that includes metadata_internal.hpp. */
        virtual void buildMetadataTree(void* rootHandle) const;
```

In the `private:` (or near `m_rawMetadata`) members block:

```cpp
        mutable std::once_flag m_metadataOnce;
        mutable Metadata       m_metadata;
```

Make sure `<mutex>` is included (it already is at line 15).

- [ ] **Step 4: Modify cvscene.cpp — implement getMetadata() and the default builder**

Open `src/slideio/core/cvscene.cpp`. Add includes near the top:

```cpp
#include "slideio/core/metadata_internal.hpp"
#include <nlohmann/json.hpp>
```

At the bottom of the file (or near the existing metadata-related methods), add:

```cpp
namespace slideio
{
    void CVScene::buildMetadataTree(void* rootHandle) const
    {
        using nlohmann::json;
        auto& root = *static_cast<json*>(rootHandle);
        switch (m_metadataFormat)
        {
        case MetadataFormat::JSON:
            try { root = json::parse(m_rawMetadata); }
            catch (...) {
                root = json{{"#error", "invalid json"}, {"raw", m_rawMetadata}};
            }
            break;
        case MetadataFormat::XML:
            root = detail::xmlStringToJson(m_rawMetadata);
            break;
        case MetadataFormat::Text:
            root = json{{"text", m_rawMetadata}};
            break;
        case MetadataFormat::None:
        default:
            root = json::object();
            break;
        }
    }

    const Metadata& CVScene::getMetadata() const
    {
        std::call_once(m_metadataOnce, [this]
        {
            nlohmann::json root;
            buildMetadataTree(&root);
            m_metadata = detail::makeMetadataFromJson(std::move(root));
        });
        return m_metadata;
    }
}
```

> **Why `void*` for the hook:** `nlohmann::json` would otherwise leak into `cvscene.hpp`, defeating the ABI insulation. Drivers that override the hook live inside the same shared library and can include `metadata_internal.hpp` to cast back to `nlohmann::json&`.

- [ ] **Step 5: Provide a typed helper for driver overrides**

Append to `src/slideio/core/metadata_internal.hpp` (inside the `slideio::detail` namespace):

```cpp
    inline nlohmann::json& asJson(void* handle)
    {
        return *static_cast<nlohmann::json*>(handle);
    }
```

- [ ] **Step 6: Make m_rawMetadata / m_metadataFormat accessible to the test stub**

The test subclass needs to set these. They're already in the `private:` section of `CVScene` per the existing code (`m_rawMetadata`, `m_metadataFormat`). Two clean options:

1. Move them to `protected:` in `cvscene.hpp`.
2. Add a protected setter `void setMetadata(const std::string&, MetadataFormat)`.

Pick option 1 — simpler and matches how drivers already initialize them via direct member access in subclasses. Move the lines:

```cpp
        std::string m_rawMetadata;
        MetadataFormat m_metadataFormat = MetadataFormat::None;
```

from `private:` (around lines 228–229) to a new `protected:` section before them. After this change the test subclass's direct assignment in its constructor compiles.

- [ ] **Step 7: Build and run**

```bash
python install.py -a build -c release
build\release\bin\slideio_tests.exe --gtest_filter=MetadataScene.*
```

Expected: 6 tests pass. If the test subclass fails to compile due to other pure-virtual overrides, add the minimum stubs the compiler asks for.

- [ ] **Step 8: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp \
        src/slideio/core/metadata_internal.hpp \
        src/tests/main/test_metadata.cpp
git commit -m "core: add CVScene::getMetadata() with lazy-cached default builder"
```

---

## Task 6: CVSlide::getMetadata() — mirror of Task 5

**Files:**
- Modify: `src/slideio/core/cvslide.hpp`
- Modify: `src/slideio/core/cvslide.cpp`
- Test: `src/tests/main/test_metadata.cpp`

- [ ] **Step 1: Add failing test**

Append to `src/tests/main/test_metadata.cpp`:

```cpp
#include "slideio/core/cvslide.hpp"

namespace
{
    class TestSlide : public slideio::CVSlide
    {
    public:
        TestSlide(const std::string& raw, slideio::MetadataFormat fmt)
        {
            m_rawMetadata    = raw;
            m_metadataFormat = fmt;
        }
        int getNumScenes() const override { return 0; }
        std::string getFilePath() const override { return {}; }
        std::shared_ptr<slideio::CVScene> getScene(int) const override { return {}; }
    };
}

TEST(MetadataSlide, JsonFormatParses)
{
    TestSlide s(R"({"vendor":"Aperio"})", slideio::MetadataFormat::JSON);
    const auto& m = s.getMetadata();
    EXPECT_EQ(m["vendor"].asString(), "Aperio");
}

TEST(MetadataSlide, XmlFormatGoesThroughWalker)
{
    TestSlide s(R"(<Slide><Vendor>Hamamatsu</Vendor></Slide>)",
                slideio::MetadataFormat::XML);
    const auto& m = s.getMetadata();
    EXPECT_EQ(m["Slide"]["Vendor"].asString(), "Hamamatsu");
}

TEST(MetadataSlide, IsCached)
{
    TestSlide s(R"({"k":1})", slideio::MetadataFormat::JSON);
    EXPECT_EQ(&s.getMetadata(), &s.getMetadata());
}
```

> **Note for the engineer:** Adjust the `TestSlide` overrides if `CVSlide`'s pure-virtuals differ from those listed.

- [ ] **Step 2: Run to confirm failure**

```bash
build\release\bin\slideio_tests.exe --gtest_filter=MetadataSlide.*
```

Expected: build error — `getMetadata` not on CVSlide, members private.

- [ ] **Step 3: Modify cvslide.hpp**

Open `src/slideio/core/cvslide.hpp`. Add include near the top:

```cpp
#include "slideio/core/metadata.hpp"
#include <mutex>
```

Add to the public section (next to `getRawMetadata`):

```cpp
        /**@brief returns metadata as a navigable tree. Built lazily on first call. */
        const Metadata& getMetadata() const;
```

Add to a `protected:` section:

```cpp
    protected:
        virtual void buildMetadataTree(void* rootHandle) const;
        std::string    m_rawMetadata;
        MetadataFormat m_metadataFormat = MetadataFormat::None;
```

Move the existing `m_rawMetadata` / `m_metadataFormat` lines from `private:` (around lines 79–80) into the new `protected:` section, deleting the duplicates.

In the `private:` section add:

```cpp
        mutable std::once_flag m_metadataOnce;
        mutable Metadata       m_metadata;
```

- [ ] **Step 4: Modify cvslide.cpp — implement**

Open `src/slideio/core/cvslide.cpp`. Add includes:

```cpp
#include "slideio/core/metadata_internal.hpp"
#include <nlohmann/json.hpp>
```

Append to the file:

```cpp
namespace slideio
{
    void CVSlide::buildMetadataTree(void* rootHandle) const
    {
        using nlohmann::json;
        auto& root = *static_cast<json*>(rootHandle);
        switch (m_metadataFormat)
        {
        case MetadataFormat::JSON:
            try { root = json::parse(m_rawMetadata); }
            catch (...) {
                root = json{{"#error", "invalid json"}, {"raw", m_rawMetadata}};
            }
            break;
        case MetadataFormat::XML:
            root = detail::xmlStringToJson(m_rawMetadata);
            break;
        case MetadataFormat::Text:
            root = json{{"text", m_rawMetadata}};
            break;
        case MetadataFormat::None:
        default:
            root = json::object();
            break;
        }
    }

    const Metadata& CVSlide::getMetadata() const
    {
        std::call_once(m_metadataOnce, [this]
        {
            nlohmann::json root;
            buildMetadataTree(&root);
            m_metadata = detail::makeMetadataFromJson(std::move(root));
        });
        return m_metadata;
    }
}
```

- [ ] **Step 5: Build and run**

```bash
python install.py -a build -c release
build\release\bin\slideio_tests.exe --gtest_filter=MetadataSlide.*
```

Expected: 3 tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/core/cvslide.hpp src/slideio/core/cvslide.cpp \
        src/tests/main/test_metadata.cpp
git commit -m "core: add CVSlide::getMetadata() with lazy-cached default builder"
```

---

## Task 7: Public `Scene::getMetadata()` and `Slide::getMetadata()`

**Files:**
- Modify: `src/slideio/slideio/scene.hpp`
- Modify: `src/slideio/slideio/scene.cpp`
- Modify: `src/slideio/slideio/slide.hpp`
- Modify: `src/slideio/slideio/slide.cpp`
- Test: `src/tests/main/test_metadata.cpp`

- [ ] **Step 1: Add failing end-to-end test**

Append to `src/tests/main/test_metadata.cpp`:

```cpp
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/slideio/scene.hpp"
#include "tests/testlib/testtools.hpp"   // or whatever path the existing tests use
                                         // for sample-file resolution

TEST(MetadataPublicApi, SceneAndSlideExposeTreeForRealSvs)
{
    // Use a small SVS sample fixture that the existing test suite already uses.
    // Search test_svs_driver.cpp for the helper used to locate fixtures and
    // reuse it — do NOT hardcode a path. As a placeholder:
    std::string path = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");

    auto slide = slideio::openSlide(path, "SVS");
    ASSERT_TRUE(slide);

    const auto& slideMeta = slide->getMetadata();
    EXPECT_FALSE(slideMeta.isNull());

    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    const auto& sceneMeta = scene->getMetadata();
    EXPECT_FALSE(sceneMeta.isNull());

    // toJson must always succeed and be valid JSON
    auto roundtrip = nlohmann::json::parse(slideMeta.toJson());
    SUCCEED();
}
```

> **Note for the engineer:** Look at `test_svs_driver.cpp` for the existing fixture-path helper (likely `TestTools::getTestImagePath` or similar). Use whatever name the codebase actually uses. If no SVS fixture is available in CI, this test can be guarded with `GTEST_SKIP()` when the file does not exist.

- [ ] **Step 2: Run to confirm failure**

```bash
build\release\bin\slideio_tests.exe --gtest_filter=MetadataPublicApi.*
```

Expected: build error — `Slide::getMetadata` / `Scene::getMetadata` not found.

- [ ] **Step 3: Add to scene.hpp**

In `src/slideio/slideio/scene.hpp`, add include near the top:

```cpp
#include "slideio/core/metadata.hpp"
```

Inside the `Scene` class public section (next to `getRawMetadata` around line 265), add:

```cpp
        /**@brief returns metadata as a navigable tree. Built lazily on first call. */
        const Metadata& getMetadata() const;
```

- [ ] **Step 4: Add to scene.cpp**

In `src/slideio/slideio/scene.cpp`, append the implementation:

```cpp
const slideio::Metadata& slideio::Scene::getMetadata() const
{
    return m_scene->getMetadata();
}
```

(Match the style of neighboring delegations.)

- [ ] **Step 5: Add to slide.hpp**

In `src/slideio/slideio/slide.hpp`, add include:

```cpp
#include "slideio/core/metadata.hpp"
```

Add public method (next to `getRawMetadata` around line 49):

```cpp
        /**@brief returns metadata as a navigable tree. Built lazily on first call. */
        const Metadata& getMetadata() const;
```

- [ ] **Step 6: Add to slide.cpp**

Append:

```cpp
const slideio::Metadata& slideio::Slide::getMetadata() const
{
    return m_slide->getMetadata();
}
```

- [ ] **Step 7: Build and run**

```bash
python install.py -a build -c release
build\release\bin\slideio_tests.exe --gtest_filter=Metadata*
```

Expected: all Metadata tests pass, including `MetadataPublicApi.SceneAndSlideExposeTreeForRealSvs` (or skipped if fixture unavailable).

- [ ] **Step 8: Commit**

```bash
git add src/slideio/slideio/scene.hpp src/slideio/slideio/scene.cpp \
        src/slideio/slideio/slide.hpp src/slideio/slideio/slide.cpp \
        src/tests/main/test_metadata.cpp
git commit -m "slideio: expose Scene::getMetadata and Slide::getMetadata"
```

---

## Task 8: Documentation note in CLAUDE.md / public docs

**Files:**
- Modify: `docs/cpp.md` (or whichever public doc lists the C++ API surface)

- [ ] **Step 1: Add a short section describing the new API**

Locate the section that documents `Slide` / `Scene` accessors (search `getRawMetadata` in `docs/cpp.md`). Add a paragraph after it:

```markdown
### Structured metadata

Both `Slide` and `Scene` also expose metadata as a tree via:

```cpp
const slideio::Metadata& meta = scene->getMetadata();
auto sizeX = meta.find("/Image/Pixels/@SizeX").asInt();
```

The returned `Metadata` is a read-only view — children are obtained via `operator[]`,
indexing, or RFC-6901 JSON pointers passed to `find()`. The tree is built lazily on
first access. For drivers whose source format is XML, attributes appear under `@name`,
text content under `#text`, and repeated sibling tags collapse into arrays.
```

- [ ] **Step 2: Commit**

```bash
git add docs/cpp.md
git commit -m "docs: document the structured-metadata tree API"
```

---

## Out of scope (explicit non-goals)

- Per-driver semantic overrides of `buildMetadataTree` (e.g., DCM tag/VR/value, CZI element extraction). These should each be their own follow-up plan, one per driver.
- Mutation of `Metadata` (setters, builders for application code) — keep the surface read-only.
- Python binding updates — the bindings live in a separate repo and will be a separate plan.
- Iterators over arrays / range-for support — the current `operator[](size_t)` + `size()` is sufficient for the v1 surface.

---

## Definition of done

- [ ] All Metadata*, MetadataXml, MetadataScene, MetadataSlide unit tests pass on Windows release build.
- [ ] `MetadataPublicApi.SceneAndSlideExposeTreeForRealSvs` passes (or is correctly skipped) using a real SVS fixture.
- [ ] `getRawMetadata()` and `getMetadataFormat()` continue to behave exactly as before (no regressions in existing tests).
- [ ] No `nlohmann/json.hpp` include appears in any installed public header.
- [ ] Eight focused commits, one per task.

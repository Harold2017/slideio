// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/metadata.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/base/exceptions.hpp"
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

        void buildDefaultMetadataTree(nlohmann::json& root,
                                      const std::string& rawMetadata,
                                      MetadataFormat fmt)
        {
            using nlohmann::json;
            switch (fmt)
            {
            case MetadataFormat::JSON:
                try { root = json::parse(rawMetadata); }
                catch (...) {
                    root = json{{"#error", "invalid json"}};
                }
                break;
            case MetadataFormat::XML:
                root = xmlStringToJson(rawMetadata);
                break;
            case MetadataFormat::Text:
                root = json{{"text", rawMetadata}};
                break;
            case MetadataFormat::None:
            default:
                root = json::object();
                break;
            }
        }
    }

    struct MetadataBuilder::Impl
    {
        std::shared_ptr<nlohmann::json> root;   // shared owner of the tree
        nlohmann::json*                 view = nullptr;   // points into *root
    };

    MetadataBuilder::MetadataBuilder()
        : m_impl(std::make_shared<Impl>())
    {
        m_impl->root = std::make_shared<nlohmann::json>();   // default = Null
        m_impl->view = m_impl->root.get();
    }
    MetadataBuilder::~MetadataBuilder() = default;
    MetadataBuilder::MetadataBuilder(const MetadataBuilder&) = default;
    MetadataBuilder::MetadataBuilder(MetadataBuilder&&) noexcept = default;
    MetadataBuilder& MetadataBuilder::operator=(const MetadataBuilder&) = default;
    MetadataBuilder& MetadataBuilder::operator=(MetadataBuilder&&) noexcept = default;
    MetadataBuilder::MetadataBuilder(std::shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}

    void MetadataBuilder::set(const std::string& value)
    {
        *m_impl->view = value;
    }
    void MetadataBuilder::set(bool value)
    {
        *m_impl->view = value;
    }
    void MetadataBuilder::set(int64_t value)
    {
        *m_impl->view = value;
    }
    void MetadataBuilder::set(double value)
    {
        *m_impl->view = value;
    }
    void MetadataBuilder::set(const char* value)
    {
        if (value == nullptr) {
            RAISE_RUNTIME_ERROR << "MetadataBuilder::set: const char* value must not be null";
        }
        *m_impl->view = std::string(value);
    }

    bool MetadataBuilder::isNull() const
    {
        return m_impl->view->is_null();
    }

    Metadata MetadataBuilder::freeze() const
    {
        return detail::makeMetadataFromJson(nlohmann::json(*m_impl->view));
    }
}

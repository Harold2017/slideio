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

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

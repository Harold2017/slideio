// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"
#include "slideio/drivers/zvi/zvitags.hpp"
#include "slideio/core/metadata_internal.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/base/log.hpp"
#include <nlohmann/json.hpp>
#include <pole/storage.hpp>
#include <type_traits>
#include <variant>

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
	} catch (const std::exception& e) {
		SLIDEIO_LOG(WARNING) << "ZVIImageDriver: failed to read /Image/Tags/Contents: " << e.what();
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

// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svstools.hpp"
#include <string>
#include <regex>
#include <vector>

using namespace slideio;

namespace {
    std::string trimWS(const std::string& s) {
        const size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return {};
        const size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }
}

int SVSTools::extractMagnifiation(const std::string& description)
{
    int magn = 0;
    std::regex rgx("\\|AppMag\\s=\\s(\\d+)\\|");
    std::smatch match;
    if(std::regex_search(description, match, rgx)){
        std::string magn_str = match[1];
        magn = std::stoi(magn_str);
    }
    return magn;
}

double SVSTools::extractResolution(const std::string& description)
{
    double res = 0;
    std::regex rgx(R"(\|MPP\s=\s((\d*(\.|,))?(\d+)?))");
    std::smatch match;
    if (std::regex_search(description, match, rgx)) {
        std::string res_str = match[1];
        std::replace(res_str.begin(), res_str.end(), ',', '.');
        res = std::stod(res_str) * 1.e-6;
    }
    return res;
}

nlohmann::json SVSTools::parseAperioMetadata(const std::string& description)
{
    using nlohmann::json;
    json result = json::object();

    const size_t firstPipe = description.find('|');
    const std::string header = (firstPipe == std::string::npos)
        ? description
        : description.substr(0, firstPipe);

    std::vector<std::string> headerLines;
    {
        std::string current;
        for (char c : header) {
            if (c == '\n') {
                std::string t = trimWS(current);
                if (!t.empty()) headerLines.push_back(std::move(t));
                current.clear();
            } else if (c != '\r') {
                current += c;
            }
        }
        std::string t = trimWS(current);
        if (!t.empty()) headerLines.push_back(std::move(t));
    }

    if (headerLines.size() >= 1) result["application"] = headerLines[0];
    if (headerLines.size() >= 2) result["image"]       = headerLines[1];

    if (firstPipe != std::string::npos) {
        json props = json::object();
        size_t cursor = firstPipe + 1;
        while (cursor <= description.size()) {
            const size_t next = description.find('|', cursor);
            const std::string token = (next == std::string::npos)
                ? description.substr(cursor)
                : description.substr(cursor, next - cursor);

            const size_t eq = token.find('=');
            if (eq != std::string::npos) {
                const std::string name  = trimWS(token.substr(0, eq));
                const std::string value = trimWS(token.substr(eq + 1));
                if (!name.empty()) props[name] = value;
            }

            if (next == std::string::npos) break;
            cursor = next + 1;
        }
        result["properties"] = std::move(props);
    }

    return result;
}

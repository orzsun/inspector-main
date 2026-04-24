#pragma once

#include "PacketData.h" // For PacketInfo
#include <optional>
#include <string>
#include <map>
#include <utility>

namespace kx::Parsing {

    // Define a function pointer type for parser functions
    using ParserFunc = std::optional<std::string>(*)(const kx::PacketInfo&);

    /**
     * @brief Central dispatcher to get a formatted tooltip string for any known parsed packet.
     *        Uses a map-based registry for extensibility.
     * @param packet The PacketInfo object.
     * @return An optional string suitable for display in a tooltip if a parser is registered,
     *         otherwise std::nullopt.
     */
    std::optional<std::string> GetParsedDataTooltipString(const kx::PacketInfo& packet);

} // namespace kx::Parsing
#pragma once
#include "../PacketData.h"
#include <optional>
#include <string>

namespace kx::Parsing {
    std::optional<std::string> ParseSessionTickPacket(const kx::PacketInfo& packet);
}

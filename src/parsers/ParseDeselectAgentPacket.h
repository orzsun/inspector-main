#pragma once
#include "../PacketData.h"
#include <optional>
#include <string>

namespace kx::Parsing {
    std::optional<std::string> ParseDeselectAgentPacket(const kx::PacketInfo& packet);
}

#pragma once
#include "../PacketData.h"
#include <optional>
#include <string>

namespace kx::Parsing {
    std::optional<std::string> ParseInteractWithAgentPacket(const kx::PacketInfo& packet);
}

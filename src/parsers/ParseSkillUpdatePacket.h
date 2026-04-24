#pragma once

#include "../PacketData.h"

#include <optional>
#include <string>

namespace kx::Parsing {

std::optional<std::string> ParseSkillUpdatePacket(const kx::PacketInfo& packet);

}

#pragma once

#include "PacketData.h"
#include <vector>

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

class ImGuiManager {
public:
    static bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd);
    static void NewFrame();
    static void Render(ID3D11DeviceContext* context, ID3D11RenderTargetView* mainRenderTargetView);
    static void RenderUI();
    static void Shutdown();
private:
    static int m_selectedPacketLogIndex; // Stores the index of the selected packet in the global log
    static std::string m_parsedPayloadBuffer; // Stores the formatted parsed data for display
    static std::string m_fullLogEntryBuffer; // Stores the full log entry string for display

    static void RenderPacketInspectorWindow(); // Main window function
    static void RenderSkillMemorySection();
    // Helper functions for RenderPacketInspectorWindow sections
    static void RenderHints();
    static void RenderInfoSection();
    static void RenderStatusControlsSection();
    static void RenderBuffProbeSection();
    static void RenderSkillCooldownMonitorSection();
    static void RenderFilteringSection();
    static void RenderPacketLogSection();
    static void RenderSelectedPacketDetailsSection(); // New section for detailed parsed data
    static void RenderSinglePacketLogRow(const kx::PacketInfo& packet, int display_index, int original_log_index);

    // Helpers for RenderPacketLogSection
    static std::vector<int> GetFilteredPacketIndicesSnapshot(size_t& out_total_packets);
    static void RenderPacketLogControls(size_t displayed_count, size_t total_count, const std::vector<int>& original_indices_snapshot);
    static void RenderPacketListWithClipping(const std::vector<int>& original_indices_snapshot);
};

#include "led_view.h"

#include <algorithm>

#include <aurora/aurora_runtime.h>
#include <imgui.h>

namespace {

constexpr float LedHeight = 28.0F;
constexpr float MinimumLedWidth = 10.0F;
constexpr float LedSpacing = 2.0F;

static_assert(Aurora::LedCount == 56,
              "The simulator view must display exactly 56 LEDs");

}  // namespace

void drawLedView(const AuroraRuntime &runtime) {
  ImGui::SetNextWindowPos(ImVec2(780.0F, 10.0F),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(410.0F, 140.0F),
                           ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("LED frame")) {
    ImGui::End();
    return;
  }

  if (runtime.ledCount() != Aurora::LedCount) {
    ImGui::TextUnformatted("Unexpected LED frame size");
    ImGui::End();
    return;
  }

  const float availableWidth =
      std::max(ImGui::GetContentRegionAvail().x, 1.0F);
  const float minimumContentWidth =
      Aurora::LedCount * MinimumLedWidth +
      (Aurora::LedCount - 1) * LedSpacing;
  const float contentWidth =
      std::max(availableWidth, minimumContentWidth);
  const float ledWidth =
      (contentWidth - (Aurora::LedCount - 1) * LedSpacing) /
      Aurora::LedCount;
  const float childHeight = LedHeight + ImGui::GetStyle().ScrollbarSize +
                            ImGui::GetStyle().WindowPadding.y * 2.0F;

  const bool childVisible = ImGui::BeginChild(
      "LED frame scroll", ImVec2(0.0F, childHeight), false,
      ImGuiWindowFlags_HorizontalScrollbar);
  if (childVisible) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
      const Aurora::Rgb8 &pixel = runtime.ledFrame()[index];
      const float x = origin.x + index * (ledWidth + LedSpacing);
      drawList->AddRectFilled(
          ImVec2(x, origin.y), ImVec2(x + ledWidth, origin.y + LedHeight),
          IM_COL32(pixel.r, pixel.g, pixel.b, 255));
    }

    ImGui::Dummy(ImVec2(contentWidth, LedHeight));
  }
  ImGui::EndChild();
  ImGui::End();
}

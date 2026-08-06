#include "aurora_field_view.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include <aurora/aurora_runtime.h>
#include <imgui.h>

namespace {

constexpr float AxisWidth = 34.0F;
constexpr float PointSpacing = 28.0F;
constexpr float PointRadius = 5.0F;
constexpr float PlotTopMargin = 10.0F;
constexpr float ValuesHeight = 52.0F;
constexpr float MinimumPlotHeight = 150.0F;
constexpr float RightMargin = 12.0F;

ImU32 rgbColor(const Aurora::Rgb8 &color, uint8_t alpha = UINT8_MAX) {
  return IM_COL32(color.r, color.g, color.b, alpha);
}

float valueY(uint8_t value, float plotTop, float plotHeight) {
  return plotTop +
         (UINT8_MAX - static_cast<float>(value)) * plotHeight / UINT8_MAX;
}

void drawCenteredText(ImDrawList *drawList, float centerX, float y,
                      ImU32 color, uint8_t value) {
  char text[4]{};
  std::snprintf(text, sizeof(text), "%u", static_cast<unsigned>(value));
  const float width = ImGui::CalcTextSize(text).x;
  drawList->AddText(ImVec2(centerX - width * 0.5F, y), color, text);
}

}  // namespace

void drawAuroraFieldView(const AuroraRuntime &runtime) {
  ImGui::SetNextWindowPos(ImVec2(780.0F, 160.0F),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(410.0F, 550.0F),
                           ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Aurora field")) {
    ImGui::End();
    return;
  }

  ImGui::TextColored(ImVec4(0.88F, 0.89F, 0.91F, 1.0F), "B: flare");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.35F, 0.65F, 1.0F, 1.0F),
                     "H: HDD background");
  ImGui::SameLine();
  ImGui::TextUnformatted("P: color progress");

  const bool childVisible = ImGui::BeginChild(
      "Aurora field scroll", ImVec2(0.0F, 0.0F), false,
      ImGuiWindowFlags_HorizontalScrollbar);
  if (childVisible) {
    const float availableHeight =
        std::max(ImGui::GetContentRegionAvail().y, 1.0F);
    const float plotHeight =
        std::max(availableHeight - PlotTopMargin - ValuesHeight -
                     ImGui::GetStyle().ScrollbarSize,
                 MinimumPlotHeight);
    const float contentWidth = AxisWidth +
                               (Aurora::LedCount - 1) * PointSpacing +
                               RightMargin;
    const float contentHeight = PlotTopMargin + plotHeight + ValuesHeight;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float plotLeft = origin.x + AxisWidth;
    const float plotRight =
        plotLeft + (Aurora::LedCount - 1) * PointSpacing;
    const float plotTop = origin.y + PlotTopMargin;
    const float plotBottom = plotTop + plotHeight;
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    constexpr uint8_t GridValues[] = {0, 64, 128, 192, UINT8_MAX};
    for (uint8_t value : GridValues) {
      const float y = valueY(value, plotTop, plotHeight);
      drawList->AddLine(ImVec2(plotLeft, y), ImVec2(plotRight, y),
                        IM_COL32(48, 53, 62, 255));
      char label[4]{};
      std::snprintf(label, sizeof(label), "%u",
                    static_cast<unsigned>(value));
      const ImVec2 labelSize = ImGui::CalcTextSize(label);
      drawList->AddText(
          ImVec2(plotLeft - labelSize.x - 5.0F,
                 y - labelSize.y * 0.5F),
          IM_COL32(139, 145, 156, 255), label);
    }

    bool havePrevious = false;
    ImVec2 previousBrightness{};
    ImVec2 previousBackground{};
    for (uint8_t index = 0; index < Aurora::LedCount; ++index) {
      const Aurora::FieldCellDiagnostics cell =
          runtime.auroraDiagnostics(index);
      const uint8_t brightness =
          static_cast<uint8_t>(cell.brightnessQ8_8 >> 8);
      const uint8_t background =
          static_cast<uint8_t>(cell.backgroundBrightnessQ8_8 >> 8);
      const float x = plotLeft + index * PointSpacing;
      const ImVec2 brightnessPoint(
          x, valueY(brightness, plotTop, plotHeight));
      const ImVec2 backgroundPoint(
          x, valueY(background, plotTop, plotHeight));

      if (havePrevious) {
        drawList->AddLine(previousBackground, backgroundPoint,
                          IM_COL32(70, 140, 230, 210), 1.5F);
        drawList->AddLine(previousBrightness, brightnessPoint,
                          IM_COL32(93, 101, 115, 255));
      }
      havePrevious = true;
      previousBrightness = brightnessPoint;
      previousBackground = backgroundPoint;

      drawList->AddRectFilled(
          ImVec2(backgroundPoint.x - 3.0F, backgroundPoint.y - 3.0F),
          ImVec2(backgroundPoint.x + 3.0F, backgroundPoint.y + 3.0F),
          IM_COL32(70, 140, 230, 255));
      drawList->AddCircleFilled(brightnessPoint, PointRadius,
                                rgbColor(cell.color));
      drawList->AddCircle(brightnessPoint, PointRadius,
                          rgbColor(cell.color, 255), 0, 1.5F);

      drawCenteredText(drawList, x, plotBottom + 4.0F,
                       IM_COL32(224, 227, 232, 255), brightness);
      drawCenteredText(drawList, x, plotBottom + 19.0F,
                       IM_COL32(90, 165, 255, 255), background);
      drawCenteredText(drawList, x, plotBottom + 34.0F,
                       rgbColor(cell.color), cell.colorProgress);

      if (ImGui::IsMouseHoveringRect(
              ImVec2(x - PointSpacing * 0.5F, plotTop),
              ImVec2(x + PointSpacing * 0.5F, plotBottom + ValuesHeight))) {
        ImGui::BeginTooltip();
        ImGui::Text("LED %u", static_cast<unsigned>(index));
        ImGui::Text("B flare: %u (Q8.8 %u)",
                    static_cast<unsigned>(brightness),
                    static_cast<unsigned>(cell.brightnessQ8_8));
        ImGui::Text("H background: %u (Q8.8 %u)",
                    static_cast<unsigned>(background),
                    static_cast<unsigned>(cell.backgroundBrightnessQ8_8));
        ImGui::Text("P color progress: %u",
                    static_cast<unsigned>(cell.colorProgress));
        ImGui::Text("Color: #%02X%02X%02X",
                    static_cast<unsigned>(cell.color.r),
                    static_cast<unsigned>(cell.color.g),
                    static_cast<unsigned>(cell.color.b));
        ImGui::EndTooltip();
      }
    }

    ImGui::Dummy(ImVec2(contentWidth, contentHeight));
  }
  ImGui::EndChild();
  ImGui::End();
}

#include "simulator_app.h"

#include <algorithm>

#include <imgui.h>

#include "aurora_field_view.h"
#include "input_panel.h"

namespace {

constexpr const char *SpeedLabels[] = {
    "0.1x", "0.25x", "0.5x", "1x",
    "2x",   "4x",    "8x",   "16x",
};

constexpr float AuroraPanelHeightRatio = 0.60F;
constexpr float SimulationPanelWidthRatio = 1.0F / 3.0F;
constexpr float MinimumPanelSize = 1.0F;
constexpr ImGuiWindowFlags DashboardPanelFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

const char *pcStateName(PcState state) {
  switch (state) {
    case PcState::Off:
      return "Off";
    case PcState::Starting:
      return "Starting";
    case PcState::Running:
      return "Running";
    case PcState::Sleeping:
      return "Sleeping";
    case PcState::AwaitShutdown:
      return "AwaitShutdown";
    case PcState::Warn:
      return "Warn";
  }
  return "Unknown";
}

const char *transitionName(TransitionEffect transition) {
  switch (transition) {
    case TransitionEffect::None:
      return "None";
    case TransitionEffect::Startup:
      return "Startup";
    case TransitionEffect::Shutdown:
      return "Shutdown";
    case TransitionEffect::ForcedShutdown:
      return "ForcedShutdown";
    case TransitionEffect::Reset:
      return "Reset";
  }
  return "Unknown";
}

}  // namespace

void SimulatorApp::update(uint32_t wallDeltaMs) {
  session_.update(wallDeltaMs);
}

void SimulatorApp::draw() {
  const AuroraSnapshot &snapshot = session_.snapshot();
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  const ImVec2 origin = viewport->WorkPos;
  const ImVec2 availableSize(
      std::max(viewport->WorkSize.x, MinimumPanelSize),
      std::max(viewport->WorkSize.y, MinimumPanelSize));
  const float auroraHeight = availableSize.y * AuroraPanelHeightRatio;
  const float controlsHeight = availableSize.y - auroraHeight;
  const float simulationWidth =
      availableSize.x * SimulationPanelWidthRatio;
  const float inputsWidth = availableSize.x - simulationWidth;

  ImGui::SetNextWindowPos(origin, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(availableSize.x, auroraHeight),
                           ImGuiCond_Always);
  drawAuroraFieldView(session_.runtime(), ledSquareBrightnessScale_,
                      DashboardPanelFlags);

  ImGui::SetNextWindowPos(ImVec2(origin.x, origin.y + auroraHeight),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(simulationWidth, controlsHeight),
                           ImGuiCond_Always);
  if (ImGui::Begin("Simulation", nullptr, DashboardPanelFlags)) {
    if (ImGui::Button(session_.paused() ? "Resume" : "Pause")) {
      session_.setPaused(!session_.paused());
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!session_.paused());
    if (ImGui::Button("Step")) session_.stepFrame();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset")) session_.reset();

    int selectedSpeed = static_cast<int>(session_.speed());
    if (ImGui::BeginCombo("Speed", SpeedLabels[selectedSpeed])) {
      for (int index = 0; index < 8; ++index) {
        const bool selected = index == selectedSpeed;
        if (ImGui::Selectable(SpeedLabels[index], selected)) {
          selectedSpeed = index;
          session_.setSpeed(static_cast<SimulationSpeed>(index));
        }
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    ImGui::SliderFloat("LED square brightness", &ledSquareBrightnessScale_,
                       1.0F, 4.0F, "%.1fx");

    ImGui::Text("Simulation time: %u ms",
                static_cast<unsigned>(session_.simulationTimeMs()));
    ImGui::Text("PC state: %s", pcStateName(snapshot.pcState));
    ImGui::Text("Transition: %s", transitionName(snapshot.transition));
    ImGui::Text("Frame updated: %s",
                snapshot.frameUpdated ? "true" : "false");
    ImGui::Text("LED count: %u",
                static_cast<unsigned>(session_.runtime().ledCount()));
  }
  ImGui::End();

  ImGui::SetNextWindowPos(
      ImVec2(origin.x + simulationWidth, origin.y + auroraHeight),
      ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(inputsWidth, controlsHeight),
                           ImGuiCond_Always);
  drawInputPanel(session_, DashboardPanelFlags);
}

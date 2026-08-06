#include "simulator_app.h"

#include <imgui.h>

#include "led_view.h"

namespace {

constexpr const char *SpeedLabels[] = {
    "0.1x", "0.25x", "0.5x", "1x",
    "2x",   "4x",    "8x",   "16x",
};

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

  ImGui::SetNextWindowPos(ImVec2(10.0F, 10.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(360.0F, 250.0F),
                           ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Simulation")) {
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

  drawLedView(session_.runtime());
}

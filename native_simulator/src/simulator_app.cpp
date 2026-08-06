#include "simulator_app.h"

#include <algorithm>

#include <imgui.h>

#include "led_view.h"

namespace {

constexpr uint32_t InitialLogicStepMs = 10;
constexpr uint32_t MaxAcceptedWallDeltaMs = 40;

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
  const uint32_t acceptedDeltaMs =
      std::min(wallDeltaMs, MaxAcceptedWallDeltaMs);
  accumulatorMs_ += static_cast<double>(acceptedDeltaMs);

  while (accumulatorMs_ >= InitialLogicStepMs) {
    session_.advance(InitialLogicStepMs);
    accumulatorMs_ -= InitialLogicStepMs;
  }
}

void SimulatorApp::draw() {
  const AuroraSnapshot &snapshot = session_.snapshot();

  ImGui::SetNextWindowPos(ImVec2(10.0F, 10.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(360.0F, 190.0F),
                           ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Simulation")) {
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

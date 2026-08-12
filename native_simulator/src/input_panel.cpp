#include "input_panel.h"

#include <imgui.h>

#include "simulator_session.h"

namespace {
constexpr const char *PowerModes[] = {"Manual", "Off", "On", "Blinking"};
constexpr const char *HddModes[] = {"Manual", "Random / Quake"};

void setControlWidth() {
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.48F);
}

void drawPowerLedControls(SimulatorSession &session) {
  VirtualInputs &inputs = session.inputs();
  PowerLedGenerator &power = session.powerLedGenerator();

  ImGui::SeparatorText("Power LED");
  int powerMode = static_cast<int>(power.mode());
  setControlWidth();
  if (ImGui::Combo("Source##power", &powerMode, PowerModes, 4)) {
    power.setMode(static_cast<PowerLedSourceMode>(powerMode));
  }
  bool manualPower = inputs.manualPowerLed();
  ImGui::BeginDisabled(power.mode() != PowerLedSourceMode::Manual);
  if (ImGui::Checkbox("Manual Power LED", &manualPower)) {
    inputs.setManualPowerLed(manualPower);
  }
  ImGui::EndDisabled();
  int halfPeriod = static_cast<int>(power.halfPeriodMs());
  ImGui::BeginDisabled(power.mode() != PowerLedSourceMode::Blinking);
  setControlWidth();
  if (ImGui::SliderInt("Blink half-period (ms)", &halfPeriod, 100, 5000)) {
    power.setHalfPeriodMs(static_cast<uint32_t>(halfPeriod));
  }
  ImGui::EndDisabled();
  ImGui::Text("Generated Power LED: %s", power.level() ? "On" : "Off");
}

void drawButtonControls(VirtualInputs &inputs) {
  ImGui::SeparatorText("Buttons and strip power");
  ImGui::BeginDisabled(inputs.powerButtonHeld());
  if (ImGui::Button("Power Press")) inputs.pressPowerButton();
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!inputs.powerButtonHeld());
  if (ImGui::Button("Power Release")) inputs.releasePowerButton();
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextUnformatted(inputs.powerButtonHeld() ? "Pressed" : "Released");

  ImGui::BeginDisabled(inputs.resetButtonHeld());
  if (ImGui::Button("Reset Press")) inputs.pressResetButton();
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!inputs.resetButtonHeld());
  if (ImGui::Button("Reset Release")) inputs.releaseResetButton();
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextUnformatted(inputs.resetButtonHeld() ? "Pressed" : "Released");

  bool stripPower = inputs.stripPowerPresent();
  if (ImGui::Checkbox("Strip power present", &stripPower)) {
    inputs.setStripPowerPresent(stripPower);
  }
}

void drawHddControls(SimulatorSession &session) {
  VirtualInputs &inputs = session.inputs();
  HddGenerator &hdd = session.hddGenerator();

  ImGui::SeparatorText("HDD");
  int hddMode = static_cast<int>(hdd.mode());
  setControlWidth();
  if (ImGui::Combo("Source##hdd", &hddMode, HddModes, 2)) {
    hdd.setMode(static_cast<HddSourceMode>(hddMode));
  }
  bool manualHdd = inputs.manualHddLed();
  ImGui::BeginDisabled(hdd.mode() != HddSourceMode::Manual);
  if (ImGui::Checkbox("Manual HDD LED", &manualHdd)) {
    inputs.setManualHddLed(manualHdd);
  }
  if (ImGui::Button("Add HDD edge")) inputs.addHddActiveEdges();
  ImGui::SameLine();
  if (ImGui::Button("Add 8 HDD edges")) inputs.addHddActiveEdges(8);
  ImGui::EndDisabled();

  HddGeneratorParams params = hdd.params();
  int seed = static_cast<int>(hdd.seed());
  ImGui::BeginDisabled(hdd.mode() != HddSourceMode::RandomQuake);
  setControlWidth();
  if (ImGui::InputInt("Seed", &seed)) {
    hdd.setSeed(static_cast<uint32_t>(seed));
  }
  setControlWidth();
  bool changed =
      ImGui::SliderInt("Activity rate", &params.activityRate, 0, 100);
  setControlWidth();
  changed |= ImGui::SliderInt("Pulse duration (ms)",
                              &params.pulseDurationMs, 5, 5000);
  setControlWidth();
  changed |= ImGui::SliderInt("Burst size", &params.burstSize, 1, 100);
  setControlWidth();
  changed |= ImGui::SliderInt("Randomness", &params.randomness, 0, 100);
  if (changed) hdd.setParams(params);
  ImGui::EndDisabled();
  ImGui::Text("Generated HDD LED: %s", hdd.level() ? "On" : "Off");
  ImGui::Text("Edges last/pending: %u / %u",
              session.lastGeneratedHddEdges(),
              inputs.pendingHddActiveEdges());
}
}

void drawInputPanel(SimulatorSession &session,
                    ImGuiWindowFlags windowFlags) {
  VirtualInputs &inputs = session.inputs();
  if (!ImGui::Begin("Virtual inputs", nullptr, windowFlags)) {
    ImGui::End();
    return;
  }

  constexpr ImGuiTableFlags TableFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
      ImGuiTableFlags_PadOuterX;
  if (ImGui::BeginTable("Virtual input columns", 2, TableFlags)) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    drawPowerLedControls(session);
    drawButtonControls(inputs);
    ImGui::TableSetColumnIndex(1);
    drawHddControls(session);
    ImGui::EndTable();
  }
  ImGui::End();
}

#include "input_panel.h"

#include <imgui.h>

#include "simulator_session.h"

namespace {
constexpr const char *PowerModes[] = {"Manual", "Off", "On", "Blinking"};
constexpr const char *HddModes[] = {"Manual", "Random / Quake"};
}

void drawInputPanel(SimulatorSession &session) {
  VirtualInputs &inputs = session.inputs();
  PowerLedGenerator &power = session.powerLedGenerator();
  HddGenerator &hdd = session.hddGenerator();
  ImGui::SetNextWindowPos(ImVec2(380.0F, 10.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(390.0F, 570.0F), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Virtual inputs")) { ImGui::End(); return; }

  ImGui::SeparatorText("Power LED");
  int powerMode = static_cast<int>(power.mode());
  if (ImGui::Combo("Source##power", &powerMode, PowerModes, 4))
    power.setMode(static_cast<PowerLedSourceMode>(powerMode));
  bool manualPower = inputs.manualPowerLed();
  ImGui::BeginDisabled(power.mode() != PowerLedSourceMode::Manual);
  if (ImGui::Checkbox("Manual Power LED", &manualPower))
    inputs.setManualPowerLed(manualPower);
  ImGui::EndDisabled();
  int halfPeriod = static_cast<int>(power.halfPeriodMs());
  ImGui::BeginDisabled(power.mode() != PowerLedSourceMode::Blinking);
  if (ImGui::SliderInt("Blink half-period (ms)", &halfPeriod, 100, 5000))
    power.setHalfPeriodMs(static_cast<uint32_t>(halfPeriod));
  ImGui::EndDisabled();
  ImGui::Text("Current generated Power LED: %s", power.level() ? "On" : "Off");

  ImGui::SeparatorText("HDD");
  int hddMode = static_cast<int>(hdd.mode());
  if (ImGui::Combo("Source##hdd", &hddMode, HddModes, 2))
    hdd.setMode(static_cast<HddSourceMode>(hddMode));
  bool manualHdd = inputs.manualHddLed();
  ImGui::BeginDisabled(hdd.mode() != HddSourceMode::Manual);
  if (ImGui::Checkbox("Manual HDD LED", &manualHdd)) inputs.setManualHddLed(manualHdd);
  if (ImGui::Button("Add HDD edge")) inputs.addHddActiveEdges();
  ImGui::SameLine();
  if (ImGui::Button("Add 8 HDD edges")) inputs.addHddActiveEdges(8);
  ImGui::EndDisabled();
  HddGeneratorParams params = hdd.params();
  int seed = static_cast<int>(hdd.seed());
  ImGui::BeginDisabled(hdd.mode() != HddSourceMode::RandomQuake);
  if (ImGui::InputInt("Seed", &seed)) hdd.setSeed(static_cast<uint32_t>(seed));
  bool changed = ImGui::SliderInt("Activity rate", &params.activityRate, 0, 100);
  changed |= ImGui::SliderInt("Pulse duration (ms)", &params.pulseDurationMs, 5, 5000);
  changed |= ImGui::SliderInt("Burst size", &params.burstSize, 1, 100);
  changed |= ImGui::SliderInt("Randomness", &params.randomness, 0, 100);
  if (changed) hdd.setParams(params);
  ImGui::EndDisabled();
  ImGui::Text("Current generated HDD LED: %s", hdd.level() ? "On" : "Off");
  ImGui::Text("Generated active edges on last interval: %u", session.lastGeneratedHddEdges());
  ImGui::Text("Pending active edges: %u", inputs.pendingHddActiveEdges());

  ImGui::SeparatorText("Buttons");
  ImGui::BeginDisabled(inputs.powerButtonHeld());
  if (ImGui::Button("Power Press")) inputs.pressPowerButton();
  ImGui::EndDisabled(); ImGui::SameLine();
  ImGui::BeginDisabled(!inputs.powerButtonHeld());
  if (ImGui::Button("Power Release")) inputs.releasePowerButton();
  ImGui::EndDisabled();
  ImGui::Text("Power button: %s", inputs.powerButtonHeld() ? "Pressed" : "Released");
  ImGui::BeginDisabled(inputs.resetButtonHeld());
  if (ImGui::Button("Reset Press")) inputs.pressResetButton();
  ImGui::EndDisabled(); ImGui::SameLine();
  ImGui::BeginDisabled(!inputs.resetButtonHeld());
  if (ImGui::Button("Reset Release")) inputs.releaseResetButton();
  ImGui::EndDisabled();
  ImGui::Text("Reset button: %s", inputs.resetButtonHeld() ? "Pressed" : "Released");

  bool stripPower = inputs.stripPowerPresent();
  if (ImGui::Checkbox("Strip power present", &stripPower))
    inputs.setStripPowerPresent(stripPower);
  ImGui::End();
}

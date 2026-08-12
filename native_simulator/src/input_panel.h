#pragma once

#include <imgui.h>

class SimulatorSession;

void drawInputPanel(SimulatorSession &session,
                    ImGuiWindowFlags windowFlags = 0);

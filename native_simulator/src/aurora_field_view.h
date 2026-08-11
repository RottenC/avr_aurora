#pragma once

#include <imgui.h>

class AuroraRuntime;

void drawAuroraFieldView(const AuroraRuntime &runtime,
                         float ledSquareBrightnessScale,
                         ImGuiWindowFlags windowFlags = 0);

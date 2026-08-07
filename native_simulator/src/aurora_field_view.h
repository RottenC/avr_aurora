#pragma once

#include <imgui.h>

class AuroraRuntime;

void drawAuroraFieldView(const AuroraRuntime &runtime,
                         ImGuiWindowFlags windowFlags = 0);

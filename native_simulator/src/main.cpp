#include <algorithm>
#include <cstdint>
#include <cstdio>

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "simulator_app.h"

namespace {

constexpr int InitialWindowWidth = 1200;
constexpr int InitialWindowHeight = 420;
constexpr double MaxAcceptedWallDeltaMs = 40.0;

SDL_Renderer *createRenderer(SDL_Window *window) {
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1,
                         SDL_RENDERER_ACCELERATED |
                             SDL_RENDERER_PRESENTVSYNC);
  if (renderer != nullptr) return renderer;

  std::fprintf(stderr,
               "Accelerated SDL renderer unavailable: %s; falling back "
               "to software rendering\n",
               SDL_GetError());
  return SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
}

void shutdown(SDL_Window *window, SDL_Renderer *renderer,
              bool platformBackendInitialized,
              bool rendererBackendInitialized) {
  if (rendererBackendInitialized) ImGui_ImplSDLRenderer2_Shutdown();
  if (platformBackendInitialized) ImGui_ImplSDL2_Shutdown();
  if (ImGui::GetCurrentContext() != nullptr) ImGui::DestroyContext();
  if (renderer != nullptr) SDL_DestroyRenderer(renderer);
  if (window != nullptr) SDL_DestroyWindow(window);
  SDL_Quit();
}

}  // namespace

int main(int, char **) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      "AVR Aurora Simulator", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, InitialWindowWidth, InitialWindowHeight,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
    shutdown(nullptr, nullptr, false, false);
    return 1;
  }

  SDL_Renderer *renderer = createRenderer(window);
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL renderer creation failed: %s\n",
                 SDL_GetError());
    shutdown(window, nullptr, false, false);
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  bool platformBackendInitialized =
      ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  if (!platformBackendInitialized) {
    std::fprintf(stderr, "Dear ImGui SDL2 backend initialization failed\n");
    shutdown(window, renderer, false, false);
    return 1;
  }

  bool rendererBackendInitialized = ImGui_ImplSDLRenderer2_Init(renderer);
  if (!rendererBackendInitialized) {
    std::fprintf(stderr,
                 "Dear ImGui SDL renderer backend initialization failed\n");
    shutdown(window, renderer, true, false);
    return 1;
  }

  const uint64_t performanceFrequency = SDL_GetPerformanceFrequency();
  if (performanceFrequency == 0) {
    std::fprintf(stderr, "SDL performance counter is unavailable: %s\n",
                 SDL_GetError());
    shutdown(window, renderer, true, true);
    return 1;
  }

  SimulatorApp app;
  uint64_t previousCounter = SDL_GetPerformanceCounter();
  double fractionalWallMs = 0.0;
  bool running = true;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
          event.window.windowID == SDL_GetWindowID(window)) {
        running = false;
      }
    }

    const uint64_t currentCounter = SDL_GetPerformanceCounter();
    const uint64_t elapsedCounter = currentCounter - previousCounter;
    previousCounter = currentCounter;
    const double measuredWallDeltaMs =
        static_cast<double>(elapsedCounter) * 1000.0 /
        static_cast<double>(performanceFrequency);
    fractionalWallMs +=
        std::min(measuredWallDeltaMs, MaxAcceptedWallDeltaMs);
    const uint32_t wholeWallDeltaMs =
        static_cast<uint32_t>(fractionalWallMs);
    fractionalWallMs -= wholeWallDeltaMs;
    app.update(wholeWallDeltaMs);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    app.draw();
    ImGui::Render();

    const ImGuiIO &io = ImGui::GetIO();
    SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x,
                       io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(renderer, 18, 18, 18, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  shutdown(window, renderer, platformBackendInitialized,
           rendererBackendInitialized);
  return 0;
}

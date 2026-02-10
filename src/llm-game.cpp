#include "aipfg/chat-client.hpp"
#include "aipfg/imgui-context.hpp"
#include "aipfg/sdl3-context.hpp"
#include "aipfg/sdl3-typedefs.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h> // SetConsoleOutputCP & SetConsoleCP for unicode on cmd.exe
#endif

class Game
{
public:
  explicit Game(SDLContext&, int w, int h)
    : window_{SDL_CreateWindow("LLM Game", w, h, SDL_WINDOW_RESIZABLE),
              SDL_DestroyWindow},
      renderer_{SDL_CreateRenderer(window_.get(), nullptr), SDL_DestroyRenderer},
      imgui_ctx_{nullptr},
      chat_client_{"https://polaris.uws.ac.uk/api/chat/completions", "POLARIS_API_KEY",
                   "gpt-oss:20b", "You are a helpful assistant."}
  {
    if (!window_ || !renderer_)
    {
      throw std::runtime_error(SDL_GetError());
    }

    SDL_SetRenderVSync(renderer_.get(), 1);

    float font_size = h / 25.0f;
    imgui_ctx_ =
      std::make_unique<ScopedImGui>(window_.get(), renderer_.get(), font_size);

    input_buffer_[0] = '\0';
  }

  void run()
  {
    bool running = true;
    while (running)
    {
      process_events(running);
      update();
      render();
    }
  }

private:
  void process_events(bool& running)
  {
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
      ImGui_ImplSDL3_ProcessEvent(&e);

      if (e.type == SDL_EVENT_QUIT)
      {
        running = false;
      }
      else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
      {
        running = false;
      }
    }
  }

  void update()
  {
    // Update cycling background colour
    const float t = SDL_GetTicks() / 1000.0f;
    const float pi = std::numbers::pi_v<float>;
    bg_r_ = 0.3f + 0.3f * std::sin(t * 3.0f);
    bg_g_ = 0.3f + 0.3f * std::sin(t * 3.0f + pi * (2.0f / 3.0f));
    bg_b_ = 0.3f + 0.3f * std::sin(t * 3.0f + pi * (4.0f / 3.0f));
  }

  void render()
  {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Chat window
    ImGui::Begin("Chat");

    // Display chat history
    ImGui::BeginChild("ChatHistory", ImVec2(0, -60), true);
    for (const auto& msg : chat_history_)
    {
      ImGui::TextWrapped("%s", msg.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    // Input area
    ImGui::Separator();
    bool send = ImGui::InputText("##input", input_buffer_, sizeof(input_buffer_),
                                 ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    send |= ImGui::Button("Send");

    if (send && input_buffer_[0] != '\0')
    {
      std::string user_msg = input_buffer_;
      chat_history_.push_back("You: " + user_msg);

      // This call is SYNCHRONOUS - the UI will freeze until it returns!
      std::string response = chat_client_.send_message(user_msg);
      chat_history_.push_back("Assistant: " + response);

      input_buffer_[0] = '\0';
    }

    ImGui::End();

    ImGui::Render();

    SDL_SetRenderDrawColorFloat(renderer_.get(), bg_r_, bg_g_, bg_b_, 1.0f);
    SDL_RenderClear(renderer_.get());

    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_.get());

    SDL_RenderPresent(renderer_.get());
  }

private:
  WindowPtr                    window_;
  RendererPtr                  renderer_;
  std::unique_ptr<ScopedImGui> imgui_ctx_;

  ChatClient               chat_client_;
  std::vector<std::string> chat_history_;
  char                     input_buffer_[256];

  // Cycling background colour
  float bg_r_ = 0.1f;
  float bg_g_ = 0.1f;
  float bg_b_ = 0.2f;
};

int main()
{
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8); // Unicode for cmd.exe
  SetConsoleCP(CP_UTF8);       // for unicode/emojis
#endif

  try
  {
    SDLContext sdl;
    Game       game{sdl, 800, 600};
    game.run();
  }
  catch (const std::exception& e)
  {
    SDL_Log("Fatal error: %s", e.what());
    return -1;
  }

  return 0;
}

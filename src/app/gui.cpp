#include <IconsFontAwesome6.h>
#include <app/gui.hpp>
#include <app/scene.hpp>
#include <filesystem>
#include <imgui-SFML.h>
#include <imgui.h>
#include <implot.h>
#include <utils/entt_lua.hpp>
namespace fs = std::filesystem;

Gui::Gui() {}

Gui::~Gui() { ImGui::SFML::Shutdown(); 
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
}

static ImGuiDockNodeFlags opt_flags = ImGuiDockNodeFlags_PassthruCentralNode;

void Gui::init(LibLog::Logger parentLog) {
  log.setParent(&parentLog);
  auto label = "Initializing GUI";
  log.start(label);
  auto &emitter = entt::locator<event_emitter>::value();

  auto &lua = entt::locator<sol::state>::value();
  // lua.new_usertype<Gui>("gui",
  //   "new", sol::no_constructor
  // );
  auto PATH = lua["app"]["PATH"].get<std::string>();

  ImGui::CreateContext();
  ImPlot::CreateContext();
  // Theme::Init();

  auto settings = lua["gui"];

  float GUI_SCALE = settings["scale"].get_or(1.0f);
  float FONT_SIZE = lua["gui"]["font_size"].get_or(16.0f);
  float ICON_SIZE = lua["gui"]["icon_size"].get_or(16.0f);
  entt::monostate<"gui_scale"_hs>{} = GUI_SCALE;
  ImGui::GetIO().FontGlobalScale = GUI_SCALE;
  ImGui::GetStyle().ScaleAllSizes(GUI_SCALE);

  auto &scene = entt::locator<Scene>::value();
  log.debug("Sfml init: {}", ImGui::SFML::Init(*scene.window, false));

  ImGui::GetIO().ConfigDockingWithShift = false;
  ImGui::GetIO().MouseDrawCursor = true;
  ImGui::GetIO().ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  icons_config.GlyphMinAdvanceX = 16.0f;

  float baseFontSize =
      FONT_SIZE * GUI_SCALE; // 13.0f is the size of the default font. Change to
                             // the font size you use.
  float iconFontSize =
      ICON_SIZE * GUI_SCALE; // FontAwesome fonts need to have their sizes
                             // reduced by 2.0f/3.0f in order to align correctly

  ImGui::GetIO().Fonts->Clear();
  // ImGui::GetIO().Fonts->AddFontDefault();
  auto main_font = settings["font"].get<std::string>();
  // auto hack_font = PATH + "/fonts/Roboto-Medium.ttf";
  log.var("Font", fs::relative(main_font, PATH).string());
  ImGui::GetIO().Fonts->AddFontFromFileTTF(main_font.c_str(), baseFontSize);
  auto fa_font = (PATH + "/fonts/" + FONT_ICON_FILE_NAME_FAS);
  log.var("Font", fs::relative(fa_font, PATH).string());
  ImGui::GetIO().Fonts->AddFontFromFileTTF(fa_font.c_str(), iconFontSize,
                                           &icons_config, icons_ranges);
  // log.info("fonts: {}", ImGui::GetIO().Fonts->Fonts.Size);
  ImGui::GetIO().Fonts->Build();
  log.var("Fonts updated", ImGui::SFML::UpdateFontTexture());

  emitter.connect<sf_event_event>([&](const auto &e, const auto &em) {
    auto &scene = entt::locator<Scene>::value();
    ImGui::SFML::ProcessEvent(*scene.window, e.event);
    ImGuiIO &io = ImGui::GetIO();
    scene.acceptInput = !io.WantCaptureKeyboard;
  });

  emitter.publish(init_event{"gui"});

  log.stop(label);
}

void Gui::serve() {
  auto STATUS_BAR_HEIGHT = 24.0f;
  auto &scene = entt::locator<Scene>::value();
  ImGui::SFML::Update(*scene.window, deltaClock.restart());
  drawDocking(STATUS_BAR_HEIGHT);
  for (auto render : renders) {
    render();
  }
  for (auto editor : editors) {
    editor->render();
  }
  ImGuiViewport *vp = ImGui::GetMainViewport();
  drawStatusBar(vp->Size.x, STATUS_BAR_HEIGHT, 0.0f,
                vp->Size.y - STATUS_BAR_HEIGHT);
  ImGui::SFML::Render(*scene.window);
}

void Gui::drawDocking(float padding) {
  ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGuiWindowFlags window_flags =
      /*ImGuiWindowFlags_MenuBar | */ ImGuiWindowFlags_NoDocking;
  auto dockSpaceSize = vp->Size;
  dockSpaceSize.y -= padding; // remove the status bar
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(dockSpaceSize);
  ImGui::SetNextWindowViewport(vp->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  if (opt_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpace", nullptr, window_flags);
  ImGui::PopStyleVar();

  ImGui::PopStyleVar(2);

  // Dockspace
  ImGuiIO &io = ImGui::GetIO();
  // io.ConfigResizeWindowsFromEdges = true;
  ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), opt_flags);
  ImGui::End();
}

void Gui::drawStatusBar(float width, float height, float pos_x, float pos_y) {
  float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
  // auto &viewport = entt::locator<Viewport>::value();
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  ImGui::SetNextWindowPos(ImVec2(pos_x, pos_y), ImGuiCond_Always);
  ImGui::Begin("statusbar", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoResize);

  ImGui::SameLine(8.0f * GUI_SCALE);
  // Font font(Font::FAMILY_MONOSPACE);
  // font.Normal().Regular().SmallSize();
  // ImGui::PushFont(font.ImGuiFont());
  auto fps = std::lround(ImGui::GetIO().Framerate);
  if (fps > 55) {
    ImGui::Text("FPS: %ld", fps);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text,
                          (ImVec4)ImColor::HSV(1.f / 7.f, 0.86f, 1.0f));
    ImGui::Text("FPS: %ld", fps);
    ImGui::PopStyleColor(1);
  }

  for (auto render : statusRenders) {
    render();
  }
  ImGui::End();
}

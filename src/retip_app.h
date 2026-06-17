// retip - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <memory>

#include <rex/rex_app.h>
#include <rex/ui/window_win.h>

#include "plume_renderer.h"
#include "video_hooks.h"

class RetipApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<RetipApp>(new RetipApp(ctx, "retip",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  void OnPreSetup(rex::RuntimeConfig &config) override {
    // Disable the xenos GPU, We will be using plume instead.
    config.graphics = nullptr;
  }
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  void OnPostSetup() override {
    auto* win32_window = static_cast<rex::ui::Win32Window*>(window());
    if (win32_window && win32_window->hwnd()) {
      plume_renderer_ = std::make_unique<PlumeRenderer>(win32_window->hwnd());
      plume_renderer_->Start();
      g_video_renderer = plume_renderer_.get();
    }
  }
  void OnShutdown() override {
    g_video_renderer = nullptr;
    if (plume_renderer_) {
      plume_renderer_->Stop();
      plume_renderer_.reset();
    }
  }
  void OnResize(rex::ui::UISetupEvent&) override {
    if (plume_renderer_) {
      plume_renderer_->NotifyResize();
    }
  }
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  void OnConfigurePaths(rex::PathConfig &paths) override {
    if (paths.game_data_root.empty()) {
      wchar_t exe_path[MAX_PATH] = {};
      GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
      auto exe_dir = std::filesystem::path(exe_path).parent_path();
      auto assets_next_to_exe = exe_dir / "../../../assets";
      auto assets_in_cwd = std::filesystem::current_path() / "../../../assets";
      if (std::filesystem::exists(assets_next_to_exe)) {
        paths.game_data_root = assets_next_to_exe;
      } else if (std::filesystem::exists(assets_in_cwd)) {
        paths.game_data_root = assets_in_cwd;
      }
    }
  }

 private:
  std::unique_ptr<PlumeRenderer> plume_renderer_;
};

REX_STUB(__imp__XUsbcamSetView)
REX_STUB(__imp__XUsbcamSetCaptureMode)
REX_STUB(__imp__XUsbcamSetConfig)
REX_STUB(__imp__XUsbcamReadFrame)
REX_STUB(__imp__XUsbcamDestroy)
REX_STUB(__imp__XUsbcamCreate)
REX_STUB(__imp__XUsbcamGetState)
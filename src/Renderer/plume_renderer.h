#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "plume_render_interface.h"

struct YUVFrame {
  std::vector<uint8_t> y, u, v;
  uint32_t y_width{}, y_height{};
  uint32_t u_width{}, u_height{};
  uint32_t v_width{}, v_height{};
};

class PlumeRenderer {
 public:
  explicit PlumeRenderer(HWND hwnd);
  ~PlumeRenderer();

  void Start();
  void Stop();
  void NotifyResize();

  void SubmitVideoQuad(float verts[6][4]);

  void SubmitYUVFrame(YUVFrame frame);

 private:
  void RenderThread();
  void CreateFramebuffers();
  void HandleResize();
  void Render();
  void EnsureYUVTextures(uint32_t yw, uint32_t yh, uint32_t uw, uint32_t uh, uint32_t vw, uint32_t vh);

  HWND hwnd_;
  std::atomic<bool> running_{false};
  std::atomic<bool> resize_pending_{false};
  std::thread thread_;

  static constexpr uint32_t kBufferCount = 2;
  static constexpr plume::RenderFormat kSwapchainFormat = plume::RenderFormat::B8G8R8A8_UNORM;

  std::unique_ptr<plume::RenderDevice> device_;
  std::unique_ptr<plume::RenderCommandQueue> command_queue_;
  std::unique_ptr<plume::RenderCommandList> command_list_;
  std::unique_ptr<plume::RenderCommandFence> fence_;
  std::unique_ptr<plume::RenderSwapChain> swap_chain_;
  std::unique_ptr<plume::RenderCommandSemaphore> acquire_semaphore_;
  std::vector<std::unique_ptr<plume::RenderCommandSemaphore>> release_semaphores_;
  std::vector<std::unique_ptr<plume::RenderFramebuffer>> framebuffers_;

  std::unique_ptr<plume::RenderPipeline> pipeline_;
  std::unique_ptr<plume::RenderPipelineLayout> pipeline_layout_;
  std::unique_ptr<plume::RenderBuffer> vertex_buffer_;
  plume::RenderVertexBufferView vertex_buffer_view_{};
  plume::RenderInputSlot input_slot_{};

  struct VideoQuadVert { float x, y, z, u, v; };
  VideoQuadVert pending_quad_[6]{};
  std::atomic<bool> quad_dirty_{false};

  std::unique_ptr<plume::RenderTexture> yuv_tex_[3];
  std::unique_ptr<plume::RenderBuffer> yuv_upload_[3];
  uint32_t yuv_upload_pitch_[3]{};
  std::unique_ptr<plume::RenderSampler> yuv_sampler_;
  std::unique_ptr<plume::RenderDescriptorSet> yuv_desc_set_;
  uint32_t yuv_tex_dims_[3][2]{};

  std::mutex yuv_mutex_;
  YUVFrame pending_yuv_;
  bool yuv_frame_ready_{false};
  bool yuv_copy_pending_{false};
};

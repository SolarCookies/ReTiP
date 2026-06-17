#include "plume_renderer.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#ifdef _WIN64
#include "shaders/videoVert.hlsl.dxil.h"
#include "shaders/videoFrag.hlsl.dxil.h"
#endif
#include "shaders/videoVert.hlsl.spirv.h"
#include "shaders/videoFrag.hlsl.spirv.h"

// Forward declare
namespace plume {
extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
}

using namespace plume;

PlumeRenderer::PlumeRenderer(HWND hwnd) : hwnd_(hwnd) {}

PlumeRenderer::~PlumeRenderer() {
  Stop();
}

void PlumeRenderer::Start() {
  running_.store(true, std::memory_order_release);
  thread_ = std::thread([this] { RenderThread(); });
}

void PlumeRenderer::Stop() {
  running_.store(false, std::memory_order_release);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void PlumeRenderer::NotifyResize() {
  resize_pending_.store(true, std::memory_order_release);
}

void PlumeRenderer::SubmitVideoQuad(float verts[6][4]) {
  constexpr float kW = 1280.0f;
  constexpr float kH = 720.0f;

  float umin = verts[0][2], umax = verts[0][2];
  float vmin = verts[0][3], vmax = verts[0][3];
  for (int i = 1; i < 6; i++) {
    umin = std::min(umin, verts[i][2]);
    umax = std::max(umax, verts[i][2]);
    vmin = std::min(vmin, verts[i][3]);
    vmax = std::max(vmax, verts[i][3]);
  }
  const bool uv_degenerate = ((umax - umin) < 1.0e-4f) || ((vmax - vmin) < 1.0e-4f);

  VideoQuadVert q[6];
  for (int i = 0; i < 6; i++) {
    q[i].x = (verts[i][0] / kW) * 2.0f - 1.0f;
    q[i].y = 1.0f - (verts[i][1] / kH) * 2.0f;
    q[i].z = 0.0f;
    if (uv_degenerate) {
      q[i].u = std::clamp(verts[i][0] / kW, 0.0f, 1.0f);
      q[i].v = std::clamp(verts[i][1] / kH, 0.0f, 1.0f);
    } else {
      q[i].u = verts[i][2];
      q[i].v = verts[i][3];
    }
  }
  std::memcpy(pending_quad_, q, sizeof(q));
  quad_dirty_.store(true, std::memory_order_release);
}

void PlumeRenderer::SubmitYUVFrame(YUVFrame frame) {
  std::lock_guard<std::mutex> lock(yuv_mutex_);
  pending_yuv_ = std::move(frame);
  yuv_frame_ready_ = true;
}

void PlumeRenderer::EnsureYUVTextures(uint32_t yw, uint32_t yh,
                                       uint32_t uw, uint32_t uh,
                                       uint32_t vw, uint32_t vh) {
  if (yuv_tex_[0] &&
      yuv_tex_dims_[0][0] == yw && yuv_tex_dims_[0][1] == yh &&
      yuv_tex_dims_[1][0] == uw && yuv_tex_dims_[1][1] == uh &&
      yuv_tex_dims_[2][0] == vw && yuv_tex_dims_[2][1] == vh)
    return;

  const uint32_t w[3] = {yw, uw, vw};
  const uint32_t h[3] = {yh, uh, vh};
  for (int p = 0; p < 3; p++) {
    yuv_tex_[p] = device_->createTexture(
        RenderTextureDesc::Texture2D(w[p], h[p], 1, RenderFormat::R8_UNORM));
    yuv_upload_pitch_[p] = (w[p] + 255u) & ~255u;
    yuv_upload_[p] = device_->createBuffer(
        RenderBufferDesc::UploadBuffer(
            static_cast<uint64_t>(yuv_upload_pitch_[p]) * h[p]));
    yuv_tex_dims_[p][0] = w[p];
    yuv_tex_dims_[p][1] = h[p];
  }

  if (!yuv_desc_set_) {
    RenderDescriptorRange ranges[2] = {
        RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE, 0, 3),
        RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER, 3, 1),
    };
    yuv_desc_set_ = device_->createDescriptorSet(
        RenderDescriptorSetDesc(ranges, 2));
    yuv_desc_set_->setSampler(3, yuv_sampler_.get());
  }
  yuv_desc_set_->setTexture(0, yuv_tex_[0].get(), RenderTextureLayout::SHADER_READ);
  yuv_desc_set_->setTexture(1, yuv_tex_[1].get(), RenderTextureLayout::SHADER_READ);
  yuv_desc_set_->setTexture(2, yuv_tex_[2].get(), RenderTextureLayout::SHADER_READ);
}

void PlumeRenderer::CreateFramebuffers() {
  framebuffers_.clear();
  for (uint32_t i = 0; i < swap_chain_->getTextureCount(); i++) {
    const RenderTexture* color = swap_chain_->getTexture(i);
    RenderFramebufferDesc desc;
    desc.colorAttachments = &color;
    desc.colorAttachmentsCount = 1;
    desc.depthAttachment = nullptr;
    framebuffers_.push_back(device_->createFramebuffer(desc));
  }
}

void PlumeRenderer::HandleResize() {
  resize_pending_.store(false, std::memory_order_relaxed);
  framebuffers_.clear();
  release_semaphores_.clear();
  if (swap_chain_) {
    swap_chain_->resize();
    CreateFramebuffers();
  }
}

void PlumeRenderer::Render() {
  {
    std::lock_guard<std::mutex> lock(yuv_mutex_);
    if (yuv_frame_ready_) {
      yuv_frame_ready_ = false;
      YUVFrame& f = pending_yuv_;

      EnsureYUVTextures(f.y_width, f.y_height,
                        f.u_width, f.u_height,
                        f.v_width, f.v_height);

      const std::vector<uint8_t>* planes[3] = { &f.y, &f.u, &f.v };
      for (int p = 0; p < 3; p++) {
        uint32_t pw = yuv_tex_dims_[p][0];
        uint32_t ph = yuv_tex_dims_[p][1];
        uint32_t pitch = yuv_upload_pitch_[p];
        auto* dst = static_cast<uint8_t*>(yuv_upload_[p]->map());
        const uint8_t* src = planes[p]->data();
        for (uint32_t row = 0; row < ph; row++) {
          std::memcpy(dst + row * pitch, src + row * pw, pw);
        }
        yuv_upload_[p]->unmap();
      }
      yuv_copy_pending_ = true;
    }
  }

  if (quad_dirty_.exchange(false, std::memory_order_acquire)) {
    void* mapped = vertex_buffer_->map();
    std::memcpy(mapped, pending_quad_, sizeof(pending_quad_));
    vertex_buffer_->unmap();
  }

  uint32_t image_index = 0;
  swap_chain_->acquireTexture(acquire_semaphore_.get(), &image_index);

  command_list_->begin();

  if (yuv_copy_pending_ && yuv_tex_[0]) {
    yuv_copy_pending_ = false;
    RenderTextureBarrier to_copy[3] = {
        RenderTextureBarrier(yuv_tex_[0].get(), RenderTextureLayout::COPY_DEST),
        RenderTextureBarrier(yuv_tex_[1].get(), RenderTextureLayout::COPY_DEST),
        RenderTextureBarrier(yuv_tex_[2].get(), RenderTextureLayout::COPY_DEST),
    };
    command_list_->barriers(RenderBarrierStage::COPY, to_copy, 3);

    for (int p = 0; p < 3; p++) {
      auto src = RenderTextureCopyLocation::PlacedFootprint(
          yuv_upload_[p].get(),
          RenderFormat::R8_UNORM,
          yuv_tex_dims_[p][0], yuv_tex_dims_[p][1], 1,
          yuv_upload_pitch_[p]);
      auto dst = RenderTextureCopyLocation::Subresource(yuv_tex_[p].get());
      command_list_->copyTextureRegion(dst, src);
    }

    RenderTextureBarrier to_read[3] = {
        RenderTextureBarrier(yuv_tex_[0].get(), RenderTextureLayout::SHADER_READ),
        RenderTextureBarrier(yuv_tex_[1].get(), RenderTextureLayout::SHADER_READ),
        RenderTextureBarrier(yuv_tex_[2].get(), RenderTextureLayout::SHADER_READ),
    };
    command_list_->barriers(RenderBarrierStage::GRAPHICS, to_read, 3);
  }

  RenderTexture* tex = swap_chain_->getTexture(image_index);
  command_list_->barriers(RenderBarrierStage::GRAPHICS,
                           RenderTextureBarrier(tex, RenderTextureLayout::COLOR_WRITE));

  command_list_->setFramebuffer(framebuffers_[image_index].get());

  const uint32_t w = swap_chain_->getWidth();
  const uint32_t h = swap_chain_->getHeight();
  command_list_->setViewports(RenderViewport(0.0f, 0.0f, float(w), float(h)));
  command_list_->setScissors(RenderRect(0, 0, w, h));
  command_list_->clearColor(0, RenderColor(0.0f, 0.0f, 0.15f, 1.0f));

  if (yuv_desc_set_ && yuv_tex_[0]) {
    command_list_->setGraphicsPipelineLayout(pipeline_layout_.get());
    command_list_->setPipeline(pipeline_.get());
    command_list_->setVertexBuffers(0, &vertex_buffer_view_, 1, &input_slot_);
    command_list_->setGraphicsDescriptorSet(yuv_desc_set_.get(), 0);
    command_list_->drawInstanced(6, 1, 0, 0);
  }

  command_list_->barriers(RenderBarrierStage::NONE,
                           RenderTextureBarrier(tex, RenderTextureLayout::PRESENT));
  command_list_->end();

  while (release_semaphores_.size() < swap_chain_->getTextureCount()) {
    release_semaphores_.emplace_back(device_->createCommandSemaphore());
  }

  const RenderCommandList* cmd    = command_list_.get();
  RenderCommandSemaphore*  wait   = acquire_semaphore_.get();
  RenderCommandSemaphore*  signal = release_semaphores_[image_index].get();

  command_queue_->executeCommandLists(&cmd, 1, &wait, 1, &signal, 1, fence_.get());
  swap_chain_->present(image_index, &signal, 1);
  command_queue_->waitForCommandFence(fence_.get());
}

void PlumeRenderer::RenderThread() {
  std::unique_ptr<RenderInterface> iface = CreateD3D12Interface();

  device_        = iface->createDevice();
  command_queue_ = device_->createCommandQueue(RenderCommandListType::DIRECT);
  fence_         = device_->createCommandFence();

  swap_chain_ = command_queue_->createSwapChain(
      RenderSwapChainDesc(hwnd_, kSwapchainFormat, kBufferCount));
  swap_chain_->resize();

  command_list_      = command_queue_->createCommandList();
  acquire_semaphore_ = device_->createCommandSemaphore();

  CreateFramebuffers();

  RenderSamplerDesc sampler_desc;
  sampler_desc.minFilter = RenderFilter::LINEAR;
  sampler_desc.magFilter = RenderFilter::LINEAR;
  sampler_desc.addressU  = RenderTextureAddressMode::CLAMP;
  sampler_desc.addressV  = RenderTextureAddressMode::CLAMP;
  sampler_desc.addressW  = RenderTextureAddressMode::CLAMP;
  yuv_sampler_ = device_->createSampler(sampler_desc);

  EnsureYUVTextures(4, 4, 2, 2, 2, 2);
  {
    uint8_t* ydst = static_cast<uint8_t*>(yuv_upload_[0]->map());
    for (uint32_t r = 0; r < 4; r++) {
      for (uint32_t c = 0; c < 4; c++) {
        ydst[r * yuv_upload_pitch_[0] + c] = ((r ^ c) & 1) ? 220 : 32;
      }
    }
    yuv_upload_[0]->unmap();

    uint8_t* udst = static_cast<uint8_t*>(yuv_upload_[1]->map());
    uint8_t* vdst = static_cast<uint8_t*>(yuv_upload_[2]->map());
    for (uint32_t r = 0; r < 2; r++) {
      for (uint32_t c = 0; c < 2; c++) {
        udst[r * yuv_upload_pitch_[1] + c] = 128;
        vdst[r * yuv_upload_pitch_[2] + c] = 128;
      }
    }
    yuv_upload_[1]->unmap();
    yuv_upload_[2]->unmap();
    yuv_copy_pending_ = true;
  }

  RenderDescriptorRange desc_ranges[2] = {
      RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE, 0, 3),
      RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER, 3, 1),
  };
  RenderDescriptorSetDesc desc_set_desc(desc_ranges, 2);

  RenderPipelineLayoutDesc layout_desc;
  layout_desc.descriptorSetDescs      = &desc_set_desc;
  layout_desc.descriptorSetDescsCount = 1;
  layout_desc.allowInputLayout        = true;
  pipeline_layout_ = device_->createPipelineLayout(layout_desc);

  RenderShaderFormat fmt = iface->getCapabilities().shaderFormat;
  std::unique_ptr<RenderShader> vs, ps;
#ifdef _WIN64
  if (fmt == RenderShaderFormat::DXIL) {
    vs = device_->createShader(videoVertBlobDXIL, sizeof(videoVertBlobDXIL), "VSMain", fmt);
    ps = device_->createShader(videoFragBlobDXIL, sizeof(videoFragBlobDXIL), "PSMain", fmt);
  } else
#endif
  {
    vs = device_->createShader(videoVertBlobSPIRV, sizeof(videoVertBlobSPIRV), "VSMain", fmt);
    ps = device_->createShader(videoFragBlobSPIRV, sizeof(videoFragBlobSPIRV), "PSMain", fmt);
  }

  input_slot_ = RenderInputSlot(0, sizeof(float) * 5);
  RenderInputElement elements[] = {
      RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32B32_FLOAT, 0, 0),
      RenderInputElement("TEXCOORD", 0, 1, RenderFormat::R32G32_FLOAT,    0,
                         sizeof(float) * 3),
  };

  RenderGraphicsPipelineDesc pipe_desc;
  pipe_desc.inputSlots           = &input_slot_;
  pipe_desc.inputSlotsCount      = 1;
  pipe_desc.inputElements        = elements;
  pipe_desc.inputElementsCount   = 2;
  pipe_desc.pipelineLayout       = pipeline_layout_.get();
  pipe_desc.vertexShader         = vs.get();
  pipe_desc.pixelShader          = ps.get();
  pipe_desc.renderTargetFormat[0]= kSwapchainFormat;
  pipe_desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
  pipe_desc.renderTargetCount    = 1;
  pipe_desc.primitiveTopology    = RenderPrimitiveTopology::TRIANGLE_LIST;
  pipeline_ = device_->createGraphicsPipeline(pipe_desc);

  const VideoQuadVert verts[6] = {
      {-1.f,  1.f, 0.f,  0.f, 0.f},
      { 1.f,  1.f, 0.f,  1.f, 0.f},
      {-1.f, -1.f, 0.f,  0.f, 1.f},
      {-1.f, -1.f, 0.f,  0.f, 1.f},
      { 1.f,  1.f, 0.f,  1.f, 0.f},
      { 1.f, -1.f, 0.f,  1.f, 1.f},
  };
  vertex_buffer_ = device_->createBuffer(
      RenderBufferDesc::VertexBuffer(sizeof(verts), RenderHeapType::UPLOAD));
  {
    void* mapped = vertex_buffer_->map();
    std::memcpy(mapped, verts, sizeof(verts));
    vertex_buffer_->unmap();
  }
  vertex_buffer_view_ = RenderVertexBufferView(vertex_buffer_.get(), sizeof(verts));

  while (running_.load(std::memory_order_acquire)) {
    if (resize_pending_.load(std::memory_order_acquire)) {
      HandleResize();
    }
    Render();
  }

  command_queue_->waitForCommandFence(fence_.get());
}

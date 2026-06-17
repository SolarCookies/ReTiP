#include "video_hooks.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/ppc/context.h>
#include <rex/ppc/function.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/thread_state.h>

#include "plume_renderer.h"

#define REX_PPC_EXTERN_IMPORT(function) \
    REX_EXTERN(__imp__rex_##function)

#define REX_PPC_HOOK(function) \
    REX_HOOK(rex_##function, function##_Hook)

static inline float swap_float(uint32_t be) {
    uint32_t le = std::byteswap(be);
    float f;
    std::memcpy(&f, &le, 4);
    return f;
}

static inline float read_be_float(uint8_t* membase, uint32_t guest_addr) {
    uint32_t raw;
    std::memcpy(&raw, membase + guest_addr, 4);
    return swap_float(raw);
}

static inline uint32_t read_be_u32(uint8_t* membase, uint32_t guest_addr) {
    uint32_t raw;
    std::memcpy(&raw, membase + guest_addr, 4);
    return std::byteswap(raw);
}

static inline void write_be_u32(uint8_t* membase, uint32_t guest_addr, uint32_t val) {
    uint32_t be = std::byteswap(val);
    std::memcpy(membase + guest_addr, &be, 4);
}

PlumeRenderer* g_video_renderer = nullptr;

REX_PPC_EXTERN_IMPORT(render_D3DDevice_Clear_8265A2A8);
int render_D3DDevice_Clear_8265A2A8_Hook(uint32_t /*device*/) {
    return 0;
}
REX_PPC_HOOK(render_D3DDevice_Clear_8265A2A8);

REX_PPC_EXTERN_IMPORT(render_D3DDevice_SetScissorRect_82657F08);
int render_D3DDevice_SetScissorRect_82657F08_Hook(uint32_t /*device*/, uint32_t /*rect_ptr*/) {
    return 0;
}
REX_PPC_HOOK(render_D3DDevice_SetScissorRect_82657F08);

REX_PPC_EXTERN_IMPORT(render_IDirect3DDevice9_SetViewport_82658000);
int render_IDirect3DDevice9_SetViewport_82658000_Hook(uint32_t /*device*/, uint32_t /*viewport_ptr*/) {
    return 0;
}
REX_PPC_HOOK(render_IDirect3DDevice9_SetViewport_82658000);

REX_PPC_EXTERN_IMPORT(render_IDirect3DDevice9_Swap_8266FBB0);
int render_IDirect3DDevice9_Swap_8266FBB0_Hook(uint32_t /*device*/, uint32_t /*front_buffer*/, uint32_t /*params*/) {
    return 0;
}
REX_PPC_HOOK(render_IDirect3DDevice9_Swap_8266FBB0);

REX_PPC_EXTERN_IMPORT(render_D3DDevice_Present_826703E0);
int render_D3DDevice_Present_826703E0_Hook(uint32_t /*device*/, uint32_t /*a2*/, uint64_t /*a3*/) {
    return 0;
}
REX_PPC_HOOK(render_D3DDevice_Present_826703E0);

REX_PPC_EXTERN_IMPORT(render_CCalGraphicsRenderer_Create_829690D8);
int render_CCalGraphicsRenderer_Create_829690D8_Hook(uint32_t /*this_ptr*/) {
    //REXLOG_ERROR("[video] CCalGraphicsRenderer::Create called – stubbed");
    return 0;
}
REX_PPC_HOOK(render_CCalGraphicsRenderer_Create_829690D8);

REX_PPC_EXTERN_IMPORT(render_CCalVideoRenderer_Close_8296A838);
int render_CCalVideoRenderer_Close_8296A838_Hook(uint32_t /*this_ptr*/) {
    //REXLOG_ERROR("[video] CCalVideoRenderer::Close called – stubbed");
    return 0;
}
REX_PPC_HOOK(render_CCalVideoRenderer_Close_8296A838);

REX_PPC_EXTERN_IMPORT(render_CCalVideoRenderer_InitializeShaders_8296AF30);
int render_CCalVideoRenderer_InitializeShaders_8296AF30_Hook(uint32_t /*this_ptr*/) {
    //REXLOG_ERROR("[video] CCalVideoRenderer::InitializeShaders called – stubbed");
    return 0;
}
REX_PPC_HOOK(render_CCalVideoRenderer_InitializeShaders_8296AF30);

REX_PPC_EXTERN_IMPORT(render_CCalVideoRenderer_InitializeVertices_8296A748);
int render_CCalVideoRenderer_InitializeVertices_8296A748_Hook(uint32_t /*this_ptr*/) {
    //REXLOG_ERROR("[video] CCalVideoRenderer::InitializeVertices called – stubbed");
    return 0;
}
REX_PPC_HOOK(render_CCalVideoRenderer_InitializeVertices_8296A748);

REX_PPC_EXTERN_IMPORT(render_CCalVideoRenderer_SetMediaInfo_8296A940);
int render_CCalVideoRenderer_SetMediaInfo_8296A940_Hook(uint32_t this_ptr, uint32_t pVideoInfo) {
    auto* runtime = rex::Runtime::instance();
    uint8_t* mem  = runtime ? runtime->memory()->virtual_membase() : nullptr;
    if (!mem || !pVideoInfo) return 0;

    const uint32_t width  = read_be_u32(mem, pVideoInfo + 56);
    const uint32_t height = read_be_u32(mem, pVideoInfo + 60);

    if (!width || !height) return 0;

    const uint32_t yw = width,       yh = height;
    const uint32_t uw = width / 2,   uh = height / 2;
    const uint32_t vw = width / 2,   vh = height / 2;

    write_be_u32(mem, this_ptr + 0x14C, yw);
    write_be_u32(mem, this_ptr + 0x150, uw);
    write_be_u32(mem, this_ptr + 0x154, vw);
    write_be_u32(mem, this_ptr + 0x158, yw);
    write_be_u32(mem, this_ptr + 0x15C, uw);
    write_be_u32(mem, this_ptr + 0x160, vw);
    write_be_u32(mem, this_ptr + 0x164, yh);
    write_be_u32(mem, this_ptr + 0x168, uh);
    write_be_u32(mem, this_ptr + 0x16C, vh);

    return 0;
}
REX_PPC_HOOK(render_CCalVideoRenderer_SetMediaInfo_8296A940);

REX_PPC_EXTERN_IMPORT(render_CCalVideoRenderer_Render_8296AB38);
int render_CCalVideoRenderer_Render_8296AB38_Hook(uint32_t this_ptr, uint32_t ul_num_bytes) {
    
    (void)ul_num_bytes;

    if (!g_video_renderer) return 0;

    auto* runtime = rex::Runtime::instance();
    if (!runtime) return 0;
    uint8_t* mem = runtime->memory()->virtual_membase();

    constexpr uint32_t kVertexStride = 20;
    constexpr uint32_t kVertexBase   = 0x90;
    float verts[6][4];
    for (int i = 0; i < 6; i++) {
        uint32_t base = this_ptr + kVertexBase + i * kVertexStride;
        verts[i][0] = read_be_float(mem, base +  0);
        verts[i][1] = read_be_float(mem, base +  4);
        verts[i][2] = read_be_float(mem, base + 12);
        verts[i][3] = read_be_float(mem, base + 16);
    }
    g_video_renderer->SubmitVideoQuad(verts);
    
    uint32_t decoder   = read_be_u32(mem, this_ptr + 0x2C);
    uint32_t vtable    = decoder ? read_be_u32(mem, decoder) : 0;
    uint32_t get_frame = vtable  ? read_be_u32(mem, vtable + 80) : 0;

    if (!decoder || !get_frame) return 0;

    auto* ts = rex::runtime::ThreadState::Get();
    auto* fd = runtime->function_dispatcher();
    if (!ts || !fd) return 0;

    uint64_t args[1] = { static_cast<uint64_t>(decoder) };
    uint64_t result  = fd->Execute(ts, get_frame, args, 1);
    uint32_t frame_ptr = static_cast<uint32_t>(result & 0xFFFFFFFF);
    
    uint8_t* mem2  = runtime->memory()->virtual_membase();
    uint32_t y_row = read_be_u32(mem2, this_ptr + 0x14C);
    uint32_t u_row = read_be_u32(mem2, this_ptr + 0x150);
    uint32_t v_row = read_be_u32(mem2, this_ptr + 0x154);
    uint32_t y_str = read_be_u32(mem2, this_ptr + 0x158);
    uint32_t u_str = read_be_u32(mem2, this_ptr + 0x15C);
    uint32_t v_str = read_be_u32(mem2, this_ptr + 0x160);
    uint32_t y_h   = read_be_u32(mem2, this_ptr + 0x164);
    uint32_t u_h   = read_be_u32(mem2, this_ptr + 0x168);
    uint32_t v_h   = read_be_u32(mem2, this_ptr + 0x16C);

    if (!y_row || !y_h || !u_row || !u_h || !v_row || !v_h) return 0;

    auto is_valid_plane_ptr = [](uint32_t p) -> bool {
        return (p & 0x3u) == 0 && p >= 0x40000000u && p < 0x80000000u;
    };
    
    uint32_t y_src = 0, u_src = 0, v_src = 0;

    {
        uint32_t p0 = read_be_u32(mem2, frame_ptr + 0x00);
        uint32_t p1 = read_be_u32(mem2, frame_ptr + 0x04);
        uint32_t p2 = read_be_u32(mem2, frame_ptr + 0x08);
        if (is_valid_plane_ptr(p0) && is_valid_plane_ptr(p1) && is_valid_plane_ptr(p2)) {
            y_src = p0; u_src = p1; v_src = p2;
        }
    }

    if (!y_src) {
        uint32_t q0 = read_be_u32(mem2, frame_ptr + 0x10);
        uint32_t q1 = read_be_u32(mem2, frame_ptr + 0x14);
        uint32_t q2 = read_be_u32(mem2, frame_ptr + 0x18);
        if (is_valid_plane_ptr(q0) && is_valid_plane_ptr(q1) && is_valid_plane_ptr(q2)) {
            y_src = q0; u_src = q1; v_src = q2;
        }
    }

    if (!y_src && is_valid_plane_ptr(frame_ptr)) {
        uint32_t u_off = frame_ptr + y_h * y_str;
        uint32_t v_off = u_off   + u_h * u_str;
        if (is_valid_plane_ptr(u_off) && is_valid_plane_ptr(v_off)) {
            y_src = frame_ptr;
            u_src = u_off;
            v_src = v_off;
        }
    }

    if (!y_src || !u_src || !v_src) return 0;
    if ((uint64_t)y_src + (uint64_t)y_h * y_str > 0x4B999999u) return 0;
    if ((uint64_t)u_src + (uint64_t)u_h * u_str > 0x4B999999u) return 0;
    if ((uint64_t)v_src + (uint64_t)v_h * v_str > 0x4B999999u) return 0;

    YUVFrame frame;
    frame.y_width  = y_row;  frame.y_height = y_h;
    frame.u_width  = u_row;  frame.u_height = u_h;
    frame.v_width  = v_row;  frame.v_height = v_h;

    frame.y.resize(y_row * y_h);
    for (uint32_t r = 0; r < y_h; r++){
        std::memcpy(frame.y.data() + r * y_row, mem2 + y_src + r * y_str, y_row);
    }

    frame.u.resize(u_row * u_h);
    for (uint32_t r = 0; r < u_h; r++){
        std::memcpy(frame.u.data() + r * u_row, mem2 + u_src + r * u_str, u_row);
    }

    frame.v.resize(v_row * v_h);
    for (uint32_t r = 0; r < v_h; r++){
        std::memcpy(frame.v.data() + r * v_row, mem2 + v_src + r * v_str, v_row);
    }

    g_video_renderer->SubmitYUVFrame(std::move(frame));

    return 0;
}
REX_PPC_HOOK(render_CCalVideoRenderer_Render_8296AB38);

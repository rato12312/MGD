#pragma once

// Maxwell GPU command buffer decoder + shader recompiler (host side).
// Honest stub: parse known commands, dump unknown, execute simple draw.
// Real recompiler would translate to host GPU API (Vulkan/D3D12/Metal).
// Here we just track state, count draw calls, and advance fences.

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

namespace mgd {
namespace gpu {

// Maxwell command opcodes (subset used by Switch games)
enum class MaxwellCmd : uint32_t {
    NOP                = 0x0000,
    SET_OBJECT         = 0x0001,
    SET_DRAW_ARRAYS    = 0x0086,
    SET_DRAW_INDEXED   = 0x0087,
    SET_VERTEX_BUFFER  = 0x00C0,
    SET_INDEX_BUFFER   = 0x00C1,
    SET_CONSTANT_BUFFER= 0x00C2,
    SET_SHADER         = 0x00C3,
    SET_VIEWPORT       = 0x00C4,
    SET_SCISSOR        = 0x00C5,
    SET_BLEND_STATE    = 0x00C6,
    SET_DEPTH_STATE    = 0x00C7,
    SET_RASTER_STATE   = 0x00C8,
    SET_TEXTURE        = 0x00C9,
    SET_SAMPLER        = 0x00CA,
    SET_RENDER_TARGET  = 0x00CB,
    CLEAR              = 0x00CC,
    BARRIER            = 0x00CD,
    COMPUTE_DISPATCH   = 0x00CE,
    SET_COMPUTE_SHADER = 0x00CF,
};

// Shader types
enum class ShaderStage : uint8_t {
    VERTEX = 0,
    FRAGMENT = 1,
    COMPUTE = 2,
};

// Texture format (subset)
enum class TexFormat : uint32_t {
    RGBA8 = 0x1,
    RGBA16F = 0x2,
    R8 = 0x3,
    D24S8 = 0x4,
};

// Simple shader representation (host-recompiled)
struct Shader {
    ShaderStage stage = ShaderStage::VERTEX;
    std::vector<uint32_t> bytecode; // Maxwell ISA
    std::string debug_name;
};

// Texture handle
struct Texture {
    uint32_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    TexFormat format = TexFormat::RGBA8;
    std::vector<uint8_t> data;
};

// Sampler state
struct Sampler {
    uint32_t id = 0;
    uint32_t min_filter = 0;
    uint32_t mag_filter = 0;
    uint32_t wrap_s = 0;
    uint32_t wrap_t = 0;
};

// Render target (color + depth)
struct RenderTarget {
    uint32_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Texture> colors;
    Texture depth;
};

// Vertex buffer binding
struct VertexBuffer {
    uint64_t gpu_addr = 0;
    uint32_t stride = 0;
    uint32_t offset = 0;
    uint32_t divisor = 0;
};

// Index buffer binding
struct IndexBuffer {
    uint64_t gpu_addr = 0;
    uint32_t index_type = 0; // 0=uint16, 1=uint32
    uint32_t offset = 0;
};

// Constant buffer binding
struct ConstantBuffer {
    uint64_t gpu_addr = 0;
    uint32_t size = 0;
    uint32_t slot = 0;
};

// GPU state tracked from command buffer
struct GpuState {
    Shader* vs = nullptr;
    Shader* fs = nullptr;
    Shader* cs = nullptr;
    std::vector<VertexBuffer> vertex_buffers;
    IndexBuffer index_buffer;
    std::vector<ConstantBuffer> constant_buffers;
    std::vector<Texture*> textures;
    std::vector<Sampler*> samplers;
    RenderTarget* rt = nullptr;
    uint32_t viewport_x = 0, viewport_y = 0;
    uint32_t viewport_w = 1280, viewport_h = 720;
    uint32_t scissor_x = 0, scissor_y = 0;
    uint32_t scissor_w = 1280, scissor_h = 720;
    uint32_t draw_count = 0;
    uint32_t compute_dispatch_count = 0;
};

// Command buffer parser
class MaxwellDecoder {
public:
    // Parse command buffer (uint32_t stream), execute on state, return bytes consumed
    static size_t parse(const uint32_t* cmds, size_t count, GpuState& state,
                        std::unordered_map<uint32_t, Shader>& shaders,
                        std::unordered_map<uint32_t, Texture>& textures,
                        std::unordered_map<uint32_t, Sampler>& samplers,
                        std::unordered_map<uint32_t, RenderTarget>& render_targets,
                        std::vector<uint32_t>& unknown_ops);

    // Recompile Maxwell shader to host (stub: returns empty for now)
    static Shader recompileShader(ShaderStage stage, const uint32_t* maxwell_code, size_t count);

    // Create texture from guest memory
    static Texture createTexture(uint32_t id, uint32_t w, uint32_t h, TexFormat fmt, const uint8_t* data, size_t size);

    // Create sampler
    static Sampler createSampler(uint32_t id, uint32_t min_f, uint32_t mag_f, uint32_t wrap_s, uint32_t wrap_t);

    // Create render target
    static RenderTarget createRenderTarget(uint32_t id, uint32_t w, uint32_t h);
};

// GPU execution context (used by Emulator to run submitted command buffers)
class GpuExecutor {
public:
    GpuExecutor();
    ~GpuExecutor() = default;

    // Execute a submitted command buffer, return true if work was done
    bool execute(const uint8_t* cmd_buf, size_t size);

    // Resource creation (called via IPC from game)
    uint32_t createTexture(uint32_t w, uint32_t h, TexFormat fmt, const uint8_t* data, size_t size);
    uint32_t createSampler(uint32_t min_f, uint32_t mag_f, uint32_t wrap_s, uint32_t wrap_t);
    uint32_t createRenderTarget(uint32_t w, uint32_t h);
    uint32_t createShader(ShaderStage stage, const std::vector<uint32_t>& bytecode);

    // Stats
    uint64_t totalDrawCalls() const { return draw_calls_; }
    uint64_t totalComputeDispatches() const { return compute_dispatches_; }
    uint64_t totalBytesExecuted() const { return bytes_executed_; }

private:
    GpuState state_;
    std::unordered_map<uint32_t, Shader> shaders_;
    std::unordered_map<uint32_t, Texture> textures_;
    std::unordered_map<uint32_t, Sampler> samplers_;
    std::unordered_map<uint32_t, RenderTarget> render_targets_;
    std::vector<uint32_t> unknown_ops_;
    uint64_t draw_calls_ = 0;
    uint64_t compute_dispatches_ = 0;
    uint64_t bytes_executed_ = 0;
    uint32_t next_tex_id_ = 1;
    uint32_t next_samp_id_ = 1;
    uint32_t next_rt_id_ = 1;
    uint32_t next_shader_id_ = 1;
};

} // namespace gpu
} // namespace mgd
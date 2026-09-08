// Maxwell GPU command buffer decoder + shader recompiler (host side).
// Honest stub: parse known commands, dump unknown, execute simple draw.
// Real recompiler would translate to host GPU API (Vulkan/D3D12/Metal).
// Here we just track state, count draw calls, and advance fences.

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>

namespace mgd {
namespace gpu {

size_t MaxwellDecoder::parse(const uint32_t* cmds, size_t count, GpuState& state,
                             std::unordered_map<uint32_t, Shader>& shaders,
                             std::unordered_map<uint32_t, Texture>& textures,
                             std::unordered_map<uint32_t, Sampler>& samplers,
                             std::unordered_map<uint32_t, RenderTarget>& render_targets,
                             std::vector<uint32_t>& unknown_ops) {
    size_t i = 0;
    while (i < count) {
        uint32_t op = cmds[i];
        uint32_t opcode = op & 0xFFFF;
        uint32_t size = (op >> 16) & 0xFFFF; // size in dwords including header
        if (size == 0) size = 1;
        if (i + size > count) {
            unknown_ops.push_back(op);
            break;
        }
        switch (static_cast<MaxwellCmd>(opcode)) {
            case MaxwellCmd::NOP:
                break;
            case MaxwellCmd::SET_OBJECT: {
                // Next dword = object id (shader, texture, etc.)
                if (i + 1 < count) {
                    // Handled by subsequent commands
                }
                break;
            }
            case MaxwellCmd::SET_SHADER: {
                if (i + 2 < count) {
                    uint32_t shader_id = cmds[i + 1];
                    uint32_t stage = cmds[i + 2] & 0xFF;
                    auto it = shaders.find(shader_id);
                    if (it != shaders.end()) {
                        if (stage == 0) state.vs = &it->second;
                        else if (stage == 1) state.fs = &it->second;
                        else if (stage == 2) state.cs = &it->second;
                    }
                }
                break;
            }
            case MaxwellCmd::SET_VERTEX_BUFFER: {
                if (i + 4 < count) {
                    VertexBuffer vb;
                    vb.gpu_addr = (static_cast<uint64_t>(cmds[i + 2]) << 32) | cmds[i + 1];
                    vb.stride = cmds[i + 3];
                    vb.offset = cmds[i + 4];
                    if (state.vertex_buffers.size() <= (cmds[i + 1] & 0xF))
                        state.vertex_buffers.resize((cmds[i + 1] & 0xF) + 1);
                    state.vertex_buffers[cmds[i + 1] & 0xF] = vb;
                }
                break;
            }
            case MaxwellCmd::SET_INDEX_BUFFER: {
                if (i + 3 < count) {
                    state.index_buffer.gpu_addr = (static_cast<uint64_t>(cmds[i + 2]) << 32) | cmds[i + 1];
                    state.index_buffer.index_type = cmds[i + 3] & 0x1;
                    state.index_buffer.offset = (i + 4 < count) ? cmds[i + 4] : 0;
                }
                break;
            }
            case MaxwellCmd::SET_CONSTANT_BUFFER: {
                if (i + 3 < count) {
                    ConstantBuffer cb;
                    cb.gpu_addr = (static_cast<uint64_t>(cmds[i + 2]) << 32) | cmds[i + 1];
                    cb.size = cmds[i + 3];
                    cb.slot = (i + 4 < count) ? cmds[i + 4] : 0;
                    state.constant_buffers.push_back(cb);
                }
                break;
            }
            case MaxwellCmd::SET_TEXTURE: {
                if (i + 1 < count) {
                    uint32_t tex_id = cmds[i + 1];
                    auto it = textures.find(tex_id);
                    if (it != textures.end()) {
                        if (state.textures.size() <= (cmds[i + 1] & 0xF))
                            state.textures.resize((cmds[i + 1] & 0xF) + 1);
                        state.textures[cmds[i + 1] & 0xF] = &it->second;
                    }
                }
                break;
            }
            case MaxwellCmd::SET_SAMPLER: {
                if (i + 1 < count) {
                    uint32_t samp_id = cmds[i + 1];
                    auto it = samplers.find(samp_id);
                    if (it != samplers.end()) {
                        if (state.samplers.size() <= (cmds[i + 1] & 0xF))
                            state.samplers.resize((cmds[i + 1] & 0xF) + 1);
                        state.samplers[cmds[i + 1] & 0xF] = &it->second;
                    }
                }
                break;
            }
            case MaxwellCmd::SET_RENDER_TARGET: {
                if (i + 1 < count) {
                    uint32_t rt_id = cmds[i + 1];
                    auto it = render_targets.find(rt_id);
                    if (it != render_targets.end()) {
                        state.rt = &it->second;
                    }
                }
                break;
            }
            case MaxwellCmd::SET_VIEWPORT: {
                if (i + 4 < count) {
                    state.viewport_x = cmds[i + 1];
                    state.viewport_y = cmds[i + 2];
                    state.viewport_w = cmds[i + 3];
                    state.viewport_h = cmds[i + 4];
                }
                break;
            }
            case MaxwellCmd::SET_SCISSOR: {
                if (i + 4 < count) {
                    state.scissor_x = cmds[i + 1];
                    state.scissor_y = cmds[i + 2];
                    state.scissor_w = cmds[i + 3];
                    state.scissor_h = cmds[i + 4];
                }
                break;
            }
            case MaxwellCmd::SET_DRAW_ARRAYS: {
                if (i + 2 < count) {
                    uint32_t vertex_count = cmds[i + 1];
                    uint32_t instance_count = cmds[i + 2];
                    // Execute draw
                    state.draw_count += instance_count > 0 ? instance_count : 1;
                }
                break;
            }
            case MaxwellCmd::SET_DRAW_INDEXED: {
                if (i + 3 < count) {
                    uint32_t index_count = cmds[i + 1];
                    uint32_t instance_count = cmds[i + 2];
                    uint32_t base_vertex = cmds[i + 3];
                    state.draw_count += instance_count > 0 ? instance_count : 1;
                }
                break;
            }
            case MaxwellCmd::CLEAR: {
                if (i + 1 < count) {
                    uint32_t flags = cmds[i + 1];
                    // Simulate clear
                }
                break;
            }
            case MaxwellCmd::BARRIER: {
                // Memory barrier
                break;
            }
            case MaxwellCmd::COMPUTE_DISPATCH: {
                if (i + 3 < count) {
                    uint32_t x = cmds[i + 1];
                    uint32_t y = cmds[i + 2];
                    uint32_t z = cmds[i + 3];
                    state.compute_dispatch_count += x * y * z;
                }
                break;
            }
            case MaxwellCmd::SET_COMPUTE_SHADER: {
                if (i + 1 < count) {
                    uint32_t shader_id = cmds[i + 1];
                    auto it = shaders.find(shader_id);
                    if (it != shaders.end()) state.cs = &it->second;
                }
                break;
            }
            default:
                unknown_ops.push_back(op);
                break;
        }
        i += size;
    }
    return i;
}

Shader MaxwellDecoder::recompileShader(ShaderStage stage, const uint32_t* maxwell_code, size_t count) {
    Shader s;
    s.stage = stage;
    s.bytecode.assign(maxwell_code, maxwell_code + count);
    s.debug_name = (stage == ShaderStage::VERTEX) ? "VS" : (stage == ShaderStage::FRAGMENT) ? "FS" : "CS";
    return s;
}

Texture MaxwellDecoder::createTexture(uint32_t id, uint32_t w, uint32_t h, TexFormat fmt, const uint8_t* data, size_t size) {
    Texture t;
    t.id = id;
    t.width = w;
    t.height = h;
    t.format = fmt;
    size_t expected = w * h * (fmt == TexFormat::RGBA8 ? 4 : (fmt == TexFormat::RGBA16F ? 8 : 1));
    if (data && size >= expected) t.data.assign(data, data + expected);
    else t.data.resize(expected);
    return t;
}

Sampler MaxwellDecoder::createSampler(uint32_t id, uint32_t min_f, uint32_t mag_f, uint32_t wrap_s, uint32_t wrap_t) {
    Sampler s;
    s.id = id;
    s.min_filter = min_f;
    s.mag_filter = mag_f;
    s.wrap_s = wrap_s;
    s.wrap_t = wrap_t;
    return s;
}

RenderTarget MaxwellDecoder::createRenderTarget(uint32_t id, uint32_t w, uint32_t h) {
    RenderTarget rt;
    rt.id = id;
    rt.width = w;
    rt.height = h;
    rt.colors.resize(1);
    rt.colors[0] = createTexture(0, w, h, TexFormat::RGBA8, nullptr, 0);
    rt.depth = createTexture(0, w, h, TexFormat::D24S8, nullptr, 0);
    return rt;
}

GpuExecutor::GpuExecutor() = default;

bool GpuExecutor::execute(const uint8_t* cmd_buf, size_t size) {
    if (!cmd_buf || size < 4) return false;
    size_t dword_count = size / 4;
    const uint32_t* cmds = reinterpret_cast<const uint32_t*>(cmd_buf);
    size_t parsed = MaxwellDecoder::parse(cmds, dword_count, state_, shaders_, textures_, samplers_, render_targets_, unknown_ops_);
    bytes_executed_ += parsed * 4;
    draw_calls_ += state_.draw_count;
    compute_dispatches_ += state_.compute_dispatch_count;
    state_.draw_count = 0;
    state_.compute_dispatch_count = 0;
    return parsed > 0;
}

uint32_t GpuExecutor::createTexture(uint32_t w, uint32_t h, TexFormat fmt, const uint8_t* data, size_t size) {
    uint32_t id = next_tex_id_++;
    textures_[id] = MaxwellDecoder::createTexture(id, w, h, fmt, data, size);
    return id;
}

uint32_t GpuExecutor::createSampler(uint32_t min_f, uint32_t mag_f, uint32_t wrap_s, uint32_t wrap_t) {
    uint32_t id = next_samp_id_++;
    samplers_[id] = MaxwellDecoder::createSampler(id, min_f, mag_f, wrap_s, wrap_t);
    return id;
}

uint32_t GpuExecutor::createRenderTarget(uint32_t w, uint32_t h) {
    uint32_t id = next_rt_id_++;
    render_targets_[id] = MaxwellDecoder::createRenderTarget(id, w, h);
    return id;
}

uint32_t GpuExecutor::createShader(ShaderStage stage, const std::vector<uint32_t>& bytecode) {
    uint32_t id = next_shader_id_++;
    Shader s = MaxwellDecoder::recompileShader(stage, bytecode.data(), bytecode.size());
    shaders_[id] = s;
    return id;
}

} // namespace gpu
} // namespace mgd
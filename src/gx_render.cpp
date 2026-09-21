#include "wp/gx_render.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "wp/memory.h"

namespace wp::gx::render {

namespace {

constexpr int kEfbWidth = 640;
constexpr int kEfbHeight = 528;
constexpr uint32_t kMaxStages = 16;
constexpr uint32_t kTextureMaps = 8;
constexpr uint32_t kVertexCapacity = 1 << 16;
constexpr float kScissorOffset = 342.0f;

const char* kPresentShaderSource = R"HLSL(
Texture2D frame : register(t0);
SamplerState frame_sampler : register(s0);

struct Output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Output vertex_main(uint id : SV_VertexID) {
    Output output;
    float2 uv = float2((id << 1) & 2, id & 2);
    output.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    output.uv = uv;
    return output;
}

float4 pixel_main(Output input) : SV_Target {
    return float4(frame.Sample(frame_sampler, input.uv).rgb, 1);
}
)HLSL";

const char* kShaderSource = R"HLSL(
cbuffer Constants : register(b0) {
    float4 initial[4];
    float4 konst[4];
    uint4 stage[16];
    uint4 header;
    uint4 swaps;
};

Texture2D t0 : register(t0);
Texture2D t1 : register(t1);
Texture2D t2 : register(t2);
Texture2D t3 : register(t3);
Texture2D t4 : register(t4);
Texture2D t5 : register(t5);
Texture2D t6 : register(t6);
Texture2D t7 : register(t7);
SamplerState s0 : register(s0);
SamplerState s1 : register(s1);
SamplerState s2 : register(s2);
SamplerState s3 : register(s3);
SamplerState s4 : register(s4);
SamplerState s5 : register(s5);
SamplerState s6 : register(s6);
SamplerState s7 : register(s7);

struct VertexInput {
    float3 position : POSITION;
    float4 c0 : COLOR0;
    float4 c1 : COLOR1;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
    float2 uv4 : TEXCOORD4;
    float2 uv5 : TEXCOORD5;
    float2 uv6 : TEXCOORD6;
    float2 uv7 : TEXCOORD7;
};

struct PixelInput {
    float4 position : SV_Position;
    float4 c0 : COLOR0;
    float4 c1 : COLOR1;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float2 uv2 : TEXCOORD2;
    float2 uv3 : TEXCOORD3;
    float2 uv4 : TEXCOORD4;
    float2 uv5 : TEXCOORD5;
    float2 uv6 : TEXCOORD6;
    float2 uv7 : TEXCOORD7;
};

PixelInput vertex_main(VertexInput input) {
    PixelInput output;
    output.position = float4(input.position, 1.0);
    output.c0 = input.c0;
    output.c1 = input.c1;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;
    output.uv2 = input.uv2;
    output.uv3 = input.uv3;
    output.uv4 = input.uv4;
    output.uv5 = input.uv5;
    output.uv6 = input.uv6;
    output.uv7 = input.uv7;
    return output;
}

float2 select_uv(PixelInput p, uint i) {
    switch (i) {
    case 0: return p.uv0;
    case 1: return p.uv1;
    case 2: return p.uv2;
    case 3: return p.uv3;
    case 4: return p.uv4;
    case 5: return p.uv5;
    case 6: return p.uv6;
    default: return p.uv7;
    }
}

float4 sample_map(uint m, float2 uv) {
    switch (m) {
    case 0: return t0.SampleLevel(s0, uv, 0);
    case 1: return t1.SampleLevel(s1, uv, 0);
    case 2: return t2.SampleLevel(s2, uv, 0);
    case 3: return t3.SampleLevel(s3, uv, 0);
    case 4: return t4.SampleLevel(s4, uv, 0);
    case 5: return t5.SampleLevel(s5, uv, 0);
    case 6: return t6.SampleLevel(s6, uv, 0);
    default: return t7.SampleLevel(s7, uv, 0);
    }
}

float3 konst_color(uint sel) {
    if (sel < 8) {
        return float3(1, 1, 1) * ((8 - sel) / 8.0);
    }
    if (sel >= 12 && sel < 16) {
        return konst[sel - 12].rgb;
    }
    if (sel >= 16) {
        uint k = (sel - 16) & 3;
        uint ch = (sel - 16) >> 2;
        return float3(1, 1, 1) * konst[k][ch];
    }
    return float3(1, 1, 1);
}

float konst_alpha(uint sel) {
    if (sel < 8) {
        return (8 - sel) / 8.0;
    }
    if (sel >= 16) {
        uint k = (sel - 16) & 3;
        uint ch = (sel - 16) >> 2;
        return konst[k][ch];
    }
    return 1.0;
}

float3 color_input(uint sel, float4 r0, float4 r1, float4 r2, float4 r3, float4 tex, float4 ras, float3 kc) {
    switch (sel) {
    case 0: return r0.rgb;
    case 1: return r0.aaa;
    case 2: return r1.rgb;
    case 3: return r1.aaa;
    case 4: return r2.rgb;
    case 5: return r2.aaa;
    case 6: return r3.rgb;
    case 7: return r3.aaa;
    case 8: return tex.rgb;
    case 9: return tex.aaa;
    case 10: return ras.rgb;
    case 11: return ras.aaa;
    case 12: return float3(1, 1, 1);
    case 13: return float3(0.5, 0.5, 0.5);
    case 14: return kc;
    default: return float3(0, 0, 0);
    }
}

float alpha_input(uint sel, float4 r0, float4 r1, float4 r2, float4 r3, float4 tex, float4 ras, float ka) {
    switch (sel) {
    case 0: return r0.a;
    case 1: return r1.a;
    case 2: return r2.a;
    case 3: return r3.a;
    case 4: return tex.a;
    case 5: return ras.a;
    case 6: return ka;
    default: return 0.0;
    }
}

bool compare_alpha(uint mode, uint value, uint reference) {
    switch (mode) {
    case 0: return false;
    case 1: return value < reference;
    case 2: return value == reference;
    case 3: return value <= reference;
    case 4: return value > reference;
    case 5: return value != reference;
    case 6: return value >= reference;
    default: return true;
    }
}

float4 apply_swap(float4 value, uint table) {
    uint packed = swaps[table];
    float components[4] = {value.r, value.g, value.b, value.a};
    return float4(components[packed & 3], components[(packed >> 2) & 3], components[(packed >> 4) & 3], components[(packed >> 6) & 3]);
}

float4 pixel_main(PixelInput p) : SV_Target {
    float4 r[4];
    r[0] = initial[0];
    r[1] = initial[1];
    r[2] = initial[2];
    r[3] = initial[3];
    uint count = header.x;
    if (count == 0) {
        r[0] = p.c0;
    }
    for (uint i = 0; i < count; i++) {
        uint ce = stage[i].x;
        uint ae = stage[i].y;
        uint order = stage[i].z;
        uint ksel = stage[i].w;
        float4 tex = float4(1, 1, 1, 1);
        if ((order & 0x40) != 0) {
            tex = apply_swap(sample_map(order & 7, select_uv(p, (order >> 3) & 7)), (ae >> 2) & 3);
        }
        float4 ras = float4(0, 0, 0, 0);
        uint chan = (order >> 7) & 7;
        if (chan == 0) {
            ras = p.c0;
        } else if (chan == 1) {
            ras = p.c1;
        }
        ras = apply_swap(ras, ae & 3);
        float3 kc = konst_color(ksel & 31);
        float ka = konst_alpha((ksel >> 5) & 31);
        float3 a = color_input((ce >> 12) & 15, r[0], r[1], r[2], r[3], tex, ras, kc);
        float3 b = color_input((ce >> 8) & 15, r[0], r[1], r[2], r[3], tex, ras, kc);
        float3 c = color_input((ce >> 4) & 15, r[0], r[1], r[2], r[3], tex, ras, kc);
        float3 d = color_input(ce & 15, r[0], r[1], r[2], r[3], tex, ras, kc);
        float sign = ((ce >> 18) & 1) != 0 ? -1.0 : 1.0;
        uint bias_code = (ce >> 16) & 3;
        float bias = bias_code == 1 ? 0.5 : (bias_code == 2 ? -0.5 : 0.0);
        uint scale_code = (ce >> 20) & 3;
        float scale = scale_code == 1 ? 2.0 : (scale_code == 2 ? 4.0 : (scale_code == 3 ? 0.5 : 1.0));
        float3 color = (d + sign * lerp(a, b, c) + bias) * scale;
        if (((ce >> 19) & 1) != 0) {
            color = saturate(color);
        } else {
            color = clamp(color, -1024.0 / 255.0, 1023.0 / 255.0);
        }
        float aa = alpha_input((ae >> 13) & 7, r[0], r[1], r[2], r[3], tex, ras, ka);
        float ab = alpha_input((ae >> 10) & 7, r[0], r[1], r[2], r[3], tex, ras, ka);
        float ac = alpha_input((ae >> 7) & 7, r[0], r[1], r[2], r[3], tex, ras, ka);
        float ad = alpha_input((ae >> 4) & 7, r[0], r[1], r[2], r[3], tex, ras, ka);
        float asign = ((ae >> 18) & 1) != 0 ? -1.0 : 1.0;
        uint abias_code = (ae >> 16) & 3;
        float abias = abias_code == 1 ? 0.5 : (abias_code == 2 ? -0.5 : 0.0);
        uint ascale_code = (ae >> 20) & 3;
        float ascale = ascale_code == 1 ? 2.0 : (ascale_code == 2 ? 4.0 : (ascale_code == 3 ? 0.5 : 1.0));
        float alpha = (ad + asign * lerp(aa, ab, ac) + abias) * ascale;
        if (((ae >> 19) & 1) != 0) {
            alpha = saturate(alpha);
        } else {
            alpha = clamp(alpha, -1024.0 / 255.0, 1023.0 / 255.0);
        }
        uint color_dest = (ce >> 22) & 3;
        uint alpha_dest = (ae >> 22) & 3;
        if (color_dest == 0) r[0].rgb = color;
        else if (color_dest == 1) r[1].rgb = color;
        else if (color_dest == 2) r[2].rgb = color;
        else r[3].rgb = color;
        if (alpha_dest == 0) r[0].a = alpha;
        else if (alpha_dest == 1) r[1].a = alpha;
        else if (alpha_dest == 2) r[2].a = alpha;
        else r[3].a = alpha;
    }
    float4 result = saturate(r[0]);
    uint compare = header.y;
    uint value = (uint)round(result.a * 255.0);
    bool first = compare_alpha((compare >> 16) & 7, value, compare & 255);
    bool second = compare_alpha((compare >> 19) & 7, value, (compare >> 8) & 255);
    uint logic = (compare >> 22) & 3;
    bool accepted = logic == 0 ? (first && second) : (logic == 1 ? (first || second) : (logic == 2 ? (first != second) : (first == second)));
    if (!accepted) {
        discard;
    }
    return result;
}
)HLSL";

struct Constants {
    float initial[4][4];
    float konst[4][4];
    uint32_t stage[kMaxStages][4];
    uint32_t header[4];
    uint32_t swaps[4];
};

template <typename T>
void release(T*& pointer) {
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}

struct CachedTexture {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* view = nullptr;
    uint64_t hash = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint64_t verified_frame = ~0ull;
};

struct CopiedTexture {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* view = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t logical_width = 0;
    uint32_t logical_height = 0;
    uint32_t bytes = 0;
    uint64_t guest_hash = 0;
};

struct Device {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11VertexShader* vertex_shader = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11InputLayout* layout = nullptr;
    ID3D11Buffer* vertex_buffer = nullptr;
    ID3D11Buffer* constants = nullptr;
    ID3D11Texture2D* target = nullptr;
    ID3D11RenderTargetView* target_view = nullptr;
    ID3D11Texture2D* depth = nullptr;
    ID3D11DepthStencilView* depth_view = nullptr;
    ID3D11Texture2D* frame = nullptr;
    ID3D11ShaderResourceView* frame_view = nullptr;
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    IDXGISwapChain1* swapchain = nullptr;
    ID3D11RenderTargetView* backbuffer_view = nullptr;
    UINT swapchain_width = 0;
    UINT swapchain_height = 0;
    ID3D11VertexShader* present_vertex = nullptr;
    ID3D11PixelShader* present_pixel = nullptr;
    ID3D11SamplerState* present_sampler = nullptr;
    ID3D11RasterizerState* present_rasterizer = nullptr;
    ID3D11RasterizerState* rasterizer = nullptr;
    std::map<uint32_t, ID3D11BlendState*> blend_states;
    std::map<uint32_t, ID3D11DepthStencilState*> depth_states;
    std::map<uint32_t, ID3D11SamplerState*> samplers;
    std::map<uint64_t, CachedTexture> textures;
    std::map<uint32_t, CopiedTexture> copies;
    bool failed = false;
    bool ready = false;
};

constexpr uint32_t kTlutSize = 0x100000;
constexpr uint32_t kTlutMask = 0x7FE00;

struct PendingBatch {
    std::vector<ScreenVertex> vertices;
    Constants constants;
    ID3D11ShaderResourceView* views[kTextureMaps];
    ID3D11SamplerState* samplers[kTextureMaps];
    uint32_t blend = 0;
    uint32_t depth = 0;
    uint32_t top_left = 0;
    uint32_t bottom_right = 0;
};

Device g_device;
PendingBatch g_pending;

void flush_pending();
bool g_logged_palette = false;
bool g_logged_copy_format = false;
uint64_t g_frame = 0;
uint8_t g_tlut[kTlutSize];
int g_scale = 1;
uint32_t g_batches = 0;
uint32_t g_batch_vertices = 0;
double g_batch_seconds = 0.0;

int scaled(int value) {
    return value * g_scale;
}

bool compile(const char* source, const char* entry, const char* profile, ID3DBlob** blob) {
    ID3DBlob* errors = nullptr;
    HRESULT result = D3DCompile(source, std::strlen(source), "gx", nullptr, nullptr, entry, profile, 0, 0, blob, &errors);
    if (FAILED(result)) {
        if (errors) {
            std::fprintf(stderr, "shader compile error: %s\n", static_cast<const char*>(errors->GetBufferPointer()));
            errors->Release();
        }
        return false;
    }
    release(errors);
    return true;
}

bool create_target() {
    D3D11_TEXTURE2D_DESC description{};
    description.Width = scaled(kEfbWidth);
    description.Height = scaled(kEfbHeight);
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(g_device.device->CreateTexture2D(&description, nullptr, &g_device.target)) ||
        FAILED(g_device.device->CreateRenderTargetView(g_device.target, nullptr, &g_device.target_view))) {
        return false;
    }
    description.Format = DXGI_FORMAT_D32_FLOAT;
    description.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(g_device.device->CreateTexture2D(&description, nullptr, &g_device.depth)) ||
        FAILED(g_device.device->CreateDepthStencilView(g_device.depth, nullptr, &g_device.depth_view))) {
        return false;
    }
    return true;
}

bool create_pipeline() {
    ID3DBlob* vertex_code = nullptr;
    ID3DBlob* pixel_code = nullptr;
    if (!compile(kShaderSource, "vertex_main", "vs_5_0", &vertex_code) || !compile(kShaderSource, "pixel_main", "ps_5_0", &pixel_code)) {
        return false;
    }
    bool ok = SUCCEEDED(g_device.device->CreateVertexShader(vertex_code->GetBufferPointer(), vertex_code->GetBufferSize(), nullptr, &g_device.vertex_shader)) &&
              SUCCEEDED(g_device.device->CreatePixelShader(pixel_code->GetBufferPointer(), pixel_code->GetBufferSize(), nullptr, &g_device.pixel_shader));
    if (ok) {
        std::vector<D3D11_INPUT_ELEMENT_DESC> elements = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        for (UINT i = 0; i < 8; i++) {
            elements.push_back({"TEXCOORD", i, DXGI_FORMAT_R32G32_FLOAT, 0, 44 + 8 * i, D3D11_INPUT_PER_VERTEX_DATA, 0});
        }
        ok = SUCCEEDED(g_device.device->CreateInputLayout(elements.data(), static_cast<UINT>(elements.size()), vertex_code->GetBufferPointer(),
                                                         vertex_code->GetBufferSize(), &g_device.layout));
    }
    release(vertex_code);
    release(pixel_code);
    if (!ok) {
        return false;
    }
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = kVertexCapacity * sizeof(ScreenVertex);
    buffer.Usage = D3D11_USAGE_DYNAMIC;
    buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    buffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_device.device->CreateBuffer(&buffer, nullptr, &g_device.vertex_buffer))) {
        return false;
    }
    buffer.ByteWidth = sizeof(Constants);
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(g_device.device->CreateBuffer(&buffer, nullptr, &g_device.constants))) {
        return false;
    }
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.ScissorEnable = TRUE;
    raster.DepthClipEnable = FALSE;
    return SUCCEEDED(g_device.device->CreateRasterizerState(&raster, &g_device.rasterizer));
}

bool initialize() {
    if (g_device.ready) {
        return true;
    }
    if (g_device.failed) {
        return false;
    }
    const char* scale_text = std::getenv("WP_SCALE");
    g_scale = scale_text ? std::clamp(std::atoi(scale_text), 1, 6) : 3;
    D3D_FEATURE_LEVEL level;
    HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &g_device.device, &level,
                                       &g_device.context);
    if (FAILED(result)) {
        result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &g_device.device, &level,
                                   &g_device.context);
    }
    if (FAILED(result) || !create_target() || !create_pipeline()) {
        std::fputs("cannot initialise the D3D11 renderer\n", stderr);
        g_device.failed = true;
        return false;
    }
    g_device.ready = true;
    return true;
}

uint16_t be16(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    return r | (g << 8) | (b << 16) | (a << 24);
}

uint32_t expand5(uint32_t v) {
    return (v << 3) | (v >> 2);
}

uint32_t expand6(uint32_t v) {
    return (v << 2) | (v >> 4);
}

uint32_t expand4(uint32_t v) {
    return v * 17;
}

uint32_t expand3(uint32_t v) {
    return (v << 5) | (v << 2) | (v >> 1);
}

uint32_t rgb565(uint16_t v) {
    return rgba(expand5((v >> 11) & 31), expand6((v >> 5) & 63), expand5(v & 31), 255);
}

uint32_t rgb5a3(uint16_t v) {
    if (v & 0x8000) {
        return rgba(expand5((v >> 10) & 31), expand5((v >> 5) & 31), expand5(v & 31), 255);
    }
    return rgba(expand4((v >> 8) & 15), expand4((v >> 4) & 15), expand4(v & 15), expand3((v >> 12) & 7));
}

struct Layout {
    uint32_t block_width;
    uint32_t block_height;
    bool supported;
};

Layout layout_for(uint32_t format) {
    switch (format) {
    case 0:
    case 8:
        return {8, 8, true};
    case 1:
    case 2:
    case 9:
        return {8, 4, true};
    case 3:
    case 4:
    case 5:
    case 6:
    case 10:
        return {4, 4, true};
    case 14:
        return {8, 8, true};
    default:
        return {4, 4, false};
    }
}

uint32_t block_bytes(uint32_t format) {
    return format == 6 ? 64 : 32;
}

void decode_cmpr_block(const uint8_t* source, uint32_t* out, uint32_t stride) {
    uint16_t c0 = be16(source);
    uint16_t c1 = be16(source + 2);
    uint32_t palette[4];
    auto split = [](uint16_t c, uint32_t& r, uint32_t& g, uint32_t& b) {
        r = expand5((c >> 11) & 31);
        g = expand6((c >> 5) & 63);
        b = expand5(c & 31);
    };
    uint32_t r0, g0, b0, r1, g1, b1;
    split(c0, r0, g0, b0);
    split(c1, r1, g1, b1);
    palette[0] = rgba(r0, g0, b0, 255);
    palette[1] = rgba(r1, g1, b1, 255);
    if (c0 > c1) {
        palette[2] = rgba((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
        palette[3] = rgba((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
    } else {
        palette[2] = rgba((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
        palette[3] = rgba(0, 0, 0, 0);
    }
    for (uint32_t row = 0; row < 4; row++) {
        uint8_t bits = source[4 + row];
        for (uint32_t column = 0; column < 4; column++) {
            out[row * stride + column] = palette[(bits >> (6 - 2 * column)) & 3];
        }
    }
}

uint32_t palette_entry(const uint8_t* tlut, uint32_t tlut_format, uint32_t index) {
    uint16_t entry = be16(tlut + index * 2);
    switch (tlut_format) {
    case 0:
        return rgba(entry & 0xFF, entry & 0xFF, entry & 0xFF, entry >> 8);
    case 1:
        return rgb565(entry);
    default:
        return rgb5a3(entry);
    }
}

std::vector<uint32_t> decode_texture(const uint8_t* data, uint32_t width, uint32_t height, uint32_t format, const uint8_t* tlut,
                                     uint32_t tlut_format) {
    Layout layout = layout_for(format);
    uint32_t padded_width = (width + layout.block_width - 1) / layout.block_width * layout.block_width;
    uint32_t padded_height = (height + layout.block_height - 1) / layout.block_height * layout.block_height;
    std::vector<uint32_t> pixels(static_cast<size_t>(padded_width) * padded_height, rgba(255, 0, 255, 255));
    if (!layout.supported) {
        return pixels;
    }
    uint32_t bytes = block_bytes(format);
    for (uint32_t by = 0; by < padded_height; by += layout.block_height) {
        for (uint32_t bx = 0; bx < padded_width; bx += layout.block_width) {
            uint32_t* out = &pixels[static_cast<size_t>(by) * padded_width + bx];
            const uint8_t* block = data;
            data += bytes;
            for (uint32_t y = 0; y < layout.block_height; y++) {
                for (uint32_t x = 0; x < layout.block_width; x++) {
                    uint32_t pixel = 0;
                    switch (format) {
                    case 0: {
                        uint32_t v = (block[y * 4 + x / 2] >> ((x & 1) ? 0 : 4)) & 15;
                        pixel = rgba(expand4(v), expand4(v), expand4(v), expand4(v));
                        break;
                    }
                    case 1: {
                        uint32_t v = block[y * 8 + x];
                        pixel = rgba(v, v, v, v);
                        break;
                    }
                    case 2: {
                        uint32_t v = block[y * 8 + x];
                        uint32_t i = expand4(v & 15);
                        pixel = rgba(i, i, i, expand4(v >> 4));
                        break;
                    }
                    case 3: {
                        uint32_t a = block[(y * 4 + x) * 2];
                        uint32_t i = block[(y * 4 + x) * 2 + 1];
                        pixel = rgba(i, i, i, a);
                        break;
                    }
                    case 4:
                        pixel = rgb565(be16(block + (y * 4 + x) * 2));
                        break;
                    case 5:
                        pixel = rgb5a3(be16(block + (y * 4 + x) * 2));
                        break;
                    case 6: {
                        uint32_t index = (y * 4 + x) * 2;
                        pixel = rgba(block[32 + index], block[32 + index + 1], block[index + 1], block[index]);
                        break;
                    }
                    case 8:
                        pixel = palette_entry(tlut, tlut_format, (block[y * 4 + x / 2] >> ((x & 1) ? 0 : 4)) & 15);
                        break;
                    case 9:
                        pixel = palette_entry(tlut, tlut_format, block[y * 8 + x]);
                        break;
                    case 10:
                        pixel = palette_entry(tlut, tlut_format, be16(block + (y * 4 + x) * 2) & 0x3FFF);
                        break;
                    case 14: {
                        uint32_t sub = (y / 4) * 2 + (x / 4);
                        uint32_t local[16];
                        decode_cmpr_block(block + sub * 8, local, 4);
                        pixel = local[(y % 4) * 4 + (x % 4)];
                        break;
                    }
                    default:
                        break;
                    }
                    out[y * padded_width + x] = pixel;
                }
            }
        }
    }
    std::vector<uint32_t> cropped(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; y++) {
        std::memcpy(&cropped[static_cast<size_t>(y) * width], &pixels[static_cast<size_t>(y) * padded_width], width * sizeof(uint32_t));
    }
    return cropped;
}

uint64_t hash_bytes(const uint8_t* data, size_t size) {
    uint64_t hash = 1469598103934665603ull;
    size_t words = size / 8;
    const uint64_t* p = reinterpret_cast<const uint64_t*>(data);
    for (size_t i = 0; i < words; i++) {
        hash = (hash ^ p[i]) * 1099511628211ull;
    }
    for (size_t i = words * 8; i < size; i++) {
        hash = (hash ^ data[i]) * 1099511628211ull;
    }
    return hash;
}

uint64_t sample_hash(const uint8_t* data, size_t size) {
    constexpr size_t kSamples = 32;
    uint64_t hash = 1469598103934665603ull;
    size_t step = size / kSamples;
    for (size_t i = 0; i < kSamples; i++) {
        uint64_t word = 0;
        std::memcpy(&word, data + std::min(i * step, size > 8 ? size - 8 : 0), std::min<size_t>(size, 8));
        hash = (hash ^ word) * 1099511628211ull;
    }
    return hash;
}

ID3D11ShaderResourceView* texture_for(uint32_t map) {
    const uint32_t* bp = bp_registers();
    uint32_t image0_reg = map < 4 ? 0x88 + map : 0xA8 + (map - 4);
    uint32_t image3_reg = map < 4 ? 0x94 + map : 0xB4 + (map - 4);
    uint32_t image0 = bp[image0_reg];
    uint32_t width = (image0 & 0x3FF) + 1;
    uint32_t height = ((image0 >> 10) & 0x3FF) + 1;
    uint32_t format = (image0 >> 20) & 15;
    uint32_t address = 0x80000000u | ((bp[image3_reg] & 0x00FFFFFF) << 5);
    Layout layout = layout_for(format);
    if (!layout.supported && !g_logged_palette) {
        g_logged_palette = true;
        std::fprintf(stderr, "unsupported texture format %u\n", format);
    }
    uint32_t padded_width = (width + layout.block_width - 1) / layout.block_width * layout.block_width;
    uint32_t padded_height = (height + layout.block_height - 1) / layout.block_height * layout.block_height;
    size_t size = static_cast<size_t>(padded_width / layout.block_width) * (padded_height / layout.block_height) * block_bytes(format);
    if (!guest_range_valid(address, size)) {
        return nullptr;
    }
    const uint8_t* source = host(address);
    auto copied = g_device.copies.find(address);
    if (copied != g_device.copies.end() && copied->second.logical_width == width && copied->second.logical_height == height &&
        (format == 4 || format == 5 || format == 6) && copied->second.guest_hash == sample_hash(source, copied->second.bytes)) {
        return copied->second.view;
    }
    uint64_t key = (static_cast<uint64_t>(address) << 20) ^ (static_cast<uint64_t>(width) << 8) ^ (static_cast<uint64_t>(height) << 32) ^ format;
    const uint8_t* tlut = nullptr;
    uint32_t tlut_format = 0;
    uint32_t entries = 0;
    if (format >= 8 && format <= 10) {
        uint32_t tlut_register = bp[map < 4 ? 0x98 + map : 0xB8 + (map - 4)];
        uint32_t tlut_offset = (tlut_register & 0x3FF) << 9;
        tlut_format = (tlut_register >> 10) & 3;
        tlut = g_tlut + (tlut_offset & kTlutMask);
        entries = format == 8 ? 16 : format == 9 ? 256 : 16384;
        key ^= (static_cast<uint64_t>(tlut_offset >> 9) << 54) ^ (static_cast<uint64_t>(tlut_format) << 52);
    }
    CachedTexture& entry = g_device.textures[key];
    if (entry.view && entry.verified_frame == g_frame && entry.width == width && entry.height == height && entry.format == format) {
        return entry.view;
    }
    uint64_t hash = hash_bytes(source, size);
    if (tlut) {
        hash = (hash ^ hash_bytes(tlut, entries * 2)) * 1099511628211ull ^ tlut_format;
    }
    if (entry.view && entry.hash == hash && entry.width == width && entry.height == height && entry.format == format) {
        entry.verified_frame = g_frame;
        return entry.view;
    }
    flush_pending();
    release(entry.view);
    release(entry.texture);
    std::vector<uint32_t> pixels = decode_texture(source, width, height, format, tlut, tlut_format);
    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{pixels.data(), width * 4, 0};
    if (FAILED(g_device.device->CreateTexture2D(&description, &initial, &entry.texture)) ||
        FAILED(g_device.device->CreateShaderResourceView(entry.texture, nullptr, &entry.view))) {
        return nullptr;
    }
    entry.hash = hash;
    entry.width = width;
    entry.height = height;
    entry.format = format;
    entry.verified_frame = g_frame;
    return entry.view;
}

ID3D11SamplerState* sampler_for(uint32_t map) {
    const uint32_t* bp = bp_registers();
    uint32_t mode = bp[map < 4 ? 0x80 + map : 0xA0 + (map - 4)];
    uint32_t key = mode & 0xFF;
    auto found = g_device.samplers.find(key);
    if (found != g_device.samplers.end()) {
        return found->second;
    }
    auto address = [](uint32_t wrap) {
        return wrap == 1 ? D3D11_TEXTURE_ADDRESS_WRAP : wrap == 2 ? D3D11_TEXTURE_ADDRESS_MIRROR : D3D11_TEXTURE_ADDRESS_CLAMP;
    };
    D3D11_SAMPLER_DESC description{};
    bool linear = ((mode >> 4) & 1) != 0 || ((mode >> 5) & 7) >= 4;
    description.Filter = linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    description.AddressU = address(mode & 3);
    description.AddressV = address((mode >> 2) & 3);
    description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState* state = nullptr;
    g_device.device->CreateSamplerState(&description, &state);
    g_device.samplers[key] = state;
    return state;
}

D3D11_BLEND source_factor(uint32_t code) {
    static const D3D11_BLEND kFactors[] = {D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR,
                                           D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA};
    return kFactors[code & 7];
}

D3D11_BLEND destination_factor(uint32_t code) {
    static const D3D11_BLEND kFactors[] = {D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR,
                                           D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA};
    return kFactors[code & 7];
}

D3D11_BLEND alpha_variant(D3D11_BLEND factor) {
    switch (factor) {
    case D3D11_BLEND_SRC_COLOR:
        return D3D11_BLEND_SRC_ALPHA;
    case D3D11_BLEND_INV_SRC_COLOR:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case D3D11_BLEND_DEST_COLOR:
        return D3D11_BLEND_DEST_ALPHA;
    case D3D11_BLEND_INV_DEST_COLOR:
        return D3D11_BLEND_INV_DEST_ALPHA;
    default:
        return factor;
    }
}

ID3D11BlendState* blend_for(uint32_t word) {
    uint32_t key = word & 0xFFFF;
    auto found = g_device.blend_states.find(key);
    if (found != g_device.blend_states.end()) {
        return found->second;
    }
    D3D11_BLEND_DESC description{};
    D3D11_RENDER_TARGET_BLEND_DESC& target = description.RenderTarget[0];
    bool enable = (word & 1) != 0;
    target.BlendEnable = enable;
    target.SrcBlend = source_factor((word >> 8) & 7);
    target.DestBlend = destination_factor((word >> 5) & 7);
    target.BlendOp = (word & (1u << 11)) ? D3D11_BLEND_OP_REV_SUBTRACT : D3D11_BLEND_OP_ADD;
    target.SrcBlendAlpha = alpha_variant(target.SrcBlend);
    target.DestBlendAlpha = alpha_variant(target.DestBlend);
    target.BlendOpAlpha = target.BlendOp;
    UINT mask = 0;
    if (word & (1u << 3)) {
        mask |= D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
    }
    if (word & (1u << 4)) {
        mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;
    }
    target.RenderTargetWriteMask = static_cast<UINT8>(mask);
    ID3D11BlendState* state = nullptr;
    g_device.device->CreateBlendState(&description, &state);
    g_device.blend_states[key] = state;
    return state;
}

ID3D11DepthStencilState* depth_for(uint32_t word) {
    uint32_t key = word & 0x1F;
    auto found = g_device.depth_states.find(key);
    if (found != g_device.depth_states.end()) {
        return found->second;
    }
    D3D11_DEPTH_STENCIL_DESC description{};
    description.DepthEnable = (word & 1) != 0;
    description.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(((word >> 1) & 7) + 1);
    description.DepthWriteMask = (word & (1u << 4)) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    ID3D11DepthStencilState* state = nullptr;
    g_device.device->CreateDepthStencilState(&description, &state);
    g_device.depth_states[key] = state;
    return state;
}

float signed11(uint32_t value) {
    int32_t v = static_cast<int32_t>(value & 0x7FF);
    if (v & 0x400) {
        v -= 0x800;
    }
    return static_cast<float>(v) / 255.0f;
}

void fill_constants(Constants& constants) {
    const uint32_t* bp = bp_registers();
    std::memset(&constants, 0, sizeof constants);
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t ra = bp[0xE0 + 2 * i];
        uint32_t bg = bp[0xE1 + 2 * i];
        constants.initial[i][0] = signed11(ra);
        constants.initial[i][3] = signed11(ra >> 12);
        constants.initial[i][2] = signed11(bg);
        constants.initial[i][1] = signed11(bg >> 12);
    }
    const uint32_t* konst = konst_registers();
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t ra = konst[2 * i];
        uint32_t bg = konst[2 * i + 1];
        constants.konst[i][0] = signed11(ra);
        constants.konst[i][3] = signed11(ra >> 12);
        constants.konst[i][2] = signed11(bg);
        constants.konst[i][1] = signed11(bg >> 12);
    }
    uint32_t stages = ((bp[0x00] >> 10) & 15) + 1;
    constants.header[0] = stages;
    constants.header[1] = bp[0xF3];
    for (uint32_t table = 0; table < 4; table++) {
        uint32_t low = bp[0xF6 + 2 * table];
        uint32_t high = bp[0xF7 + 2 * table];
        constants.swaps[table] = (low & 3) | (((low >> 2) & 3) << 2) | ((high & 3) << 4) | (((high >> 2) & 3) << 6);
    }
    for (uint32_t i = 0; i < stages && i < kMaxStages; i++) {
        uint32_t order_word = bp[0x28 + i / 2];
        uint32_t order = (i & 1) ? (order_word >> 12) : order_word;
        uint32_t map = order & 7;
        uint32_t coord = (order >> 3) & 7;
        uint32_t enable = (order >> 6) & 1;
        uint32_t channel = (order >> 7) & 7;
        uint32_t ksel_word = bp[0xF6 + i / 2];
        uint32_t ksel = (i & 1) ? ((ksel_word >> 14) & 0x3FF) : ((ksel_word >> 4) & 0x3FF);
        constants.stage[i][0] = bp[0xC0 + 2 * i];
        constants.stage[i][1] = bp[0xC1 + 2 * i];
        constants.stage[i][2] = map | (coord << 3) | (enable << 6) | (channel << 7);
        constants.stage[i][3] = (ksel & 31) | (((ksel >> 5) & 31) << 5);
    }
}

void flush_pending() {
    PendingBatch& pending = g_pending;
    if (pending.vertices.empty()) {
        return;
    }
    auto started = std::chrono::steady_clock::now();
    g_batches++;
    g_batch_vertices += static_cast<uint32_t>(pending.vertices.size());
    ID3D11DeviceContext* context = g_device.context;
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(context->Map(g_device.constants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        std::memcpy(mapped.pData, &pending.constants, sizeof pending.constants);
        context->Unmap(g_device.constants, 0);
    }
    context->PSSetShaderResources(0, kTextureMaps, pending.views);
    context->PSSetSamplers(0, kTextureMaps, pending.samplers);
    D3D11_VIEWPORT viewport{0, 0, static_cast<float>(scaled(kEfbWidth)), static_cast<float>(scaled(kEfbHeight)), 0.0f, 1.0f};
    context->RSSetViewports(1, &viewport);
    D3D11_RECT scissor;
    scissor.left = static_cast<LONG>(((pending.top_left >> 12) & 0x7FF) - kScissorOffset);
    scissor.top = static_cast<LONG>((pending.top_left & 0x7FF) - kScissorOffset);
    scissor.right = static_cast<LONG>(((pending.bottom_right >> 12) & 0x7FF) - kScissorOffset + 1);
    scissor.bottom = static_cast<LONG>((pending.bottom_right & 0x7FF) - kScissorOffset + 1);
    scissor.left = std::max<LONG>(scissor.left, 0);
    scissor.top = std::max<LONG>(scissor.top, 0);
    scissor.right = std::min<LONG>(scissor.right, kEfbWidth);
    scissor.bottom = std::min<LONG>(scissor.bottom, kEfbHeight);
    scissor.left *= g_scale;
    scissor.top *= g_scale;
    scissor.right *= g_scale;
    scissor.bottom *= g_scale;
    context->RSSetScissorRects(1, &scissor);
    context->RSSetState(g_device.rasterizer);
    context->OMSetRenderTargets(1, &g_device.target_view, g_device.depth_view);
    float blend_factor[4] = {1, 1, 1, 1};
    context->OMSetBlendState(blend_for(pending.blend), blend_factor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(depth_for(pending.depth), 0);
    UINT stride = sizeof(ScreenVertex);
    UINT zero = 0;
    context->IASetVertexBuffers(0, 1, &g_device.vertex_buffer, &stride, &zero);
    context->IASetInputLayout(g_device.layout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(g_device.vertex_shader, nullptr, 0);
    context->PSSetShader(g_device.pixel_shader, nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &g_device.constants);
    size_t total = pending.vertices.size();
    size_t offset = 0;
    while (offset < total) {
        uint32_t batch = static_cast<uint32_t>(std::min<size_t>(total - offset, kVertexCapacity));
        if (FAILED(context->Map(g_device.vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            break;
        }
        std::memcpy(mapped.pData, pending.vertices.data() + offset, batch * sizeof(ScreenVertex));
        context->Unmap(g_device.vertex_buffer, 0);
        context->Draw(batch, 0);
        offset += batch;
    }
    pending.vertices.clear();
    g_batch_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}

}

const char* api_name() {
    return "Direct3D 11";
}

void draw(const ScreenVertex* vertices, uint32_t count) {
    if (count == 0 || !initialize()) {
        return;
    }
    const uint32_t* bp = bp_registers();
    Constants constants;
    fill_constants(constants);
    ID3D11ShaderResourceView* views[kTextureMaps] = {};
    ID3D11SamplerState* samplers[kTextureMaps] = {};
    uint32_t used_maps = 0;
    for (uint32_t i = 0; i < constants.header[0] && i < kMaxStages; i++) {
        if (constants.stage[i][2] & 0x40) {
            used_maps |= 1u << (constants.stage[i][2] & 7);
        }
    }
    for (uint32_t map = 0; map < kTextureMaps; map++) {
        if (used_maps & (1u << map)) {
            views[map] = texture_for(map);
            samplers[map] = sampler_for(map);
        }
    }
    uint32_t blend = bp[0x41] & 0xFFFF;
    uint32_t depth = bp[0x40] & 0x1F;
    PendingBatch& pending = g_pending;
    bool same = !pending.vertices.empty() && pending.blend == blend && pending.depth == depth && pending.top_left == bp[0x20] &&
                pending.bottom_right == bp[0x21] && std::memcmp(&pending.constants, &constants, sizeof constants) == 0 &&
                std::memcmp(pending.views, views, sizeof views) == 0 && std::memcmp(pending.samplers, samplers, sizeof samplers) == 0;
    if (!same) {
        flush_pending();
        pending.constants = constants;
        std::memcpy(pending.views, views, sizeof views);
        std::memcpy(pending.samplers, samplers, sizeof samplers);
        pending.blend = blend;
        pending.depth = depth;
        pending.top_left = bp[0x20];
        pending.bottom_right = bp[0x21];
    }
    pending.vertices.insert(pending.vertices.end(), vertices, vertices + count);
}

void take_statistics(uint32_t& batches, uint32_t& vertices, double& seconds) {
    batches = g_batches;
    vertices = g_batch_vertices;
    seconds = g_batch_seconds;
    g_batches = 0;
    g_batch_vertices = 0;
    g_batch_seconds = 0.0;
}

bool guest_range_valid(uint32_t address, size_t size) {
    return static_cast<size_t>(address & kAddressMask) + size <= kPhysicalSize;
}

void load_tlut(uint32_t address, uint32_t tmem_offset, uint32_t bytes) {
    tmem_offset &= kTlutMask;
    bytes = std::min(bytes, kTlutSize - tmem_offset);
    if (!guest_range_valid(address, bytes)) {
        return;
    }
    std::memcpy(g_tlut + tmem_offset, host(address), bytes);
}

void copy_to_texture(uint32_t address, int x, int y, int width, int height, bool half, uint32_t format) {
    if (!initialize()) {
        return;
    }
    if (format != 4 && format != 5 && format != 6) {
        if (!g_logged_copy_format) {
            g_logged_copy_format = true;
            std::fprintf(stderr, "unsupported EFB copy format %u\n", format);
        }
        g_device.copies.erase(address);
        return;
    }
    x = std::max(0, x);
    y = std::max(0, y);
    width = std::min(width, kEfbWidth - x);
    height = std::min(height, kEfbHeight - y);
    if (width <= 0 || height <= 0) {
        return;
    }
    flush_pending();
    CopiedTexture& entry = g_device.copies[address];
    uint32_t w = static_cast<uint32_t>(width);
    uint32_t h = static_cast<uint32_t>(height);
    uint32_t physical_width = w * g_scale;
    uint32_t physical_height = h * g_scale;
    if (!entry.texture || entry.width != physical_width || entry.height != physical_height) {
        release(entry.view);
        release(entry.texture);
        D3D11_TEXTURE2D_DESC description{};
        description.Width = physical_width;
        description.Height = physical_height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g_device.device->CreateTexture2D(&description, nullptr, &entry.texture)) ||
            FAILED(g_device.device->CreateShaderResourceView(entry.texture, nullptr, &entry.view))) {
            release(entry.view);
            release(entry.texture);
            g_device.copies.erase(address);
            return;
        }
        entry.width = physical_width;
        entry.height = physical_height;
    }
    D3D11_BOX box{static_cast<UINT>(scaled(x)), static_cast<UINT>(scaled(y)), 0, static_cast<UINT>(scaled(x + width)), static_cast<UINT>(scaled(y + height)), 1};
    g_device.context->CopySubresourceRegion(entry.texture, 0, 0, 0, 0, g_device.target, 0, &box);
    entry.logical_width = half ? std::max(1u, w / 2) : w;
    entry.logical_height = half ? std::max(1u, h / 2) : h;
    entry.bytes = entry.logical_width * entry.logical_height * (format == 6 ? 4 : 2);
    entry.guest_hash = guest_range_valid(address, entry.bytes) ? sample_hash(host(address), entry.bytes) : 0;
}

void copy_to_framebuffer(int x, int y, int width, int height) {
    if (!initialize()) {
        return;
    }
    flush_pending();
    g_frame++;
    x = std::max(0, x);
    y = std::max(0, y);
    width = std::min(width, kEfbWidth - x);
    height = std::min(height, kEfbHeight - y);
    if (width <= 0 || height <= 0) {
        return;
    }
    uint32_t physical_width = static_cast<uint32_t>(scaled(width));
    uint32_t physical_height = static_cast<uint32_t>(scaled(height));
    if (!g_device.frame || g_device.frame_width != physical_width || g_device.frame_height != physical_height) {
        release(g_device.frame_view);
        release(g_device.frame);
        D3D11_TEXTURE2D_DESC description{};
        description.Width = physical_width;
        description.Height = physical_height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g_device.device->CreateTexture2D(&description, nullptr, &g_device.frame)) ||
            FAILED(g_device.device->CreateShaderResourceView(g_device.frame, nullptr, &g_device.frame_view))) {
            release(g_device.frame_view);
            release(g_device.frame);
            return;
        }
        g_device.frame_width = physical_width;
        g_device.frame_height = physical_height;
    }
    D3D11_BOX box{static_cast<UINT>(scaled(x)), static_cast<UINT>(scaled(y)), 0, static_cast<UINT>(scaled(x + width)),
                  static_cast<UINT>(scaled(y + height)), 1};
    g_device.context->CopySubresourceRegion(g_device.frame, 0, 0, 0, 0, g_device.target, 0, &box);
}

bool create_present_pipeline() {
    ID3DBlob* vertex_code = nullptr;
    ID3DBlob* pixel_code = nullptr;
    if (!compile(kPresentShaderSource, "vertex_main", "vs_5_0", &vertex_code) || !compile(kPresentShaderSource, "pixel_main", "ps_5_0", &pixel_code)) {
        return false;
    }
    bool ok = SUCCEEDED(g_device.device->CreateVertexShader(vertex_code->GetBufferPointer(), vertex_code->GetBufferSize(), nullptr, &g_device.present_vertex)) &&
              SUCCEEDED(g_device.device->CreatePixelShader(pixel_code->GetBufferPointer(), pixel_code->GetBufferSize(), nullptr, &g_device.present_pixel));
    release(vertex_code);
    release(pixel_code);
    if (!ok) {
        return false;
    }
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = FALSE;
    return SUCCEEDED(g_device.device->CreateSamplerState(&sampler, &g_device.present_sampler)) &&
           SUCCEEDED(g_device.device->CreateRasterizerState(&raster, &g_device.present_rasterizer));
}

bool ensure_swapchain(HWND window, UINT width, UINT height) {
    if (!g_device.swapchain) {
        IDXGIDevice* dxgi_device = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory2* factory = nullptr;
        bool ok = SUCCEEDED(g_device.device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi_device))) &&
                  SUCCEEDED(dxgi_device->GetAdapter(&adapter)) &&
                  SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&factory)));
        if (ok) {
            DXGI_SWAP_CHAIN_DESC1 description{};
            description.Width = width;
            description.Height = height;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            description.BufferCount = 2;
            description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            ok = SUCCEEDED(factory->CreateSwapChainForHwnd(g_device.device, window, &description, nullptr, nullptr, &g_device.swapchain));
            if (ok) {
                factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
            }
        }
        release(factory);
        release(adapter);
        release(dxgi_device);
        if (!ok || !create_present_pipeline()) {
            release(g_device.swapchain);
            g_device.failed = true;
            return false;
        }
        g_device.swapchain_width = width;
        g_device.swapchain_height = height;
    } else if (width != g_device.swapchain_width || height != g_device.swapchain_height) {
        release(g_device.backbuffer_view);
        if (FAILED(g_device.swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) {
            return false;
        }
        g_device.swapchain_width = width;
        g_device.swapchain_height = height;
    }
    if (!g_device.backbuffer_view) {
        ID3D11Texture2D* buffer = nullptr;
        if (FAILED(g_device.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&buffer)))) {
            return false;
        }
        HRESULT result = g_device.device->CreateRenderTargetView(buffer, nullptr, &g_device.backbuffer_view);
        release(buffer);
        return SUCCEEDED(result);
    }
    return true;
}

bool present_frame(void* window_handle, double aspect) {
    if (!g_device.ready || !g_device.frame_view || !window_handle) {
        return false;
    }
    HWND window = static_cast<HWND>(window_handle);
    RECT client;
    GetClientRect(window, &client);
    if (client.right <= 0 || client.bottom <= 0 || !ensure_swapchain(window, static_cast<UINT>(client.right), static_cast<UINT>(client.bottom))) {
        return false;
    }
    flush_pending();
    ID3D11DeviceContext* context = g_device.context;
    float black[4] = {0, 0, 0, 1};
    context->OMSetRenderTargets(1, &g_device.backbuffer_view, nullptr);
    context->ClearRenderTargetView(g_device.backbuffer_view, black);
    double width = client.right;
    double height = width / aspect;
    if (height > client.bottom) {
        height = client.bottom;
        width = height * aspect;
    }
    D3D11_VIEWPORT viewport{static_cast<float>((client.right - width) / 2), static_cast<float>((client.bottom - height) / 2),
                            static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    context->RSSetViewports(1, &viewport);
    context->RSSetState(g_device.present_rasterizer);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(g_device.present_vertex, nullptr, 0);
    context->PSSetShader(g_device.present_pixel, nullptr, 0);
    context->PSSetShaderResources(0, 1, &g_device.frame_view);
    context->PSSetSamplers(0, 1, &g_device.present_sampler);
    context->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr;
    context->PSSetShaderResources(0, 1, &none);
    g_device.swapchain->Present(0, 0);
    return true;
}

bool read_frame(std::vector<uint32_t>& pixels, uint32_t& width, uint32_t& height) {
    if (!g_device.ready || !g_device.frame) {
        return false;
    }
    flush_pending();
    D3D11_TEXTURE2D_DESC description{};
    g_device.frame->GetDesc(&description);
    description.BindFlags = 0;
    description.Usage = D3D11_USAGE_STAGING;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging = nullptr;
    if (FAILED(g_device.device->CreateTexture2D(&description, nullptr, &staging))) {
        return false;
    }
    g_device.context->CopyResource(staging, g_device.frame);
    D3D11_MAPPED_SUBRESOURCE mapped;
    bool ok = SUCCEEDED(g_device.context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped));
    if (ok) {
        width = description.Width;
        height = description.Height;
        pixels.resize(static_cast<size_t>(width) * height);
        for (uint32_t row = 0; row < height; row++) {
            const uint8_t* source = static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch;
            for (uint32_t column = 0; column < width; column++) {
                pixels[static_cast<size_t>(row) * width + column] = (static_cast<uint32_t>(source[column * 4]) << 16) |
                                                                    (static_cast<uint32_t>(source[column * 4 + 1]) << 8) | source[column * 4 + 2];
            }
        }
        g_device.context->Unmap(staging, 0);
    }
    release(staging);
    return ok;
}

void clear() {
    if (!initialize()) {
        return;
    }
    flush_pending();
    const uint32_t* bp = bp_registers();
    float color[4] = {(bp[0x4F] & 0xFF) / 255.0f, ((bp[0x50] >> 8) & 0xFF) / 255.0f, (bp[0x50] & 0xFF) / 255.0f, ((bp[0x4F] >> 8) & 0xFF) / 255.0f};
    g_device.context->ClearRenderTargetView(g_device.target_view, color);
    g_device.context->ClearDepthStencilView(g_device.depth_view, D3D11_CLEAR_DEPTH, static_cast<float>(bp[0x51] & 0xFFFFFF) / 16777215.0f, 0);
}

}

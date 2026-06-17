[[vk::binding(0, 0)]] Texture2D<float> YTex         : register(t0);
[[vk::binding(1, 0)]] Texture2D<float> UTex         : register(t1);
[[vk::binding(2, 0)]] Texture2D<float> VTex         : register(t2);
[[vk::binding(3, 0)]] SamplerState     LinearSampler : register(s3);

struct PSInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float y  = (YTex.Sample(LinearSampler, input.texcoord) * 255.0f - 16.0f)  / 219.0f;
    float cb = (UTex.Sample(LinearSampler, input.texcoord) * 255.0f - 128.0f) / 224.0f;
    float cr = (VTex.Sample(LinearSampler, input.texcoord) * 255.0f - 128.0f) / 224.0f;

    float r = saturate(y + 1.40200f * cr);
    float g = saturate(y - 0.34414f * cb - 0.71414f * cr);
    float b = saturate(y + 1.77200f * cb);

    return float4(r, g, b, 1.0f);
}
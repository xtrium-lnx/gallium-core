struct PushConstants
{
    float4x4* joints;

    float3*   inPositions;
    float3*   inNormals;
    float2*   inUVs;
    float3*   inTangents;
    uint4*    inJoints;
    float4*   inWeights;

    float3*   outPositions;
    float3*   outNormals;
    float2*   outUVs;
    float3*   outTangents;

    uint      vertexCount;
};
[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

struct Skinned
{
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
};

[shader("compute")]
[numthreads(64, 1, 1)]
void cs_main(uint3 tid: SV_DispatchThreadID)
{
    uint i = tid.x;
    if (i >= pc.vertexCount)
        return;

    float4x4 skinMatrix = pc.inWeights[i].x * pc.joints[pc.inJoints[i].x]
                        + pc.inWeights[i].y * pc.joints[pc.inJoints[i].y]
                        + pc.inWeights[i].z * pc.joints[pc.inJoints[i].z]
                        + pc.inWeights[i].w * pc.joints[pc.inJoints[i].w];

    pc.outPositions[i] = mul(skinMatrix, float4(pc.inPositions[i], 1.0f)).xyz;

    if (pc.inNormals != nullptr)
        pc.outNormals[i] = mul(skinMatrix, float4(pc.inNormals[i],   0.0f)).xyz;

    if (pc.inTangents != nullptr)
        pc.outTangents[i] = mul(skinMatrix, float4(pc.inTangents[i], 0.0f)).xyz;

    if (pc.inUVs != nullptr)
        pc.outUVs[i] = pc.inUVs[i];
}

// Shader constants
struct Constants {
    float3x3 worldViewProj;
    float4 color;
    bool hasTexture;
	bool hasMesh;
    float z;
};
ConstantBuffer<Constants> constants : register(b0);


// Texture inputs
Texture2D colorTexture : register(t0);
SamplerState linearSampler : register(s0);


// Shaders
void VSMain(float2 posL : POSITION,
			float2 texCoord : TEXCOORD0,
			uint vertexId : SV_VertexID,
			out float4 posHOut : SV_Position,
			out float2 texCoordOut : TEXCOORD0)
{
    if (constants.hasMesh) {
		float3 posH = mul(float3(posL, 1), constants.worldViewProj);
		posHOut = float4(posH.xy, constants.z * posH.z, posH.z);
		texCoordOut = texCoord;
    }
    else {
		// Triangle strip based on vertex id
		// 3-----2
		// |   / |
		// | /   |
		// 1-----0
		// 0: (1, 0)
		// 1: (0, 0)
		// 2: (1, 1)
		// 3: (0, 1)
	
		texCoordOut.x = (vertexId & 1) ^ 1; // 1 if bit0 is 0.
		texCoordOut.y = vertexId >> 1; // 1 if bit1 is 1.

        float2 posL = (texCoordOut.xy * 2.0f - float2(1, 1)) * 0.5f;

        float3 posH = mul(float3(posL, 1), constants.worldViewProj);
        posHOut = float4(posH.xy, constants.z * posH.z, posH.z);
    }
}


float4 PSMain(float4 posS : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float4 texColor;
    if (constants.hasTexture) {
        texColor = colorTexture.Sample(linearSampler, texCoord);
        return texColor * constants.color;
    }
    else {
		return constants.color;
    }
}
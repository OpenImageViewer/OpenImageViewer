//uniform float4x4 worldViewProj;

struct ShaderIn
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD;
};

struct ShaderOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};


void main(in ShaderIn input, out ShaderOut output)
{
    output.pos = float4(input.pos.xy,0,1);// mul(worldViewProj, input.pos);
	output.uv = input.uv;
}
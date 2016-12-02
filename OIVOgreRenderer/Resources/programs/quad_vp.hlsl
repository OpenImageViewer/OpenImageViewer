//uniform float4x4 worldViewProj;

struct ShaderIn
{
	float2 pos : POSITION;
};

struct ShaderOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};


void main(in ShaderIn input, out ShaderOut output)
{
    output.pos = float4(input.pos,0,1);
    output.uv = float2(input.pos * 0.5 + 0.5);
    output.uv.y = 1.0 - output.uv.y;
}
uniform Texture2D texture_1;
uniform SamplerState textureState_1;
uniform float2 uvScale;
uniform float2 uvOffset;
struct ShaderIn
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct ShaderOut
{
	float4 texelOut : SV_Target;
};



void main(in ShaderIn input, out ShaderOut output)
{
	float4 white = float4(1,1,1,1);
	float4 gray25 = float4(0.75,0.75,0.75,1);
	
	float2 uv = input.uv * uvScale + uvOffset;
	float4 texel = texture_1.Sample(textureState_1, uv);
	float2 checkerSize =  float2(0.03,0.03);
	float2 checkerPos = input.uv / checkerSize;
	int2 checkerPosInt = int2(checkerPos);
	checkerPosInt = checkerPosInt % 2;
	float4 blendColor;
	if (checkerPosInt.x == checkerPosInt.y)
	blendColor = gray25;
	else
	blendColor = white;
	
	texel = lerp(blendColor,texel,texel.w);
	texel.w = 1;
	
	
	output.texelOut = texel;

}
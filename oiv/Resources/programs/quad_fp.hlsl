static const float4 white = float4(1, 1, 1, 1);
static const float4 gray25 = float4(0.75, 0.75, 0.75, 1);
static const float4 black = float4(0, 0, 0, 1);
static const float4 midnightBlue = float4(25 / 255.0, 25 / 255.0 , 112 / 255.0, 1);
static const float4 darkBlue = float4(0 / 255.0, 0 / 255.0, 40 / 255.0, 1);


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

float4 GetChecker(float4 color1, float4 color2, float2 uv)
{
    float2 checkerSize = float2(0.03, 0.03);
    float2 checkerPos = uv / checkerSize;
    int2 checkerPosInt = int2(checkerPos);
    checkerPosInt = checkerPosInt % 2;
    float4 checkerColor;
    if (checkerPosInt.x == checkerPosInt.y)
        checkerColor = color1;
    else
        checkerColor = color2;

    return checkerColor;
}



void main(in ShaderIn input, out ShaderOut output)
{
    float2 uv = input.uv * uvScale + uvOffset;

    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
    {
        output.texelOut = GetChecker(black, darkBlue, input.uv);
    }
    else
    {

        float4 texel = texture_1.Sample(textureState_1, uv);
        float4 checkerColor = GetChecker(white, gray25, input.uv);
        texel = lerp(checkerColor, texel, texel.w);
        texel.w = 1;
        output.texelOut = texel;
    }
}
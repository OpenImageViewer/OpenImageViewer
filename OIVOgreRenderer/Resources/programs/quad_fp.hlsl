#if defined(HLSL) || defined(D3D11)
	#define STATIC_CONST static const
	#define SAMPLER2D Texture2D
	#define SAMPLER_STATE SamplerState
#elif GLSL
	#define STATIC_CONST const
	#define SAMPLER2D sampler2D
	#define SAMPLER_STATE float
	#define float2 vec2 
	#define float3 vec3 
	#define float4 vec4 
	#define int2 ivec2
	#define fmod mod
	#define lerp mix
#endif


STATIC_CONST float4 white = float4(1, 1, 1, 1);
STATIC_CONST float4 gray25 = float4(0.75, 0.75, 0.75, 1);
STATIC_CONST float4 black = float4(0, 0, 0, 1);
STATIC_CONST float4 midnightBlue = float4(25 / 255.0, 25 / 255.0 , 112 / 255.0, 1);
STATIC_CONST float4 darkBlue = float4(0 / 255.0, 0 / 255.0, 40 / 255.0, 1);
STATIC_CONST float4 red = float4(1, 0, 0, 1);

//Globals
uniform SAMPLER2D texture_1;
uniform SAMPLER_STATE texture_1_state;
uniform float2 uvScale;
uniform float2 uvOffset;
uniform float2 uImageSize;
uniform float2 uViewportSize;
uniform int uShowGrid;

float4 SampleTexture(SAMPLER2D i_Tex, float2 coords)
{
#if defined(HLSL) || defined(D3D11)
//TODDO: change texture_1_state to support any sampler state
	return texture_1.Sample(texture_1_state, coords);
#elif GLSL
	return texture(i_Tex, coords);
#endif
	
}
#line 46
float4 GetChecker(float4 color1, float4 color2, float2 uv, float2 viewportSize)
{
    float2 checkerSize = float2(0.03, 0.03);
	//Fix aspect ratio
	if (viewportSize.x > viewportSize.y)
		checkerSize.y *= viewportSize.x / viewportSize.y;
	else
		checkerSize.x *= viewportSize.y / viewportSize.x;

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

void DrawPixelGrid(
					  float2 imageSize
					, float2 screenSize
					, float2 screenUV
					, float2 uvScale
					, float2 uvOffset
					, inout float4 texel)
{
		float2 pixelDrawThreshold = float2(1.5,1.5);
		float2 minLineWidth = float2(1.2,1.2);
		float2 maxLineWidth = float2(5,5);
		
		float2 pixelSize = screenSize / imageSize / uvScale;
		float2 lineWidthInPixels = min(max(minLineWidth,pixelSize / 40.0),maxLineWidth) ;
		
		/*
		float screenAR = screenSize.x / screenSize.y;
		if (screenAR > 1.0)
		{
			lineWidthInPixels.y *=screenAR;
			pixelDrawThreshold.y *= screenAR;
	    }
		else
		{
			lineWidthInPixels.x /=screenAR;
			pixelDrawThreshold.x /= screenAR;
		}
		*/
		
		float2 oneOverPixelSize = 1.0 / pixelSize;
		float2 factor2 = screenSize * oneOverPixelSize;
		float2 offset = float2(0,0);
		offset = fmod( (uvOffset / uvScale) , pixelSize);
		if (pixelSize.x >= pixelDrawThreshold.x && pixelSize.y >= pixelDrawThreshold.y)
		{
			float2 lineWidth = lineWidthInPixels / screenSize;
			float2 modaa = fmod( (screenUV + offset), (1.0 / factor2));
			if (modaa.x < lineWidth.x || modaa.y < lineWidth.y )
				texel = red;
		}
} 

void FillBackGround(float2 uv,float2 screenUV, float2 viewportSize, inout float4 texel)
{
	if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
            texel = GetChecker(black, darkBlue, screenUV, viewportSize);
}
void DrawImage(float2 uv, float2 screenUV,float2 viewportSize, inout float4 texel)
{
    float4 sampledTexel = SampleTexture(texture_1,uv);
    float4 checkerColor = GetChecker(white, gray25, screenUV, viewportSize);
    texel = lerp(checkerColor, sampledTexel, sampledTexel.w);
}

float4 GetFinalTexel(float2 i_inputUV,float2 i_viewportSize, float2 i_imageSize, float2 i_uvScale,float2 i_uvOffset,int i_showGrid )
{
    float4 texel;
    float2 uv = i_inputUV * i_uvScale + i_uvOffset;

    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
        FillBackGround(uv, i_inputUV, i_viewportSize, texel);
    else
    {
        DrawImage(uv, i_inputUV, i_viewportSize, texel);
        if (i_showGrid == 1)
            DrawPixelGrid(i_imageSize, i_viewportSize, i_inputUV, i_uvScale, i_uvOffset, texel);
    }

    texel.w = 1;
    return texel;
}
#if defined(HLSL) || defined(D3D11)
////////////////////////
///DIRECT3D HLSL FRAGMENT SHADER
///////////////////////
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
    output.texelOut = GetFinalTexel(input.uv, uViewportSize, uImageSize,uvScale, uvOffset, uShowGrid);
}
#else
////////////////////////
///OPENGL GLSL FRAGMENT SHADER
///////////////////////
in vec2 coords;
out vec4 outColor;
void main()
{
  outColor = GetFinalTexel(coords, uViewportSize, uImageSize,uvScale, uvOffset, uShowGrid);
}

#endif
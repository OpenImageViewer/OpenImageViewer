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

#include "colorCorrection.shader"
#include "imagecommon.shader"

STATIC_CONST float4 white = float4(1, 1, 1, 1);
STATIC_CONST float4 gray25 = float4(0.75, 0.75, 0.75, 1);
STATIC_CONST float4 black = float4(0, 0, 0, 1);
STATIC_CONST float4 midnightBlue = float4(25 / 255.0, 25 / 255.0 , 112 / 255.0, 1);
STATIC_CONST float4 darkBlue = float4(0 / 255.0, 0 / 255.0, 40 / 255.0, 1);
STATIC_CONST float4 red = float4(1, 0, 0, 1);

//TODO: extract background colours to the CPU.
STATIC_CONST float4 BackgroundColor1  = black;
STATIC_CONST float4 BackgroundColor2 = darkBlue;

//Globals

cbuffer BaseImageData_ : register(b0) 
{
BaseImageData baseImageData;
};

cbuffer MainImageData : register(b1) 
{
	//------------------------
	float4 uTransparencyColor1;
	//------------------------
	float4 uTransparencyColor2;
	//------------------------
	int   uShowGrid;
	float uExposure;
	float uOffset;
	float uGamma;
	//------------------------
	float uSaturation;
	float3 reserved;
};

uniform SAMPLER2D texture_1;
uniform SAMPLER_STATE texture_1_state;


float4 SampleTexture(SAMPLER2D i_Tex, float2 coords)
{
#if defined(HLSL) || defined(D3D11)
//TODDO: change texture_1_state to support any sampler state
	return texture_1.Sample(texture_1_state, coords);
#elif GLSL
	return texture(i_Tex, coords);
#endif
	
}

float4 GetChecker(float4 color1, float4 color2, float2 uv, float2 viewportSize)
{
	static const float2 CheckerSizeInPixels = float2(16, 16);
    float2 checkerSize = CheckerSizeInPixels / viewportSize;
    float2 checkerPos = uv / checkerSize;
    uint2 checkerPosInt = uint2(checkerPos);
    checkerPosInt = checkerPosInt % 2;
    float4 checkerColor;
    if (checkerPosInt.x == checkerPosInt.y)
        checkerColor = color1;
    else
        checkerColor = color2;

    return checkerColor;
}


void DrawPixelGrid2(  in    float2 i_imageSize
				    , in    float2 i_imageOffset
					, in    float2 i_imageScale
					, in    float4 i_viewportSize
					, in    float2 i_inputUV
					, in	float i_originalSampledAlpha
					, inout float4 o_texel)
{
	//Base width of the grid
	const float constantFactor = 2.2;
	//The rate of change in grid width relative to scale - higher value means thinner line sooner.
	const float decayFactor = 0.6;
	//The rate of change in opacity relative to scale - higher value means less opacity sooner.
	const float blendDecay = 2.8;
	// On what scale the grid will be fully opaque
	const float fullOpacityGridScale = 5;

	float minImageScale = min(i_imageScale.x, i_imageScale.y);

	
	float2 pixelOnViewportNorm =  i_inputUV;
	float2 imageOffsetNorm = i_viewportSize.zw * i_imageOffset;
	float aspectRatioFactor = i_viewportSize.z / i_viewportSize.w;
	float2 pixelSizeNorm =  i_viewportSize.zw;
	float2 widthOfGrid =  pixelSizeNorm * (constantFactor /  pow(abs(i_imageScale),decayFactor));
	float2 currentDistance = widthOfGrid / 2.0;
	float2 imageSpacePositionNorm = (pixelOnViewportNorm - imageOffsetNorm) / i_imageScale;
	float2 dd = abs( fmod(imageSpacePositionNorm,  pixelSizeNorm ));
	float2 rdd = abs(pixelSizeNorm - dd);
	float2 ddMin = float2(min(dd.x, rdd.x), min(dd.y, rdd.y));
	float maxDistance = max(currentDistance.x  , currentDistance.y);
	
	//Fixed aspect ratio so width of grid lines would be the same for each axis.
	if (aspectRatioFactor < 1)
		ddMin.x /= aspectRatioFactor;
	else
		ddMin.y *= aspectRatioFactor;
	
	float minDD = min(ddMin.x , ddMin.y);
	
	if ( minDD < maxDistance)
	{
		float minImageScale = min(i_imageScale.x, i_imageScale.y);
		float alpha = (1.0 -  minDD / maxDistance) * min(1, (minImageScale / fullOpacityGridScale));
		float3 inverted = 1.0 - o_texel.rgb;
		float3 gridBlendColor = float3(1,0,0);
		float3 gridMaxBlend = 0.0;
		float3 finalGridColor = lerp(inverted,gridBlendColor,max(1.0 - i_originalSampledAlpha ,gridMaxBlend) );
		o_texel.rgb = lerp(o_texel.rgb, finalGridColor, pow(abs(alpha), blendDecay));
		o_texel.a = 1.0;
	}
	
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
	//if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
            texel = GetChecker(BackgroundColor1, BackgroundColor2, screenUV, viewportSize);
}

void DrawImage(float2 uv
			, in 	float2 screenUV
			, in 	float2 scale
			, in 	float2 viewportSize
			, in	float2 imageSize
			, in	float4 sampledTexel,
			  inout float4 texel)
{
    sampledTexel.xyz = clamp(0, 1.0, pow(abs(sampledTexel.xyz * uExposure  + uOffset), 1.0 / uGamma)) ;
    sampledTexel.xyz = saturate(sampledTexel.xyz, uSaturation);

    float4 checkerColor = GetChecker(uTransparencyColor1, uTransparencyColor2, screenUV, viewportSize);
    texel = lerp(checkerColor, sampledTexel, sampledTexel.w);
}

float4 GetFinalTexel(float2 i_inputUV,float4 i_viewportSize, float2 i_imageSize, float2 i_imageScale,float2 i_ImageOffset,int i_showGrid )
{
	float4 texel;

	float2 uvScale =  i_viewportSize.xy / (i_imageSize.xy * i_imageScale);
	float2 offset=  -i_ImageOffset.xy / i_viewportSize.xy * uvScale;
	float2 uv = i_inputUV * uvScale  + offset;
	
 if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
        FillBackGround(uv, i_inputUV, i_viewportSize.xy, texel);
    else
    {
	float4 sampledTexel = SampleTexture(texture_1,uv);
        DrawImage(uv, i_inputUV,uvScale, i_viewportSize.xy, i_imageSize, sampledTexel, texel);
        //if (i_showGrid == 1)
          //  DrawPixelGrid(i_imageSize, i_viewportSize.xy, i_inputUV, uvScale, offset, texel);
	
	if (i_showGrid == 1)
		DrawPixelGrid2(i_imageSize,i_ImageOffset,i_imageScale ,i_viewportSize, i_inputUV, sampledTexel.a, texel);
			
    }
	
	//texel.w = 1;
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

    output.texelOut = GetFinalTexel(input.uv, baseImageData.uViewportSize, baseImageData.uImageSize, baseImageData.uScale, baseImageData.uImageOffset, uShowGrid);
}
#else
////////////////////////
///OPENGL GLSL FRAGMENT SHADER
///////////////////////
in vec2 coords;
out vec4 outColor;
void main()
{
  outColor = GetFinalTexel(coords, uViewportSize, uImageSize,uScale, uImageOffset, uShowGrid);
}

#endif
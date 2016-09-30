static const float4 white = float4(1, 1, 1, 1);
static const float4 gray25 = float4(0.75, 0.75, 0.75, 1);
static const float4 black = float4(0, 0, 0, 1);
static const float4 midnightBlue = float4(25 / 255.0, 25 / 255.0 , 112 / 255.0, 1);
static const float4 darkBlue = float4(0 / 255.0, 0 / 255.0, 40 / 255.0, 1);
static const float4 red = float4(1, 0, 0, 1);
uniform Texture2D texture_1;
uniform SamplerState textureState_1;
uniform float2 uvScale;
uniform float2 uvOffset;
uniform float2 uImageSize;
uniform float2 uViewportSize;
uniform int uShowGrid;

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
		offset = uvOffset / uvScale % pixelSize;
		if (pixelSize.x >= pixelDrawThreshold.x && pixelSize.y >= pixelDrawThreshold.y)
		{
			float2 lineWidth = lineWidthInPixels / screenSize;
			float2 modaa = (screenUV + offset)  % (1.0 / factor2);
			if (modaa.x < lineWidth.x || modaa.y < lineWidth.y )
				texel = red;
		}
} 

void FillBackGround(float2 uv,float2 screenUV, inout float4 texel)
{
	if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
        texel = GetChecker(black, darkBlue, screenUV);
}
void DrawImage(float2 uv, float2 screenUV, inout float4 texel)
{
    float4 sampledTexel = texture_1.Sample(textureState_1, uv);
    float4 checkerColor = GetChecker(white, gray25, screenUV);
    texel = lerp(checkerColor, sampledTexel, sampledTexel.w);
}
void main(in ShaderIn input, out ShaderOut output)
{
    float4 texel = (float4)0;
    float2 uv = input.uv * uvScale + uvOffset;

    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
		FillBackGround(uv, input.uv, texel);
	else
	{
		DrawImage(uv, input.uv, texel);
		if (uShowGrid == 1)
        	        DrawPixelGrid(uImageSize,uViewportSize,input.uv,uvScale,uvOffset,texel);
	}
		
	texel.w = 1;
    output.texelOut = texel;
}
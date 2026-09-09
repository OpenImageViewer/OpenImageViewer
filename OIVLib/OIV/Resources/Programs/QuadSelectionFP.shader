#if defined(VK)
layout(push_constant) uniform SelectionPushConstants
{
    vec2 viewportSize;
    vec4 selectionRect;
} pushConstants;

layout(location = 0) in vec2 coords;
layout(location = 0) out vec4 outColor;

vec4 GetFinalTexel(vec2 inputUV)
{
    vec2 viewportSize = pushConstants.viewportSize;
    vec2 pixelOnScreen = viewportSize * inputUV;

    vec2 p0 = pushConstants.selectionRect.xy;
    vec2 p1 = pushConstants.selectionRect.zw;

    const float BorderPrecentage = 0.01;
    const float MinBorderWidth = 1.0;
    const float MaxBorderWidth = 1.0;
    const float segmentPrecantage = 0.1;
    const float MinSegmentLength = 15.0;
    const float MaxSegmentLength = 100.0;

    vec2 rectSize = p1 - p0;
    vec2 borderRequestedSize = rectSize * BorderPrecentage;
    float borderSize = clamp(min(borderRequestedSize.x, borderRequestedSize.y), MinBorderWidth, MaxBorderWidth);

    vec2 segmentLength = clamp(rectSize * segmentPrecantage, vec2(MinSegmentLength), vec2(MaxSegmentLength));
    vec2 halfSegmentLength = segmentLength / 2.0;

    if (pixelOnScreen.x > p0.x && pixelOnScreen.x < p1.x && pixelOnScreen.y > p0.y && pixelOnScreen.y < p1.y)
    {
        float d1 = abs(pixelOnScreen.x - p0.x);
        float d2 = abs(pixelOnScreen.y - p0.y);
        float d3 = abs(pixelOnScreen.x - p1.x);
        float d4 = abs(pixelOnScreen.y - p1.y);

        float minimum = min(d1, min(d2, min(d3, d4)));

        if (minimum <= borderSize)
        {
            vec2 rectSpace = pixelOnScreen - p0;
            vec4 selectionRectColor;

            if (minimum == d2 || minimum == d4)
            {
                if (mod(rectSpace.x, segmentLength.x) < halfSegmentLength.x)
                    selectionRectColor = vec4(0.0, 120.0/255.0, 215.0/255.0, 1.0);
                else
                    selectionRectColor = vec4(1.0, 1.0, 0.0, 1.0);
            }
            else if (minimum == d1 || minimum == d3)
            {
                if (mod(rectSpace.y, segmentLength.y) < halfSegmentLength.y)
                    selectionRectColor = vec4(0.0, 120.0/255.0, 215.0/255.0, 1.0);
                else
                    selectionRectColor = vec4(1.0, 1.0, 0.0, 1.0);
            }
            else
                selectionRectColor = vec4(0.0, 0.0, 0.0, 0.0);

            return selectionRectColor;
        }
        else
            return vec4(0.0, 0.0, 0.0, 0.0);
    }
    else
    {
        return vec4(1.0, 1.0, 1.0, 0.8);
    }
}

void main()
{
    outColor = GetFinalTexel(coords);
}
#else

uniform int2 uViewportSize;
uniform int4 uSelectionRect;


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


float4 GetFinalTexel(float2 i_inputUV)
{
	static const float4 BackgroundBlend  = float4(1.0,1.0,1.0, 0.8);
	static const float4 InsideRectBlend  = float4(0.0, 0.0, 0.0, 0.0);
	static const float4 BorderColor1 = float4(0 / 255.0, 120 / 255.0, 215 / 255.0,1.0);
	static const float4 BorderColor2 = float4(255 / 255.0, 255 / 255.0, 0 / 255.0,1.0);
	static const float segmentPrecantage = 0.1;
	static const float MaxSegmentLength = 100;
	static const float MinSegmentLength = 15;
	static const float BorderPrecentage = 0.01;
	static const float MaxBorderWidth = 1;
	static const float MinBorderWidth = 1;


	float2 viewportSize = float2(uViewportSize.xy);
	float2 pixelOnScreen = viewportSize.xy * i_inputUV;

	float2 p0 = float2(uSelectionRect.xy);
	float2 p1 = float2(uSelectionRect.zw);

	const float2 rectSize = p1 - p0;
	const float2 borderRequestedSize = rectSize * BorderPrecentage;

	const float BorderSize = clamp( min(borderRequestedSize.x, borderRequestedSize.y), MinBorderWidth, MaxBorderWidth);

	const float2 segmentLength = clamp(rectSize * segmentPrecantage,MinSegmentLength,MaxSegmentLength);
	const float2 halfSegmentLength = segmentLength / 2;


	if (pixelOnScreen.x >  p0.x  &&  pixelOnScreen.x < p1.x
	&& pixelOnScreen.y > p0.y &&  pixelOnScreen.y < p1.y)
	{
		float4 selectionRectColor;

		float d1 = abs(pixelOnScreen.x -  p0.x);
		float d2 = abs(pixelOnScreen.y -  p0.y);
		float d3 = abs(pixelOnScreen.x -  p1.x);
		float d4 = abs(pixelOnScreen.y -  p1.y);

		float minimum = min(d1,min(d2,min(d3,d4)));


		if (minimum <= BorderSize)
		{
			float2 rectSpace = pixelOnScreen - p0;

			if (minimum  == d2 || minimum  == d4)
			{
		        	if ( rectSpace.x  % segmentLength.x < halfSegmentLength.x )

				    selectionRectColor = BorderColor1;

			      else
                selectionRectColor = BorderColor2;
			}
			else if (minimum  == d1 || minimum  == d3)
			{
		        	if ( rectSpace.y  % segmentLength.y < halfSegmentLength.y )

				    selectionRectColor = BorderColor1;

			      else
                selectionRectColor = BorderColor2;
			}



		}
		else
			 selectionRectColor = InsideRectBlend;

		return selectionRectColor;
	}
    else
	{
		//Whiten background
        return BackgroundBlend;
	}
}
void main(in ShaderIn input, out ShaderOut output)
{
    output.texelOut = GetFinalTexel(input.uv);
}
#else
////////////////////////
///OPENGL GLSL FRAGMENT SHADER
/////////////////////////
in vec2 coords;
out vec4 outColor;
void main()
{
    outColor = GetFinalTexel(coords, uViewportSize, uImageSize, uvScale, uvOffset, uShowGrid);
}
#endif
#endif

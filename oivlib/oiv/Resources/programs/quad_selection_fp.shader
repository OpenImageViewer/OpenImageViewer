
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
	static const float MaxBorderWidth = 7;
	static const float MinBorderWidth = 2;


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
///////////////////////
in vec2 coords;
out vec4 outColor;
void main()
{
    outColor = GetFinalTexel(coords, uViewportSize, uImageSize, uvScale, uvOffset, uShowGrid);
}

#endif
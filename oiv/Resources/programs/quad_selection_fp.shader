
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
	float2 viewportSize = float2(uViewportSize.xy);
	float2 pixelOnScreen = viewportSize.xy * i_inputUV;
	
	float2 p0 = float2(uSelectionRect.xy);
	float2 p1 = float2(uSelectionRect.zw);
	
	if (pixelOnScreen.x >  p0.x  &&  pixelOnScreen.x < p1.x 
	&& pixelOnScreen.y > p0.y &&  pixelOnScreen.y < p1.y)
	{
		float4 selectionRectColor;
		
		float d1 = abs(pixelOnScreen.x -  p0.x);
		float d2 = abs(pixelOnScreen.y -  p0.y);
		float d3 = abs(pixelOnScreen.x -  p1.x);
		float d4 = abs(pixelOnScreen.y -  p1.y);
		
		float minimum = min(d1,min(d2,min(d3,d4)));
		if (minimum <= 1)
		      selectionRectColor = float4(0 / 255.0, 120 / 255.0, 215 / 255.0,1);
		else
			selectionRectColor = float4(0.7, 0.7, 1, 0.25);
		return selectionRectColor;
	}
    else
	{
		//Darken background
        return float4(0, 0, 0,0.5 );
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
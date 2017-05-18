
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
	float2 p0 = float2(uSelectionRect.xy) / viewportSize;
	float2 p1 = float2(uSelectionRect.zw) / viewportSize; 
	
	if (i_inputUV.x >  p0.x  &&  i_inputUV.x < p1.x 
	&& i_inputUV.y > p0.y &&  i_inputUV.y < p1.y)
	
        return float4(1, 1, 1,0.15);
    else
        return float4(0, 0, 0,0.4 );
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
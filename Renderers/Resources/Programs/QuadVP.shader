#if defined(HLSL) || defined(D3D11)
////////////////////////
///DIRECT3D HLSL VERTEX SHADER
///////////////////////
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

#elif GLSL
////////////////////////
///OPENGL GLSL VERTEX SHADER
/////////////////////////
in vec2 position;
out vec2 coords;
void main()
{
	coords = vec2(position * 0.5 + 0.5);
	coords.y = 1.0 -  coords.y;
	gl_Position = vec4(position, 0.0, 1.0);
}

#elif defined(VK)
layout(location = 0) out vec2 coords;

vec2 quadPositions[4] = vec2[4](
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(1.0, -1.0)
);

void main()
{
    coords = quadPositions[gl_VertexIndex] * 0.5 + 0.5;
    gl_Position = vec4(quadPositions[gl_VertexIndex], 0.0, 1.0);
}
#endif

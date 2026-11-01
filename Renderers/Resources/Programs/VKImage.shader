layout(push_constant) uniform PushConstants
{
    vec2 viewportSize;
    vec2 imageSize;
    vec2 imageScale;
    vec2 imageOffset;
    vec4 backgroundColor1;
    vec4 backgroundColor2;
    vec4 transparencyColor1;
    vec4 transparencyColor2;
    float opacity;
    float exposure;
    float colorOffset;
    float gamma;
    float saturation;
    int showGrid;
} pushConstants;

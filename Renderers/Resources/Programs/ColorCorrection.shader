float3 saturate(float3 color, float amount)
{
	const float3 RGBWeights = float3(0.299, 0.587, 0.114);
	float3 v  = color * color * RGBWeights;
	float luminance = sqrt(v.r + v.g + v.b); 
	return  luminance + ( color - luminance) * amount;
}

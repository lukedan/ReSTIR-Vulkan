

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn, float gamma)
{
  return vec4(pow(srgbIn.xyz, vec3(gamma)), srgbIn.w);
}

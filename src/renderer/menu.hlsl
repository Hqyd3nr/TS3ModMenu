// Menu shader: textured, coloured triangles. During a theme change it only draws inside the
// reveal circle.
cbuffer View : register(b0) { float2 scale; float2 offset; uint crisp; float2 revealOrigin; float revealRadius; uint revealEnabled; };
struct Input {float2 pos:POSITION;float2 uv:TEXCOORD0;float4 color:COLOR0;};
struct Output {float4 pos:SV_POSITION;float2 uv:TEXCOORD0;float4 color:COLOR0;};
Output VS(Input i){Output o;o.pos=float4(i.pos*scale+offset,0,1);o.uv=i.uv;o.color=i.color;return o;}
Texture2D image:register(t0);SamplerState filter:register(s0);
float4 PS(Output i):SV_TARGET {if(revealEnabled)clip(revealRadius-distance(i.pos.xy,revealOrigin));uint w,h;image.GetDimensions(w,h);float4 tex=crisp?image.Load(int3(clamp(int2(i.uv*float2(w,h)),int2(0,0),int2(w-1,h-1)),0)):image.Sample(filter,i.uv);return tex*i.color;}

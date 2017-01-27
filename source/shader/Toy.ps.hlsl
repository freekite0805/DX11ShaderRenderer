
Texture2D backbuffer : register(t0);
cbuffer ToyBuffer : register(b0)
{
	float2     resolution;     // viewport resolution (in pixels)
	float      time;           // shader playback time (in seconds)
	float      aspect;         // cached aspect of viewport
	float4     mouse;          // mouse pixel coords. xy: current (if MLB down), zw: click
};

// Shader Toy Clouds
// Originally Created by inigo quilez - Edited by Matthew Bennett to work without textures & ported to HLSL
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// Volumetric clouds. It performs level of detail (LOD) for faster rendering

// Create noise without texture https://www.shadertoy.com/view/4sfGzS
float iqhash(float n)
{
	return frac(sin(n)*43758.5453);
}

float noise(float3 x)
{
	// The noise function returns a value in the range -1.0f -> 1.0f
	float3 p = floor(x);
	float3 f = frac(x);

	f = f*f*(3.0 - 2.0*f);
	float n = p.x + p.y*57.0 + 113.0*p.z;
	return lerp(lerp(lerp(iqhash(n + 0.0), iqhash(n + 1.0), f.x),
		lerp(iqhash(n + 57.0), iqhash(n + 58.0), f.x), f.y),
		lerp(lerp(iqhash(n + 113.0), iqhash(n + 114.0), f.x),
			lerp(iqhash(n + 170.0), iqhash(n + 171.0), f.x), f.y), f.z);
}


float map5(in float3 p)
{
	float3 q = p - float3(0.0, 0.1, 1.0)*time;
	float f;
	f = 0.50000*noise(q); q = q*2.02;
	f += 0.25000*noise(q); q = q*2.03;
	f += 0.12500*noise(q); q = q*2.01;
	f += 0.06250*noise(q); q = q*2.02;
	f += 0.03125*noise(q);
	return clamp(1.5 - p.y - 2.0 + 1.75*f, 0.0, 1.0);
}

float map4(in float3 p)
{
	float3 q = p - float3(0.0, 0.1, 1.0)*time;
	float f;
	f = 0.50000*noise(q); q = q*2.02;
	f += 0.25000*noise(q); q = q*2.03;
	f += 0.12500*noise(q); q = q*2.01;
	f += 0.06250*noise(q);
	return clamp(1.5 - p.y - 2.0 + 1.75*f, 0.0, 1.0);
}
float map3(in float3 p)
{
	float3 q = p - float3(0.0, 0.1, 1.0)*time;
	float f;
	f = 0.50000*noise(q); q = q*2.02;
	f += 0.25000*noise(q); q = q*2.03;
	f += 0.12500*noise(q);
	return clamp(1.5 - p.y - 2.0 + 1.75*f, 0.0, 1.0);
}
float map2(in float3 p)
{
	float3 q = p - float3(0.0, 0.1, 1.0)*time;
	float f;
	f = 0.50000*noise(q); q = q*2.02;
	f += 0.25000*noise(q);
	return clamp(1.5 - p.y - 2.0 + 1.75*f, 0.0, 1.0);
}

float3 sundir = normalize(float3(1.0, 0.0, 2.0));

float4 integrate(in float4 sum, in float dif, in float den, in float3 bgcol, in float t)
{
	// lighting
	float3 lin = float3(0.65, 0.7, 0.75)*1.4 + float3(1.0, 0.6, 0.3)*dif;
	float4 col = float4(lerp(float3(1.0, 0.95, 0.8), float3(0.25, 0.3, 0.35), den), den);
	col.xyz *= lin;
	col.xyz = lerp(col.xyz, bgcol, 1.0 - exp(-0.003*t*t));
	// front to back blending    
	col.a *= 0.4;
	col.rgb *= col.a;
	return sum + col*(1.0 - sum.a);
}

#define MARCH(STEPS,MAPLOD) for(int i=0; i<STEPS; i++) { float3  pos = ro + t*rd; if( pos.y<-3.0 || pos.y>2.0 || sum.a > 0.99 ) break; float den = MAPLOD( pos ); if( den>0.01 ) { float dif =  clamp((den - MAPLOD(pos+0.3*sundir))/0.6, 0.0, 1.0 ); sum = integrate( sum, dif, den, bgcol, t ); } t += max(0.05,0.02*t); }

#define STEPS 40

float4 raymarch(in float3 ro, in float3 rd, in float3 bgcol)
{
	float4 sum = float4(0.0, 0.0, 0.0, 0.0);

	float t = 0.0;

	MARCH(STEPS, map5);
	MARCH(STEPS, map4);
	MARCH(STEPS, map3);
	MARCH(STEPS, map2);

	return clamp(sum, 0.0, 1.0);
}

float3x3 setCamera(in float3 ro, in float3 ta, float cr)
{
	float3 cw = normalize(ta - ro);
	float3 cp = float3(sin(cr), cos(cr), 0.0);
	float3 cu = normalize(cross(cw, cp));
	float3 cv = normalize(cross(cu, cw));
	return float3x3(cu, cv, cw);
}

float4 render(in float3 ro, in float3 rd)
{
	// background sky     
	float sun = clamp(dot(sundir, rd), 0.0, 1.0);
	float3 col = float3(0.6, 0.71, 0.75) - rd.y*0.2*float3(1.0, 0.5, 1.0) + 0.15*0.5;
	col += 0.2*float3(1.0, .6, 0.1)*pow(sun, 8.0);

	// clouds    
	float4 res = raymarch(ro, rd, col);
	col = col*(1.0 - res.w) + res.xyz;

	// sun glare    
	col += 0.2*float3(1.0, 0.4, 0.2)*pow(sun, 3.0);

	return float4(col, 1.0);
}

float4 main(float4 pos : SV_Position) : SV_Target
{
	float2 p = (-resolution.xy + 2.0*pos.xy) / resolution.y;

	// camera
	float3 ro = float3(0.0, 1.5, 5.0);
	float3 ta = float3(0.0, 0.0, 0.0);
	float3x3 ca = setCamera(ro, ta, 0.0);
	//ray
	float3 rd = mul(ca, normalize(float3(-p.xy, 1.5)));

	return render(ro, rd);
}

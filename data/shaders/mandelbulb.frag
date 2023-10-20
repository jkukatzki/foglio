// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#version 450 core

uniform sampler2D inTexture;
in vec3 pass_Uvs;
out vec4 out_Color;
uniform UBO
{
	float iTime;
} ubo;
/* void main() 
{
	vec4 myTexture = texture(inTexture, vec2(pass_Uvs.x, pass_Uvs.y));
	out_Color = vec4(myTexture.xyz, 0.5);
	
} */

float hash(float p)
{
    return fract(sin(dot(vec2(p), vec2(12.9898, 78.233))) * 43758.5453);    
}

float map(in vec3 pos, out vec3 orbit_trap)
{
    float thres = length(pos) - 1.2;
    if (thres > 0.2) {
        return thres;
    }
    
    // Zn <- Zn^8 + c
    // Zn' <- 8*Zn^7 + 1    
    const float power = 2.0;
    vec3 z = pos;
    vec3 c = pos;
    
    orbit_trap = vec3(1e20);
    
    float dr = 1.0;
    float r = 0.0;
    for (int i = 0; i < 100; ++i) {        
        // to polar
        r = length(z);
        if (r > 2.0) { break; }        
        float theta = acos(z.z/r);
        float phi = atan(z.y, z.x);
        
        // derivate
        dr = pow(r, power - 1.0) * power * dr + 1.0;
        
        // scale and rotate
        float zr = pow(r, power);
        theta *= power;
        phi *= power;
        
        // to cartesian
        z = zr * vec3(sin(theta)*cos(phi), sin(phi)*sin(theta), cos(theta));        
        z += c;
               
        orbit_trap.x = min(pow(abs(z.z),0.1), orbit_trap.x);
        orbit_trap.y = min(abs(z.x) - 0.15, orbit_trap.y);
        orbit_trap.z = min(length(z), orbit_trap.z);
    }
    
    return 0.5 * log(r) * r / dr;
}

vec3 calcNormal(vec3 pos)
{
    vec3 trash;

    // Tetrahedron technique
    // https://iquilezles.org/articles/normalsSDF
    const float h = 0.0001;
    const vec2 k = vec2(1,-1);
    return normalize(
        k.xyy * map(pos + k.xyy * h, trash) + 
        k.yyx * map(pos + k.yyx * h, trash) + 
        k.yxy * map(pos + k.yxy * h, trash) + 
        k.xxx * map(pos + k.xxx * h, trash)
    );
}

float ambientOcclusion(vec3 pos, vec3 N, float fallout)
{
    vec3 trash;    
    const int nS = 12; // number of samples
    const float max_dist = 0.07;
    
    float diff = 0.0;
    for (int i = 0; i < nS; ++i)
    {        
        float dist = max_dist * hash(float(i)); // rand dist        
        float s_dist = max(0.0, map(pos + dist * N, trash)); // sample
        
        diff += (dist - s_dist) / max_dist;
    }
    
    float diff_norm = diff / float(nS);
    float ao = 1.0 - diff_norm/fallout;
    
    return clamp(0.0, 1.0, ao);
}

float castRay(vec3 ro, vec3 rd, out vec3 trap)
{
    const float tmax = 200.0;
    float t = 0.0;
    for (int i = 0; i < 100; ++i)
    {
        vec3 pos = ro + t * rd;
        float h = map(pos, trap);
        if (h < 0.0003)
        {
            break;
        }
        t += h;
        if (t > tmax)
        {
            t = -1.0;
            break;
        }
    }
    return t;
}

void main()
{
	vec2 iResolution = vec2(1000, 1000);
	vec2 fragCoord = vec2(pass_Uvs.x*1920, pass_Uvs.y*1080);
	float iTime = ubo.iTime;
    float freq = 80.0 + iTime;
    vec3 cam_pos = vec3(20.0 * cos(0.1 * 0.125 * freq) * sin(0.1 * 0.5*freq), sin(0.1 * freq), 2.0 * cos(0.1 * 0.5 * freq));
    const vec3 cam_target = vec3(0);
    
    const float fov = 90.0 * 3.141592 / 180.0;
    float h = 1.0; //tan(fov/2.0) * length(cam_target - cam_pos);
    
    vec3 cam_ww = normalize(cam_target - cam_pos);
    
    vec3 cam_uu = normalize(cross(vec3(0,1,0), cam_ww));
    vec3 cam_vv = normalize(cross(cam_ww, cam_uu));
    
    vec3 final_col = vec3(0);
    
    #if AA
    for( int m=0; m<AA; ++m )
    for( int n=0; n<AA; ++n )
    {
	vec2 o = vec2(n, m) / float(AA);
    vec2 p = (2.0 * (fragCoord + o) - iResolution.xy) / iResolution.y;
	#else
	vec2 p = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
    #endif

	vec3 col = mix(vec3(0.001), vec3(0.05), clamp(exp(2.0*p.y),0.0,1.0));

	vec3 ro = cam_pos;
    vec3 rd = normalize(p.x * h * cam_uu + p.y * h * cam_vv + cam_ww - ro);

    vec3 trap;
    float t = castRay(ro, rd, trap);

	/*
	Palette:
	{[0.906, 0.929, 0.922], [0, 0.227, 0.42], [0.259, 0.765, 0.969]}
	{[0.373, 0.18, 0.18], [0.165, 0.125, 0.165], [0.545, 0.255, 0.212]}
	*/

	if (t > 0.0)
    {           
    	vec3 base_col1 = mix(vec3(0.906, 0.929, 0.922), vec3(0.373, 0.18, 0.18), abs(sin(0.1*iTime)));
        vec3 base_col2 = mix(vec3(0, 0.227, 0.42), vec3(0.165, 0.125, 0.165), abs(sin(0.1*iTime)));
        vec3 base_col3 = mix(vec3(0.259, 0.765, 0.969), vec3(0.545, 0.255, 0.212), abs(sin(0.1*iTime)));

        col = base_col1 * clamp(pow(trap.x,20.0),0.0,1.0);
        col += base_col2 * clamp(pow(trap.y,20.0),0.0,1.0);
        col += base_col3 * clamp(pow(trap.z,20.0),0.0,1.0);

        vec3 pos = ro + t * rd;
        vec3 N = calcNormal(pos);
        float ao = ambientOcclusion(pos, N, 0.46);

        col *= 0.01 + ao;        
	}
    else
    {
    	col = vec3(0,0,0); //texture(inTexture, vec2(pass_Uvs.x, pass_Uvs.y)).xyz;
	}
#if AA
    final_col += col;
    }
    final_col /= float(AA * AA);
#else
    final_col = col;
#endif        
    
    // RGB -> sRGB
    final_col = pow(final_col, vec3(0.4545));
    
    // contrast
    final_col = 1.1 * (final_col - 0.5) + 0.5;
    final_col = clamp(final_col, vec3(0.0), vec3(1.0));
    
    out_Color = vec4(vec3(final_col),1.0);
}
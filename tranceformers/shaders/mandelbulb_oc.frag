// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#version 450 core

#define LIGHTCOL vec3(1.f, 0.9f, 0.8f)
#define O_COLOR  vec3(0.5f, 0.7f, 1.f)
#define LIGHTDIR vec3(1.f, 1.f, 1.f)
#define B_COLOR  vec3(0.8f, 0.7f, 0.6f)

uniform sampler2D inTexture;
in vec3 pass_Uvs;
out vec4 out_Color;
uniform UBO
{
	float iTime;
    float power_to;
} ubo;
/* void main() 
{
	vec4 myTexture = texture(inTexture, vec2(pass_Uvs.x, pass_Uvs.y));
	out_Color = vec4(myTexture.xyz, 0.5);
	
} */


mat3 ident(){
    return mat3(vec3(1., 0., 0.),
                vec3(0., 1., 0.),
                vec3(0., 0., 1.));
}

mat3 rotatorXAxis(in float dRotation){
    return mat3(vec3(1., 0., 0),
                vec3(0., cos(radians(dRotation)), sin(radians(dRotation))),
                vec3(0., -sin(radians(dRotation)), cos(radians(dRotation))));
}

mat3 rotatorYAxis(in float dRotation){
    return mat3(vec3(cos(radians(dRotation)), 0., -sin(radians(dRotation))),
                vec3(0., 1., 0.),
                vec3(sin(radians(dRotation)), 0., cos(radians(dRotation))));
}

float mandelbulb(vec3 pos){
    vec3 v = pos;
    vec3 c = v;
    float n = 6.f * (sin(ubo.iTime * 0.02f) * 0.5f + 0.5f) + 2.f;
    float dr = 1.f;
    float nextR = length(v);
    float r, phi, theta, iX, iY, iZ, rN;
    
    for(int i = 0; i < 6; i++){
        r = length(v);
        if(r > 3.f){
            break;
        }
        phi = atan(v.y, v.x);
        theta = acos(v.z / r) + ubo.iTime * 0.25f;
        
        iX = sin(n * theta) * cos(n * phi);
        iY = sin(n * theta) * sin(n * phi);
        iZ = cos(n * theta);
        
        rN = pow(r, n);
        v = rN * vec3(iX, iY, iZ) + c;
        
        dr = pow(r, n - 1.f) * n * dr + 1.f;
    }
    
    return 0.5f * log(r) * r / dr;
}

vec4 rayMarching(vec3 startPos, vec3 rayDir, int maxSteps){
    vec3 curPos = startPos;
    float curSDF = mandelbulb(curPos);
    float depth = 0.f;
    for(int i = 0; i < maxSteps; i++){
        if(curSDF < 0.00000f || depth > 10.f){
            break;
        }
        curPos += curSDF * rayDir;
        depth += curSDF;
        
        curSDF = mandelbulb(curPos);
    }
    
    return vec4(curPos, depth);
}

vec3 normal(vec3 pos){
    vec3 offset = vec3(0.01f, 0.f, 0.f);
    
    return normalize(vec3(mandelbulb(pos + offset.xyz) - mandelbulb(pos - offset.xyz), 
                          mandelbulb(pos + offset.yxz) - mandelbulb(pos - offset.yxz), 
                          mandelbulb(pos + offset.yyx) - mandelbulb(pos - offset.yyx)));
}

vec3 lambert(vec3 nor){
    vec3 lightDir = normalize(LIGHTDIR);
    float shadow = dot(nor, lightDir) * 0.5 + 0.5;
    
    return shadow * O_COLOR * LIGHTCOL;
}

vec3 phong(vec3 nor,vec3 camDir){
    vec3 lightDir = normalize(LIGHTDIR);
    vec3 h = normalize(lightDir + camDir);
    float gloss = 0.0f;
    vec3 phong = LIGHTCOL * O_COLOR * pow(max(dot(nor, h), 0.f), gloss);
    
    return phong;
}

void main()
{
    vec2 iResolution = vec2(1920, 1080);
    vec2 fragCoord = vec2(pass_Uvs.x*1920, pass_Uvs.y*1080);
    vec2 VF = (fragCoord.xy - iResolution.xy / 2.f) / min(iResolution.x, iResolution.y);
    vec3 dir = normalize(vec3(VF, 1.f));
    vec3 startPos = vec3(0.f, 1.f, -2.f);
    
    //mat3 rotator = rotatorYAxis(radians(ubo.iTime * 360.f * 5.f));
    //dir *= rotator;
    //startPos *= rotator;
    
    vec4 mandelbulbDate = rayMarching(startPos, dir, 255);
    
    vec3 nor = normal(mandelbulbDate.xyz);
    
    vec3 lambert = lambert(nor);
    vec3 phong = phong(nor, -dir);
    vec3 col = lambert + phong;
    float background = float(mandelbulbDate.w < 10.f);
    vec3 b_Col = (1.f - background) * B_COLOR;
    out_Color = vec4(vec3(col * background + b_Col), 1.f);
}
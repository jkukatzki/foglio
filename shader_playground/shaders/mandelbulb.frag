// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#version 450 core
float PI = 3.1415;
uniform sampler2D inTexture;
in vec3 pass_Uvs;
out vec4 out_Color;
uniform UBO
{
    int cameraPath;
    float audio_bass;
    float audio_mids;
    float audio_highs;
	float iTime;
    float midiKnob0;
    float midiKnob1;
    float midiKnob2;
    float midiKnob3;
    float midiKnob4;
    float midiKnob5;
    float midiKnob6;
    float midiKnob7;
    float midiPitchBend;
    float midiPitchBendAcc;
} ubo;

mat2 rot2D(float a) {
    return mat2(cos(a), -sin(a), sin(a), cos(a));
}

float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

float mandelbulb(vec3 p, int cameraPath) {
    float n = 14.0 + sin(ubo.iTime / 10)*4.0;
    float dr = 1.0;
    float r = 0.0;
    vec3 z = p;
    vec3 variation = vec3(0.0, 0.0, 0.0);
    if (cameraPath == 0) {
        variation = vec3(ubo.audio_bass * 2, ubo.audio_mids, ubo.audio_highs);
    } else if (cameraPath == 1) {
        variation = vec3(0, ubo.audio_mids * 2.0, ubo.audio_highs);
    } else if (cameraPath == 2) {
        variation = vec3(ubo.audio_bass, ubo.audio_mids, ubo.audio_highs);
    }
    for (int i = 0; i < 6; i++) {
        r = length(z);
        if (r > 2.0) break;
        // convert to polar coordinates
        float theta = acos(z.z/r);
        float phi = atan(z.y,z.x);
        dr =  pow( r, n) * n * dr + 1.0;
        // scale and rotate the point
        float zr = pow( r, n);
        theta = theta*n - variation[0];
        phi = phi*n;
        // convert back to cartesian coordinates
        z = zr*vec3(sin(theta*0.5 + variation[0])*cos(phi + variation[1]), sin(phi + variation[2])*sin(theta), cos(theta));
        z+=p;
    }
    return 0.5*log(r)*r/dr;
}

float map(vec3 p, int cameraPath) {
    float d = mandelbulb(p, cameraPath);
    return d;
}



void main() 
{
    vec3 new_pass_Uvs = (pass_Uvs - 0.5) * 2.0;
    vec3 rd = normalize(vec3(new_pass_Uvs.xy, 1));
    vec3 cameraOrbit = vec3(0.0, 0.0, 0.0);
    int cameraPath = int(mod(ubo.iTime / 248 + ubo.cameraPath, 3));
    if (cameraPath == 0) {
        float cameraSpeed = 10;
        rd.xz *= rot2D(-PI-ubo.iTime/cameraSpeed);
        rd.zy *= rot2D(-PI-ubo.iTime/cameraSpeed);
        cameraOrbit = vec3(sin(ubo.iTime/cameraSpeed), cos(ubo.iTime/cameraSpeed), 0.);
        cameraOrbit *= 1.5;
    } else if (cameraPath == 1) {
        float cameraSpeed = 6.0;
        rd.xy *= rot2D(-PI/2);
        rd.xz *= rot2D(-PI-ubo.iTime/cameraSpeed);
        cameraOrbit = vec3(sin(ubo.iTime/cameraSpeed), cos(ubo.iTime/cameraSpeed), 0.);
        cameraOrbit *= 1.8 - ubo.audio_bass*0.4;
    } else if (cameraPath == 2) {
        float cameraSpeed = 5.0;
        cameraOrbit = vec3(sin(ubo.iTime/cameraSpeed), cos(ubo.iTime/cameraSpeed), 0.);
        rd.xy *= rot2D(-PI/2);
        rd.xz *= rot2D(-0.76*PI-ubo.iTime/cameraSpeed);
        cameraOrbit *= 1.35;
    }
    
	vec3 ro = vec3(cameraOrbit.x, cameraOrbit.z, cameraOrbit.y);
    
	


    float t = 0.; // total distance traveled

    // marching loop
    int maxSteps = 300;
    int i; 
    for (i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t; // current position
        float d = map(p, cameraPath); // distance to closest surface
        t += d; // increment distance traveled
        if (d < 0.001) break; // close enough to call it a hit
        if ( d > 100.0) break; // marched too far, give up
    }
    vec3 col = vec3(i) / maxSteps;
    float colorChangeSpeed = 0.2;
    vec3 noiseColor = vec3( (sin(ubo.iTime*colorChangeSpeed)+1.0)/2, (1.0+cos(ubo.iTime*colorChangeSpeed*1.3))/2, (1.0+sin(ubo.iTime*colorChangeSpeed*1.6))/2 );
    noiseColor = normalize(noiseColor);
    noiseColor *= 1.7;
    col *= noiseColor;
    //col *= vec3(1.0, 0.5, 0.5)*2.0;
    col = col * (1.0 - (t/100.0)) *1.5;
    out_Color = vec4(col, 1.0);
}


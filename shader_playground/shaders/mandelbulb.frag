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

float mandelbulb(vec3 p) {
    float n = ubo.midiKnob0*16.0;
    float dr = 1.0;
    float r = 0.0;
    vec3 z = p;
    for (int i = 0; i < 6; i++) {
        r = length(z);
        if (r > 2.0) break;
        // convert to polar coordinates
        float theta = acos(z.z/r);
        float phi = atan(z.y,z.x);
        dr =  pow( r, n) * n * dr + 1.0;
        // scale and rotate the point
        float zr = pow( r, n);
        theta = theta*n;
        phi = phi*n;
        // convert back to cartesian coordinates
        z = zr*vec3(sin(theta)*cos(phi), sin(phi)*sin(theta), cos(theta));
        z+=p;
    }
    return 0.5*log(r)*r/dr;
}

float map(vec3 p) {
    float d = mandelbulb(p);
    return d;
}



void main() 
{
    vec2 cameraOrbit = vec2(sin(ubo.midiPitchBendAcc*2), cos(ubo.midiPitchBendAcc*2));
    cameraOrbit *= 1.0+10.0*ubo.midiKnob4;
	vec3 ro = vec3(cameraOrbit.x, 0., cameraOrbit.y);
    vec3 new_pass_Uvs = (pass_Uvs - 0.5) * 2.0;
	vec3 rd = normalize(vec3(new_pass_Uvs.xy, 1));
    rd.xz *= rot2D(-PI-ubo.midiPitchBendAcc*2.0);


    float t = 0.; // total distance traveled

    // marching loop
    int maxSteps = 300;
    int i; 
    for (i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t; // current position
        float d = map(p); // distance to closest surface
        t += d; // increment distance traveled
        if (d < 0.001) break; // close enough to call it a hit
        if ( d > 100.0) break; // marched too far, give up
    }
    vec3 col = vec3(i) / maxSteps;
    col *= vec3(1.0, 0.5, 0.5)*2.0;
    col = col * (1.0 - (t/100.0)) *1.5;
    out_Color = vec4(col, 1.0);
}


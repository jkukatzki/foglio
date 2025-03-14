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


void main() 
{
    vec2 blob1Coords = vec2(0.1, 0.1);
    vec2 blob2Coords = vec2(0.3, 0.6);
    vec2 blob3Coords = vec2(0.6, 0.1);
    vec2 blob4Coords = vec2(0.8, 0.4);
    vec3 new_pass_Uvs = pass_Uvs;
    vec3 col = vec3(0.0);
    if (length(new_pass_Uvs.xy - blob1Coords) < 0.1 * ( 0.8 + ubo.audio_mids + ubo.audio_bass)) {
        col = vec3(0.9059, 0.3333, 0.1255);
    }
    else if (length(new_pass_Uvs.xy - blob2Coords) < 0.3 * (1.0 + ubo.audio_bass)) {
        col = vec3(0.1059, 0.3922, 0.1059);
    } else if (length(new_pass_Uvs.xy - blob3Coords) < 0.2 * (1.0 + ubo.audio_mids)) {
        col = vec3(0.102, 0.102, 0.5255);
    } else if (length(new_pass_Uvs.xy - blob4Coords) < 0.1 * (1.0 + ubo.audio_highs)) {
        col = vec3(0.9882, 0.8745, 0.0);
    }
    
    out_Color = vec4(col, 1.0);
}


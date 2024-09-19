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

void main() 
{
    out_Color = vec4(texture(inTexture, pass_Uvs.xy).xyz*10.0, 1.0);
}


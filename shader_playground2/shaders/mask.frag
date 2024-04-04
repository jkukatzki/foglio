// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#version 450 core

uniform sampler2D i_flower;
uniform sampler2D inTexture;
in vec3 pass_Uvs;
out vec4 out_Color;

uniform UBO
{
	float someValue;
} ubo;

void main() 
{
	vec4 myTexture = texture(inTexture, vec2(pass_Uvs.x, pass_Uvs.y));
	out_Color = myTexture;
    //out_Color = vec4(myTexture.xyz, texture(i_mask, vec2(pass_Uvs.x, pass_Uvs.y)).w);
}
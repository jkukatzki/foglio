// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#version 450 core

uniform sampler2D maskTexture;
uniform sampler2D inTexture;
in vec3 pass_Uvs;
out vec4 out_Color;

void main() 
{
	out_Color = vec4(texture(inTexture, pass_Uvs.xy).xyz, texture(maskTexture, vec2(pass_Uvs.x, pass_Uvs.y)).w);
}
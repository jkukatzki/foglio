// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#version 450 core

uniform sampler2D inTexture;

uniform UBO {
	float frameThickness;
	vec3 mousePos;
} ubo;

in vec3 pass_Uvs;

out vec4 out_Color;

void main() 
{
	out_Color = texture(inTexture, vec2(pass_Uvs.x, pass_Uvs.y));
}
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#version 450 core
uniform nap
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 modelMatrix;
} mvp;

in vec3	in_Position;
in vec3	in_UV0;
in vec3 in_CornerOffset;
out vec3 pass_Uvs;

vec3 new_pos = in_Position.xyz + in_CornerOffset.xyz;


void main(void)
{
	gl_Position = mvp.projectionMatrix * mvp.viewMatrix * mvp.modelMatrix * vec4(new_pos, 1.0);
	pass_Uvs = in_UV0;
}
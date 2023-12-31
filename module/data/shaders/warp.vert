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

uniform UBO
{
	vec3 topLeft;
	vec3 topRight;
	vec3 bottomLeft;
	vec3 bottomRight;
} ubo;

in vec3	in_Position;
in vec3	in_UV0;
out vec3 pass_Uvs;

vec2 offsetPosition = mix(
        mix(ubo.bottomLeft.xy, ubo.bottomRight.xy, in_Position.x+0.5),
        mix(ubo.topLeft.xy, ubo.topRight.xy, in_Position.x+0.5),
        in_Position.y+0.5
    );

void main(void)
{
	gl_Position = mvp.projectionMatrix * mvp.viewMatrix * mvp.modelMatrix * vec4(in_Position.xy + offsetPosition, in_Position.z, 1.0);
	pass_Uvs = in_UV0;
}
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
vec2 newTopLeft = vec2(ubo.topLeft.x, 1+ubo.topLeft.y);
vec2 newTopRight = vec2(1+ubo.topRight.x, 1+ubo.topLeft.y);
vec2 newBottomLeft = vec2(ubo.bottomLeft.x, ubo.bottomLeft.y);
vec2 newBottomRight = vec2(1+ubo.bottomRight.x, ubo.bottomRight.y);
float newX = (1.0 - in_Position.y) * ((1.0 - in_Position.x) * newBottomLeft.x + in_Position.x * newBottomRight.x) + in_Position.y * ((1.0 - in_Position.x) * newTopLeft.x + in_Position.x * newTopRight.x);
float newY = (1.0 - in_Position.x) * ((1.0 - in_Position.y) * newBottomLeft.y + in_Position.y * newTopLeft.y) + in_Position.x * ((1.0 - in_Position.y) * newBottomRight.y + in_Position.y * newTopRight.y);



void main(void)
{
	gl_Position = mvp.projectionMatrix * mvp.viewMatrix * mvp.modelMatrix * vec4(newX, newY, in_Position.z, 1.0);
	pass_Uvs = in_UV0;
}
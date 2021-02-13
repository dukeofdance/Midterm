#version 420

layout(binding = 0) uniform sampler2D u_Scene;
layout(binding = 1) uniform sampler2D u_Bloom;

layout(location = 0) in vec2 inUv;

out vec4 FragColor;

void main() 
{
	vec4 colorA = texture(u_Scene, inUv);
	vec4 colorB = texture(u_Bloom, inUv);

	FragColor = 1.0 - (1.0 - colorA) * (1.0 - colorB);
}
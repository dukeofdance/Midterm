#version 420

out vec4 FragColor;

layout(location = 0) in vec2 inUv;

layout(binding = 0) uniform sampler2D s_screenTex;

void main() 
{
	vec4 source = texture(s_screenTex, inUv);

	FragColor.rgb = source.rgb;
	FragColor.a = source.a;
}
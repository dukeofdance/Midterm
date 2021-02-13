#version 420

uniform float u_Threshold;

out vec4 FragColor;

layout (binding = 0) uniform sampler2D s_screenTex;
layout(location = 0) in vec2 inUv;


void main() 
{
	vec4 color = texture(s_screenTex,inUv);
	
	float brightness = (color.r + color.g + color.b) / 3.0;
	
	if (brightness > u_Threshold) 
	{
		FragColor = color;
	}
	else
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
}
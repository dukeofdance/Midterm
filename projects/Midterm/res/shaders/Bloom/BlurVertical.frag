#version 420

uniform float u_PixelSize; //1.0 / Window_Height

out vec4 FragColor;

layout (binding = 0) uniform sampler2D s_screenTex;
layout(location = 0) in vec2 inUv;


void main() 
{
	//Sample pixels in a horizontal row
	//Weight should add up to 1
	FragColor = vec4(0.0, 0.0, 0.0, 0.0);

	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y - 4.0 * u_PixelSize)) * 0.06;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y - 3.0 * u_PixelSize)) * 0.09;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y - 2.0 * u_PixelSize)) * 0.12;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y - 	  u_PixelSize)) * 0.15;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y			   	     )) * 0.16;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y +		  u_PixelSize)) * 0.15;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y + 2.0 * u_PixelSize)) * 0.12;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y + 3.0 * u_PixelSize)) * 0.09;
	FragColor += texture(s_screenTex, vec2(inUv.x, inUv.y + 4.0 * u_PixelSize)) * 0.06;
}
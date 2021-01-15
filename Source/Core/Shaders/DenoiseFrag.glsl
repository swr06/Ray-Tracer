#version 330 core

layout (location = 0) out vec3 o_Color;

in vec2 v_TexCoords;
uniform sampler2D u_CurrentFrame;
uniform sampler2D u_PreviousFrame;
uniform bool u_Denoise = false;

void main()
{
	vec3 Color_1 = texture(u_CurrentFrame, v_TexCoords).rgb;
	vec3 Color_2 = texture(u_PreviousFrame, v_TexCoords).rgb;

	if (u_Denoise)
	{
		o_Color = mix(Color_1, Color_2, 0.9f);
	}

	else
	{
		o_Color = Color_1;
	}
}
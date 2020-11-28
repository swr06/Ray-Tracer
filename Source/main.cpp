#include <stdio.h>
#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <memory>

#include <glad/glad.h>          
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Core/Application.h"
#include "Core/VertexBuffer.h"
#include "Core/VertexArray.h"
#include "Core/Shader.h"

using namespace RayTracer;
typedef uint32_t uint;
typedef unsigned char byte;

// Helpers
struct i8vec2
{
	uint8_t x, y;
};

struct i16vec2
{
	uint16_t x, y;
};

struct ivec2
{
	uint32_t x, y;
};

struct RGB
{
	byte r;
	byte g;
	byte b;
};

const uint g_Width = 800;
const uint g_Height = 600;
GLubyte* const g_PixelData = new GLubyte[g_Width * g_Height * 3];
GLuint g_Texture = 0;

std::unique_ptr<GLClasses::VertexBuffer> g_VBO;
std::unique_ptr<GLClasses::VertexArray> g_VAO;
std::unique_ptr<GLClasses::Shader> g_RenderShader;

int color = 0;

class RayTracerApp : public Application
{
public:

	RayTracerApp()
	{
		m_Width = g_Width;
		m_Height = g_Height;
		memset(g_PixelData, 255, g_Width * g_Height * 3);
	}

	void OnUserCreate(double ts) override
	{

	}

	void OnUserUpdate(double ts) override
	{

	}

	void OnImguiRender(double ts) override
	{
		ImGuiWindowFlags window_flags = 0;

		if (ImGui::Begin("Settings"))
		{
			if (ImGui::SliderInt("Color", &color, 0, 255))
			{

			}
		}

		ImGui::End();
	}

	void OnEvent(Event e) override
	{

	}

};

RayTracerApp g_App;

/* Creating the ray traced texture */
void InitializeForRender()
{
	g_VBO = std::unique_ptr<GLClasses::VertexBuffer>(new GLClasses::VertexBuffer);
	g_VAO = std::unique_ptr<GLClasses::VertexArray>(new GLClasses::VertexArray);
	g_RenderShader = std::unique_ptr<GLClasses::Shader>(new GLClasses::Shader);

	g_RenderShader->CreateShaderProgramFromFile("Core/Shaders/BasicVert.glsl", "Core/Shaders/BasicFrag.glsl");
	g_RenderShader->CompileShaders();

	float Vertices[] =
	{
		-1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f, -1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f
	};

	g_VAO->Bind();
	g_VBO->Bind();
	g_VBO->BufferData(sizeof(Vertices), Vertices, GL_STATIC_DRAW);
	g_VBO->VertexAttribPointer(0, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), 0);
	g_VBO->VertexAttribPointer(1, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
	g_VAO->Unbind();
}

void CreateRenderTexture()
{
	glCreateTextures(GL_TEXTURE_2D, 1, &g_Texture);
	glBindTexture(GL_TEXTURE_2D, g_Texture);
	glTextureStorage2D(g_Texture, 1, GL_RGB8, g_Width, g_Height);
	glTextureParameteri(g_Texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(g_Texture, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTextureParameteri(g_Texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(g_Texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);
}

void BufferTextureData()
{
	glBindTexture(GL_TEXTURE_2D, g_Texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTextureSubImage2D(g_Texture, 0, 0, 0, g_Width, g_Height, GL_RGB, GL_UNSIGNED_BYTE, g_PixelData);
}

void Render()
{
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	g_RenderShader->Use();
	g_RenderShader->SetInteger("u_Texture", 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_Texture);

	g_VAO->Bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);
	g_VAO->Unbind();
}

/* Pixel putter and getter functions */

constexpr void PutPixel(const ivec2& loc, const RGB& col) noexcept
{
	uint _loc = (loc.x + loc.y * g_Width) * 3;
	g_PixelData[_loc + 0] = col.r;
	g_PixelData[_loc + 1] = col.g;
	g_PixelData[_loc + 2] = col.b;
}

/* Test */

void WritePixelData()
{
	for (uint i = 0; i < g_Width; i++)
	{
		for (uint j = 0; j < g_Height; j++)
		{
			int dist = glm::distance(glm::vec2(i, j), glm::vec2(400, 300));
			dist = 255 - dist;
			dist = glm::clamp(dist, 0, 255);

			byte _dist = static_cast<byte>(dist);
			PutPixel({ i, j }, { _dist, _dist, _dist });
		}
	}
}

/* Render Method */
void DoRenderLoop()
{
	while (!glfwWindowShouldClose(g_App.GetWindow()))
	{
		glViewport(0, 0, g_Width, g_Height);

		g_App.OnUpdate();
		Render();
		g_App.FinishFrame();

		BufferTextureData();
	}
}

int main()
{
	g_App.Initialize();
	InitializeForRender();

	CreateRenderTexture();
	WritePixelData();

	DoRenderLoop();
	return 0;
}

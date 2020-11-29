/*
Title : A Simple CPU Side Ray Tracer
Reference : https://raytracing.github.io/books/RayTracingInOneWeekend.html
By : Samuel Wesley Rasquinha (@swr06) 
*/


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
typedef double floatp; // float precision

// Helpers
struct i8vec2
{
	uint8_t x, y;
};

struct i16vec2
{
	uint16_t x, y;
};

struct RGB
{
	byte r;
	byte g;
	byte b;
};

const uint g_Width = 1024;
const uint g_Height = 576;
GLubyte* const g_PixelData = new GLubyte[g_Width * g_Height * 3];
GLuint g_Texture = 0;

std::unique_ptr<GLClasses::VertexBuffer> g_VBO;
std::unique_ptr<GLClasses::VertexArray> g_VAO;
std::unique_ptr<GLClasses::Shader> g_RenderShader;

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
			ImGui::Text("Simple Ray Tracer v01 :)");

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

void PutPixel(const glm::ivec2& loc, const RGB& col) noexcept
{
	uint _loc = (loc.x + loc.y * g_Width) * 3;
	g_PixelData[_loc + 0] = col.r;
	g_PixelData[_loc + 1] = col.g;
	g_PixelData[_loc + 2] = col.b;
}

RGB GetPixel(const glm::ivec2& loc)
{
	RGB col;
	uint _loc = (loc.x + loc.y * g_Width) * 3;

	col.r = g_PixelData[_loc + 0];
	col.g = g_PixelData[_loc + 1];
	col.b = g_PixelData[_loc + 2];

	return col;
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

/* Ray Tracing and Rendering Stuff Begins Here */

// Ray Tracing Constants

const glm::vec3 g_Origin = glm::vec3(0.0f);
const float g_AspectRatio = 16.0f / 9.0f; // Window aspect ratio. Easier to keep it as 16:9
const float g_FocalLength = 1.0f;

// Viewport stuff
const float g_ViewportHeight = 2.0f;
const float g_ViewportWidth = g_ViewportHeight * g_AspectRatio;

/* Helper Classes */

class Ray
{
public:

	Ray(const glm::vec3& origin, const glm::vec3& direction) :
		m_Origin(origin), m_Direction(direction) 
	{
		//
	}

	const glm::vec3& GetOrigin() const noexcept
	{
		return m_Origin;
	}

	const glm::vec3& GetDirection() const noexcept
	{
		return m_Direction;
	}

	glm::vec3 GetAt(floatp scale) noexcept
	{
		return m_Origin + (m_Direction * glm::vec3(scale));
	}

private:

	glm::vec3 m_Origin;
	glm::vec3 m_Direction;
};

/* Functions */

RGB ToRGB(const glm::ivec3& v)
{
	RGB rgb;
	glm::ivec3 val = v;

	val.r = glm::clamp(val.r, 0, 255);
	val.g = glm::clamp(val.g, 0, 255);
	val.b = glm::clamp(val.b, 0, 255);

	rgb.r = static_cast<byte>(val.r);
	rgb.g = static_cast<byte>(val.g);
	rgb.b = static_cast<byte>(val.b);

	return rgb;
}

glm::vec3 Lerp(const glm::vec3& v1, const glm::vec3& v2, float t)
{
	return (1.0f - t) * v1 + t * v2;
}

RGB GetGradientColorAtRay(const Ray& ray)
{
	glm::vec3 ray_direction = ray.GetDirection();
	glm::vec3 v = Lerp(glm::vec3(255.0f, 255.0f, 255.0f), glm::vec3(128.0f, 178.0f, 255.0f), ray_direction.y);

	return ToRGB(glm::ivec3(v));
}

RGB GetRayColor(const Ray& ray)
{
	return GetGradientColorAtRay(ray);

}

void TraceScene()
{
	const glm::vec3 Horizontal = glm::vec3(g_ViewportWidth, 0.0f, 0.0f);
	const glm::vec3 Vertical = glm::vec3(0.0f, g_ViewportHeight, 0.0f);

	for (int i = 1; i < g_Width; i++)
	{
		for (int j = 1; j < g_Height; j++)
		{
			// Calculate the UV Coordinates
			float u = (float)i / (float)g_Width;
			float v = (float)j / (float)g_Height;

			Ray ray(g_Origin, (Horizontal * u) + (v * Vertical) - g_Origin);
			PutPixel(glm::ivec2(i, j), GetRayColor(ray));
		}
	}
}

void WritePixelData()
{
	std::cout << std::endl << "Writing Pixel Data.." << std::endl;
	std::cout << "Ray Tracing.." << std::endl;
	TraceScene();
	std::cout << "Done!";
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

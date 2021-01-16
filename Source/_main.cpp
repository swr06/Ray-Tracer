#include <stdio.h>
#include <iostream>
#include <array>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <chrono> 
#include <memory>

#include <glad/glad.h>          
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "Core/Application.h"
#include "Core/VertexBuffer.h"
#include "Core/VertexArray.h"
#include "Core/Shader.h"
#include "Core/CubeTextureMap.h"
#include "Core/Framebuffer.h"

using namespace RayTracer;

const int g_Width = 1024;
const int g_Height = 576;
int g_SPP = 1;
bool g_Denoise = false;
std::unique_ptr<GLClasses::Shader> _TraceShader;
std::pair<std::string, std::string> TraceShaderPaths = { "Core/Shaders/RayTraceVert.glsl", "Core/Shaders/RayTraceFrag.glsl" }; ;

inline float RandomFloat()
{
	static std::uniform_real_distribution<float> distribution(0.0, 1.0);
	static std::mt19937 generator;
	return distribution(generator);
}

inline float RandomFloat(float min, float max)
{
	return min + (max - min) * RandomFloat();
}

glm::vec3 Lerp(const glm::vec3& v1, const glm::vec3& v2, float t)
{
	return (1.0f - t) * v1 + t * v2;
}

glm::vec3 ConvertTo0_1Range(const glm::vec3& v)
{
	return 0.5f * (v + 1.0f);
}

void DisplayFrameRate(GLFWwindow* pWindow, const std::string& title)
{
	static double lastTime = 0;
	static float nbFrames = 0;
	double currentTime = glfwGetTime();
	double delta = currentTime - lastTime;
	nbFrames++;
	if (delta >= 1.0)
	{
		double fps = double(nbFrames) / delta;

		std::stringstream ss;
		ss << title << " [" << fps << " FPS]";

		glfwSetWindowTitle(pWindow, ss.str().c_str());

		nbFrames = 0;
		lastTime = currentTime;
	}
}

// Camera

class Camera
{
public:

	Camera(const glm::vec3& origin, const glm::vec3& lookat, const glm::vec3& up,
		float fov) : m_FOV(fov)
	{
		m_Origin = origin;
		m_LookAt = lookat;
		m_Up = up;
		m_FOV = fov;

		Update();
	}

	void Update()
	{
		float theta = glm::radians(m_FOV);
		float H = glm::tan(theta / 2.0f);

		m_ViewportHeight = 2.0f * H;
		m_ViewportWidth = m_ViewportHeight * m_AspectRatio;

		glm::vec3 w = glm::normalize(m_Origin - m_LookAt);
		glm::vec3 u = glm::normalize(glm::cross(m_Up, w));
		glm::vec3 v = glm::cross(w, u);

		m_Horizontal = m_ViewportWidth * u;
		m_Vertical = m_ViewportHeight * v;
		m_BottomLeft = m_Origin - (m_Horizontal / 2.0f) - (m_Vertical / 2.0f) - w;
	}

	void UpdateOnMouseMovement(double xpos, double ypos)
	{
		if (FirstMove == true)
		{
			FirstMove = false;
			m_PrevMx = xpos;
			m_PrevMy = ypos;
		}

		float sensitivity = 0.2f;
		ypos = -ypos;

		float x_diff = xpos - m_PrevMx;
		float y_diff = ypos - m_PrevMy;

		x_diff = x_diff * sensitivity;
		y_diff = y_diff * sensitivity;

		m_PrevMx = xpos;
		m_PrevMy = ypos;

		m_Yaw = m_Yaw + x_diff;
		m_Pitch = m_Pitch + y_diff;

		if (m_Pitch > 89.0f)
		{
			m_Pitch = 89.0f;
		}

		if (m_Pitch < -89.0f)
		{
			m_Pitch = -89.0f;
		}

		glm::vec3 front;

		front.x = cos(glm::radians(m_Pitch)) * cos(glm::radians(m_Yaw));
		front.y = sin(glm::radians(m_Pitch));
		front.z = cos(glm::radians(m_Pitch)) * sin(glm::radians(m_Yaw));

		m_LookAt = front;
	}

	glm::vec3 GetRight() const noexcept
	{
		return glm::normalize(glm::cross(m_LookAt, m_Up));
	}

	glm::vec3 m_LookAt;
	glm::vec3 m_Up;
	float m_FOV;

	glm::vec3 m_Origin = glm::vec3(0.0f);
	const float m_AspectRatio = 16.0f / 9.0f; // Window aspect ratio. Easier to keep it as 16:9
	const float m_FocalLength = 1.0f;

	// Viewport stuff
	float m_ViewportHeight;
	float m_ViewportWidth;
	glm::vec3 m_Horizontal;
	glm::vec3 m_Vertical;
	glm::vec3 m_BottomLeft;

	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;
	double m_PrevMx = 0.0f;
	double m_PrevMy = 0.0f;
	bool FirstMove = true;
};

Camera g_SceneCamera(glm::vec3(0.0f),
	glm::vec3(0.0f, 0.0f, -1.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	90.0f);

class RayTracerApp : public Application
{
public:

	RayTracerApp()
	{
		m_Width = g_Width;
		m_Height = g_Height;
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
		if (e.type == EventTypes::KeyPress && e.key == GLFW_KEY_F1)
		{
			_TraceShader->Destroy();
			_TraceShader->CreateShaderProgramFromFile(TraceShaderPaths.first, TraceShaderPaths.second);
			_TraceShader->CompileShaders();
		}

		if (e.type == EventTypes::KeyPress && e.key == GLFW_KEY_F2)
		{
			g_Denoise = !g_Denoise;
		}

		if (e.type == EventTypes::KeyPress && e.key == GLFW_KEY_F5)
		{
			if (g_SPP < 8)
			{
				g_SPP++;
			}
		}

		if (e.type == EventTypes::KeyPress && e.key == GLFW_KEY_F6)
		{
			if (g_SPP > 1)
			{
				g_SPP--;
			}
		}


		if (e.type == EventTypes::MouseMove)
		{
			g_SceneCamera.UpdateOnMouseMovement(e.mx, e.my);
			g_SceneCamera.Update();
		}
	}

};

enum class Material : int
{
	Invalid = -1,
	Glass,
	Diffuse,
	Metal
};

class Sphere
{
public:

	glm::vec3 Center;
	glm::vec3 Color;
	float Radius;
	Material SphereMaterial;
	float FuzzLevel;

	Sphere(const glm::vec3& center, const glm::vec3& color, float radius, Material mat, float fuzz = 0.0f) :
		Center(center),
		Color(color),
		Radius(radius),
		SphereMaterial(mat),
		FuzzLevel(fuzz)
	{

	}

	Sphere() :
		Center(glm::vec3(0.0f)),
		Color(glm::vec3(0.0f)),
		Radius(0.0f),
		SphereMaterial(Material::Invalid),
		FuzzLevel(0.0f)
	{

	}
};

RayTracerApp g_App;

std::vector<Sphere> SceneSpheres =
{
	Sphere(glm::vec3(-1.0, 0.0, -1.0), glm::vec3(0.8f, 0.6f, 0.2f), 0.5f, Material::Metal, 0.7f),
	Sphere(glm::vec3(0.0, 0.0, -1.0), glm::vec3(1, 0, 0), 0.5f, Material::Diffuse),
	Sphere(glm::vec3(1.0, 0.0, -1.0), glm::vec3(0.8f), 0.5f, Material::Metal, 0.0f),
	Sphere(glm::vec3(0.0f, -100.5f, -1.0f), glm::vec3(0, 1, 0), 100.0f, Material::Diffuse)
};

inline void SetSphereUniform(const std::string& name, const Sphere& sphere, GLClasses::Shader& shader)
{
	shader.SetVector3f(name + std::string(".Center"), sphere.Center, 0);
	shader.SetVector3f(name + std::string(".Color"), sphere.Color, 0);
	shader.SetFloat(name + std::string(".Radius"), sphere.Radius, 0);
	shader.SetFloat(name + std::string(".FuzzLevel"), sphere.FuzzLevel, 0);
	shader.SetInteger(name + std::string(".Material"), static_cast<int>(sphere.SphereMaterial), 0);
}

void SetSceneSphereUniforms(GLClasses::Shader& shader)
{
	const std::string name = "u_SceneSpheres";

	shader.SetInteger("u_SceneSphereCount", SceneSpheres.size());

	for (int i = 0 ; i < SceneSpheres.size() ; i++)
	{
		std::string uniform_name = name + "[" + std::to_string(i) + "]";
		SetSphereUniform(uniform_name, SceneSpheres[i], shader);
	}
}

int main()
{
	long long m_CurrentFrame = 1;

	g_App.Initialize();
	g_App.SetCursorLocked(true);

	std::vector<std::string> paths = 
	{
		"Res/right.bmp",
		"Res/left.bmp",
		"Res/top.bmp",
		"Res/bottom.bmp",
		"Res/front.bmp",
		"Res/back.bmp"
	};

	GLClasses::Shader DenoiseShader;
	DenoiseShader.CreateShaderProgramFromFile("Core/Shaders/DenoiseVert.glsl", "Core/Shaders/DenoiseFrag.glsl");
	DenoiseShader.CompileShaders();

	GLClasses::Framebuffer Accumulation_Buffer(g_Width, g_Height, false);
	GLClasses::Framebuffer FBO_1(g_Width, g_Height, false);
	GLClasses::Framebuffer FBO_2(g_Width, g_Height, false);

	_TraceShader = std::unique_ptr<GLClasses::Shader>(new GLClasses::Shader);
	GLClasses::Shader& TraceShader = *_TraceShader;

	GLClasses::VertexBuffer VBO;
	GLClasses::VertexArray VAO;
	GLClasses::CubeTextureMap SKYBOX;

	SKYBOX.CreateCubeTextureMap(paths, false);

	unsigned long long CurrentFrame = 0;

	float Vertices[] =
	{
		-1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f, -1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f
	};

	VAO.Bind();
	VBO.Bind();
	VBO.BufferData(sizeof(Vertices), Vertices, GL_STATIC_DRAW);
	VBO.VertexAttribPointer(0, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), 0);
	VBO.VertexAttribPointer(1, 2, GL_FLOAT, 0, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
	VAO.Unbind();

	TraceShader.CreateShaderProgramFromFile(TraceShaderPaths.first, TraceShaderPaths.second);
	TraceShader.CompileShaders();

	while (!glfwWindowShouldClose(g_App.GetWindow()))
	{
		int FBO_Used = 0;

		g_App.OnUpdate();

		// Bind the approprite FBO
		if (m_CurrentFrame % 2 == 0)
		{
			FBO_2.Bind();
			FBO_Used = 2;
		}

		else
		{
			FBO_1.Bind();
			FBO_Used = 1;
		}


		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

		glViewport(0, 0, g_Width, g_Height);

		// Render
		TraceShader.Use();

		TraceShader.SetFloat("u_Time", glfwGetTime());
		TraceShader.SetVector2f("u_ViewportDimensions", glm::vec2(g_Width, g_Height));
		TraceShader.SetVector3f("u_CameraBottomLeft", g_SceneCamera.m_BottomLeft);
		TraceShader.SetVector3f("u_CameraHorizontal", g_SceneCamera.m_Horizontal);
		TraceShader.SetVector3f("u_CameraVertical", g_SceneCamera.m_Vertical);
		TraceShader.SetVector3f("u_CameraOrigin", g_SceneCamera.m_Origin);
		TraceShader.SetInteger("u_Skybox", 0);
		TraceShader.SetInteger("SAMPLES_PER_PIXEL", g_SPP);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, SKYBOX.GetID());

		SetSceneSphereUniforms(TraceShader);

		VAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		VAO.Unbind();

		// Finish the render
		DenoiseShader.Use();
		Accumulation_Buffer.Bind();

		DenoiseShader.SetInteger("u_CurrentFrame", 0);
		DenoiseShader.SetInteger("u_PreviousFrame", 1);
		DenoiseShader.SetBool("u_Denoise", g_Denoise);

		if (FBO_Used == 2)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, FBO_2.GetTexture());

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, FBO_1.GetTexture());
		}

		else
		{
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, FBO_2.GetTexture());

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, FBO_1.GetTexture());
		}

		VAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		VAO.Unbind();

		// Blit the accumulation buffer onto the screen
		Accumulation_Buffer.Bind();
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, Accumulation_Buffer.GetWidth(), Accumulation_Buffer.GetHeight(), 
			0, 0, g_Width, g_Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Blit the accumulation buffer to the FBO used

		Accumulation_Buffer.Bind();

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		if (FBO_Used == 1)
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_1.GetFramebuffer());
		}

		else
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_2.GetFramebuffer());
		}

		glBlitFramebuffer(0, 0, Accumulation_Buffer.GetWidth(), Accumulation_Buffer.GetHeight(),
			0, 0, g_Width, g_Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);


		DisplayFrameRate(g_App.GetWindow(), "Raytracer!");
		g_App.FinishFrame();
		g_SceneCamera.Update();

		m_CurrentFrame++;
		CurrentFrame++;
	}

	return 0;
}
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

using namespace RayTracer;

const int g_Width = 1024;
const int g_Height = 576;

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

	}

};

inline bool PointIsInSphere(const glm::vec3& point, float radius)
{
	return ((point.x * point.x) + (point.y * point.y) + (point.z * point.z)) < (radius * radius);
}

inline glm::vec3 GeneratePointInUnitSphere()
{
	glm::vec3 ReturnVal;

	ReturnVal.x = RandomFloat(-1.0f, 1.0f);
	ReturnVal.y = RandomFloat(-1.0f, 1.0f);
	ReturnVal.z = RandomFloat(-1.0f, 1.0f);

	while (!PointIsInSphere(ReturnVal, 1.0f))
	{
		ReturnVal.x = RandomFloat(-1.0f, 1.0f);
		ReturnVal.y = RandomFloat(-1.0f, 1.0f);
		ReturnVal.z = RandomFloat(-1.0f, 1.0f);
	}

	return ReturnVal;
}

class Camera
{
public:

	Camera(const glm::vec3& lookfrom, const glm::vec3& lookat, const glm::vec3& up,
		float fov) : m_FOV(fov)
	{
		float theta = glm::radians(fov);
		float H = glm::tan(theta / 2.0f);

		m_ViewportHeight = 2.0f * H;
		m_ViewportWidth = m_ViewportHeight * m_AspectRatio;

		glm::vec3 w = glm::normalize(lookfrom - lookat);
		glm::vec3 u = glm::normalize(glm::cross(up, w));
		glm::vec3 v = glm::cross(w, u);

		m_Origin = lookfrom;
		m_Horizontal = m_ViewportWidth * u;
		m_Vertical = m_ViewportHeight * v;
		m_BottomLeft = m_Origin - (m_Horizontal / 2.0f) - (m_Vertical / 2.0f) - w;
	}

	glm::vec3 m_Origin = glm::vec3(0.0f);
	const float m_AspectRatio = 16.0f / 9.0f; // Window aspect ratio. Easier to keep it as 16:9
	const float m_FocalLength = 1.0f;

	// Viewport stuff
	float m_ViewportHeight;
	float m_ViewportWidth;
	glm::vec3 m_Horizontal;
	glm::vec3 m_Vertical;
	glm::vec3 m_BottomLeft;

	float m_FOV;
};

enum class Material
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
Camera g_SceneCamera(glm::vec3(0.0f),
	glm::vec3(0.0f, 0.0f, -1.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	90.0f);

std::vector<Sphere> SceneSpheres =
{
	Sphere(glm::vec3(-1.0, 0.0, -1.0), glm::vec3(255, 0, 0), 0.5f, Material::Diffuse, 0.65f),
	Sphere(glm::vec3(0.0, 0.0, -1.0), glm::vec3(0, 255, 0), 0.5f, Material::Diffuse),
	Sphere(glm::vec3(1.0, 0.0, -1.0), glm::vec3(0, 0, 255), 0.5f, Material::Diffuse, 0.0f),
	Sphere(glm::vec3(0.0f, -100.5f, -1.0f), glm::vec3(255, 0, 0), 100.0f, Material::Diffuse)
};

inline void SetSphereUniform(const std::string& name, const Sphere& sphere, GLClasses::Shader& shader)
{
	shader.SetVector3f(name + std::string(".Center"), sphere.Center, 0);
	shader.SetVector3f(name + std::string(".Color"), sphere.Color, 0);
	shader.SetFloat(name + std::string(".Radius"), sphere.Radius, 0);
	shader.SetFloat(name + std::string(".FuzzLevel"), sphere.FuzzLevel, 0);
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
	g_App.Initialize();

	GLClasses::VertexBuffer VBO;
	GLClasses::VertexArray VAO;
	GLClasses::Shader TraceShader;
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

	TraceShader.CreateShaderProgramFromFile("Core/Shaders/RayTraceVert.glsl", "Core/Shaders/RayTraceFrag.glsl");
	TraceShader.CompileShaders();

	while (!glfwWindowShouldClose(g_App.GetWindow()))
	{
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, g_Width, g_Height);

		g_App.OnUpdate();

		// Render
		TraceShader.Use();

		TraceShader.SetVector2f("u_ViewportDimensions", glm::vec2(g_Width, g_Height));
		TraceShader.SetVector3f("u_CameraBottomLeft", g_SceneCamera.m_BottomLeft);
		TraceShader.SetVector3f("u_CameraHorizontal", g_SceneCamera.m_Horizontal);
		TraceShader.SetVector3f("u_CameraVertical", g_SceneCamera.m_Vertical);
		TraceShader.SetVector3f("u_CameraOrigin", g_SceneCamera.m_Origin);
		SetSceneSphereUniforms(TraceShader);

		VAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 6);
		VAO.Unbind();

		g_App.FinishFrame();

		CurrentFrame++;
		DisplayFrameRate(g_App.GetWindow(), "Raytracer!");
	}

	return 0;
}
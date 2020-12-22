﻿/*
Title : A Simple CPU Side Ray Tracer
Reference : https://raytracing.github.io/books/RayTracingInOneWeekend.html
By : Samuel Wesley Rasquinha (@swr06) 
*/


#include <stdio.h>
#include <iostream>
#include <array>
#include <random>
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

class RGB
{
	public :
	
	RGB(byte R, byte G, byte B)
	{
		r = R;
		g = G;
		b = B;
	}

	RGB() : r(0), g(0), b(0)
	{}

	byte r;
	byte g;
	byte b;
};

struct RayHitRecord
{
	glm::vec3 Point;
	glm::vec3 Normal;
	float T; 
};

// Needs to be 16:9 aspect ratio
// 1024, 576
const uint g_Width = 1024;
const uint g_Height = 576;
GLubyte* const g_PixelData = new GLubyte[g_Width * g_Height * 3];
GLuint g_Texture = 0;

std::unique_ptr<GLClasses::VertexBuffer> g_VBO;
std::unique_ptr<GLClasses::VertexArray> g_VAO;
std::unique_ptr<GLClasses::Shader> g_RenderShader;

// Utility 
const double _INFINITY = std::numeric_limits<double>::infinity();
const double C_PI = 3.14159265354;

inline double RandomFloat() 
{
	static std::uniform_real_distribution<double> distribution(0.0, 1.0);
	static std::mt19937 generator;
	return distribution(generator);
}

inline float RandomFloat(float min, float max)
{
	return min + (max - min) * RandomFloat();
}

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

RGB ToRGBVec3_01(const glm::vec3& v)
{
	glm::vec3 val = v;
	RGB rgb;

	val.x = v.x * 255.0f;
	val.y = v.y * 255.0f;
	val.z = v.z * 255.0f;

	glm::ivec3 _col = val;

	_col.r = glm::clamp(_col.r, 0, 255);
	_col.g = glm::clamp(_col.g, 0, 255);
	_col.b = glm::clamp(_col.b, 0, 255);

	rgb.r = static_cast<byte>(_col.r);
	rgb.g = static_cast<byte>(_col.g);
	rgb.b = static_cast<byte>(_col.b);

	return rgb;
}

glm::vec3 Lerp(const glm::vec3& v1, const glm::vec3& v2, float t)
{
	return (1.0f - t) * v1 + t * v2;
}

glm::vec3 ConvertTo0_1Range(const glm::vec3& v)
{
	return 0.5f * (v + 1.0f);
}

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
	BufferTextureData();
	
	while (!glfwWindowShouldClose(g_App.GetWindow()))
	{
		glViewport(0, 0, g_Width, g_Height);

		g_App.OnUpdate();
		Render();
		g_App.FinishFrame();
	}
}

/* Ray Tracing and Rendering Stuff Begins Here */

// Ray Tracing Constants

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

	glm::vec3 GetAt(floatp scale) const noexcept
	{
		return m_Origin + (m_Direction * glm::vec3(scale));
	}

private:

	glm::vec3 m_Origin;
	glm::vec3 m_Direction;
};

struct Sphere
{
	glm::vec3 Center;
	float Radius;
};

inline bool PointIsInSphere(const glm::vec3& point, float radius)
{
	return ((point.x * point.x) + (point.y * point.y) + (point.z * point.z)) < radius;
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
public :

	Ray GetRay(float u, float v)
	{
		const glm::vec3 Horizontal = glm::vec3(m_ViewportWidth, 0.0f, 0.0f);
		const glm::vec3 Vertical = glm::vec3(0.0f, m_ViewportHeight, 0.0f);

		auto BottomLeft = m_Origin - Horizontal / 2.0f - Vertical / 2.0f - glm::vec3(0.0f, 0.0f, m_FocalLength);

		Ray ray(m_Origin, BottomLeft + (Horizontal * u) + (v * Vertical) - m_Origin);
		return ray;
	}

private :
	const glm::vec3 m_Origin = glm::vec3(0.0f);
	const float m_AspectRatio = 16.0f / 9.0f; // Window aspect ratio. Easier to keep it as 16:9
	const float m_FocalLength = 1.0f;

	// Viewport stuff
	const float m_ViewportHeight = 2.0f;
	const float m_ViewportWidth = m_ViewportHeight * m_AspectRatio;
};

RGB GetGradientColorAtRay(const Ray& ray)
{
	glm::vec3 ray_direction = ray.GetDirection();
	glm::vec3 v = Lerp(glm::vec3(255.0f, 255.0f, 255.0f), glm::vec3(128.0f, 178.0f, 255.0f), ray_direction.y * 2.0f);

	return ToRGB(glm::ivec3(v));
}

bool RaySphereIntersectionTest(const Sphere& sphere, const Ray& ray, float tmin, float tmax, RayHitRecord& hit_record) 
{
	// p(t) = t²b⋅b+2tb⋅(A−C)+(A−C)⋅(A−C)−r² = 0
	// The discriminant of this equation tells us the number of possible solutions
	// we calculate that discriminant of the equation 

	glm::vec3 oc = ray.GetOrigin() - sphere.Center;
	float A = glm::dot(ray.GetDirection(), ray.GetDirection());
	float B = 2.0 * glm::dot(oc, ray.GetDirection());
	float C = dot(oc, oc) - sphere.Radius * sphere.Radius;
	float Discriminant = B * B - 4 * A * C;
	
	if (Discriminant < 0)
	{
		return false;
	}

	else
	{
		// Solve the quadratic equation and
		// find t (T is the distance from the ray origin to the center of the sphere)
		float root = (-B - sqrt(Discriminant)) / (2.0f * A); // T

		if (root < tmin || root > tmax)
		{
			root = (-B + sqrt(Discriminant)) / (2.0f * A);

			if (root < tmin || root > tmax)
			{
				return false;
			}
		}

		// The root was found successfully 
		hit_record.T = root;
		hit_record.Point = ray.GetAt(root);

		// TODO ! : CHECK THIS! 
		// SHOULD THE RADIUS BE MULTIPLIED HERE?
		hit_record.Normal = (ray.GetAt(root) - sphere.Center);
		
		return true;
	}
}

Sphere sphere_0 = { glm::vec3(0, 0, -1), 0.5f };

std::vector<Sphere> Spheres = { sphere_0,
	{glm::vec3(0.0f, -100.5f, -1.0f), 100.0f}
};

bool IntersectSceneSpheres(const Ray& ray, float tmin, float tmax, RayHitRecord& closest_hit_rec)
{
	RayHitRecord TempRecord;
	bool HitAnything = false;
	float ClosestDistance = tmax;

	for (auto& e : Spheres)
	{
		// T is the distance of ray origin to the sphere's center
		// RaySphereIntersectionTest(e, ray, 0.0f, _INFINITY);

		if (RaySphereIntersectionTest(e, ray, tmin, ClosestDistance, TempRecord))
		{
			HitAnything = true;
			ClosestDistance = TempRecord.T;
			closest_hit_rec = TempRecord;
		}
	}

	return HitAnything;
}

RGB GetRayColor(const Ray& ray, int ray_depth)
{
	if (ray_depth <= 0)
	{
		return RGB(0, 0, 0);
	}

	RayHitRecord ClosestSphere;

	if (IntersectSceneSpheres(ray, 0, _INFINITY, ClosestSphere))
	{
		glm::vec3 S = ClosestSphere.Point + ClosestSphere.Normal + GeneratePointInUnitSphere();
		Ray new_ray(ClosestSphere.Point, S - ClosestSphere.Point);

		RGB Ray_Color = GetRayColor(new_ray, ray_depth - 1);
		//Ray_Color.r /= 4;
		//Ray_Color.g /= 4;
		//Ray_Color.b /= 4;

		return Ray_Color;
	}

	return GetGradientColorAtRay(ray);
}

Camera g_SceneCamera; // Origin = 0,0,0. Faces the negative Z axis

void TraceScene()
{
	auto start_time = glfwGetTime();
	const int SPP = 200;

	for (int i = 0; i < g_Width; i++)
	{
		std::cout << "Rows Remaining : " << g_Width - i << "\n";

		for (int j = 0; j < g_Height; j++)
		{
			glm::ivec3 FinalColor;

			for (int s = 0; s < SPP; s++)
			{
				// Calculate the UV Coordinates

				float u = ((float)i + RandomFloat()) / (float)g_Width;
				float v = ((float)j + RandomFloat()) / (float)g_Height;

				Ray ray = g_SceneCamera.GetRay(u, v);
				RGB ray_color = GetRayColor(ray, 50);

				FinalColor.r += ray_color.r;
				FinalColor.g += ray_color.g;
				FinalColor.b += ray_color.b;
			}

			FinalColor /= SPP;
			PutPixel(glm::ivec2(i, j), ToRGB(FinalColor));

			//PutPixel(glm::ivec2(i, j), GetRayColor(g_SceneCamera.GetRay(u, v), 50));
		}
	}

	auto end_time = glfwGetTime();
	auto total_time = end_time - start_time;

	std::cout << "Tracing Finished in " << total_time << " seconds.." << std::endl;
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

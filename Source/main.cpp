/*
Title : A Simple CPU Side Ray Tracer
Reference : https://raytracing.github.io/books/RayTracingInOneWeekend.html
By : Samuel Wesley Rasquinha (@swr06) 
*/

#define THREAD_SPAWN_COUNT 4

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

	inline glm::vec3 ToVec3() const noexcept
	{
		glm::vec3 vec;
		vec.r = static_cast<float>(r);
		vec.g = static_cast<float>(g);
		vec.b = static_cast<float>(b);

		return vec;
	}

	byte r;
	byte g;
	byte b;
};

struct RayHitRecord
{
	glm::vec3 Point;
	glm::vec3 Normal;
	float T = 0.0f; 
	bool Inside = false;
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
const double PI = 3.14159265354;

inline double RandomFloat() 
{
	static std::uniform_real_distribution<float> distribution(0.0, 1.0);
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

RGB ToRGB(const glm::vec3& v)
{
	glm::vec3 val = v;
	RGB rgb;

	glm::ivec3 _col = val;

	_col.r = glm::clamp(_col.r, 0, 255);
	_col.g = glm::clamp(_col.g, 0, 255);
	_col.b = glm::clamp(_col.b, 0, 255);

	rgb.r = static_cast<byte>(_col.r);
	rgb.g = static_cast<byte>(_col.g);
	rgb.b = static_cast<byte>(_col.b);

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

inline void PutPixel(const glm::ivec2& loc, const RGB& col) noexcept
{
	if (loc.x >= g_Width || loc.y >= g_Height) { return; }

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
	static unsigned long long m_CurrentFrame = 0;

	while (!glfwWindowShouldClose(g_App.GetWindow()))
	{
		if (m_CurrentFrame % 15 == 0)
		{
			BufferTextureData();
		}

		glViewport(0, 0, g_Width, g_Height);

		g_App.OnUpdate();
		Render();
		g_App.FinishFrame();

		m_CurrentFrame++;
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

enum class Material
{
	Glass = 0,
	Diffuse,
	Metal,
	FuzzyMetal
};

class Sphere
{
public :

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
		Color(glm::vec3(1.0f)),
		Radius(0.0f),
		SphereMaterial(Material::Diffuse),
		FuzzLevel(0.0f)
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

		auto w = glm::normalize(lookfrom - lookat);
		auto u = glm::normalize(glm::cross(up, w));
		auto v = glm::cross(w, u);

		m_Origin = lookfrom;
		m_Horizontal = m_ViewportWidth * u;
		m_Vertical = m_ViewportHeight * v;
		m_BottomLeft = m_Origin - (m_Horizontal / 2.0f) - (m_Vertical / 2.0f) - w;

	}

	inline Ray GetRay(float u, float v) const 
	{
		Ray ray(m_Origin, m_BottomLeft + (m_Horizontal * u) + (v * m_Vertical) - m_Origin);
		return ray;
	}

private :
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

inline RGB GetGradientColorAtRay(const Ray& ray)
{
	const glm::vec3 ray_direction = ray.GetDirection();
	glm::vec3 v = Lerp(glm::vec3(255.0f, 255.0f, 255.0f), glm::vec3(128.0f, 178.0f, 255.0f), ray_direction.y * 1.8f);

	return ToRGB(glm::ivec3(v));
}

inline bool RaySphereIntersectionTest(const Sphere& sphere, const Ray& ray, float tmin, float tmax, RayHitRecord& hit_record) 
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
		float root = (-B - glm::sqrt(Discriminant)) / (2.0f * A); // T

		if (root < tmin || root > tmax)
		{
			root = (-B + glm::sqrt(Discriminant)) / (2.0f * A);

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
		hit_record.Normal = (hit_record.Point - sphere.Center) / sphere.Radius;
		
		if (glm::dot(ray.GetDirection(), hit_record.Normal) > 0.0f)
		{
			hit_record.Normal = -hit_record.Normal;
			hit_record.Inside = true;
		}
		
		return true;
	}
}

std::vector<Sphere> Spheres = 
{ 
	Sphere(glm::vec3(-1.0, 0.0, -1.0), glm::vec3(255, 0, 0), 0.5f, Material::Diffuse, 0.5f),
	Sphere(glm::vec3(0.0, 0.0, -1.0), glm::vec3(0, 255, 0), 0.5f, Material::Diffuse),
	Sphere(glm::vec3(1.0, 0.0, -1.0), glm::vec3(0, 0, 255), 0.5f, Material::Diffuse, 0.0f),
	Sphere(glm::vec3(0.0f, -100.5f, -1.0f), glm::vec3(0, 255, 0), 100.0f, Material::Diffuse)
};

bool IntersectSceneSpheres(const Ray& ray, float tmin, float tmax, RayHitRecord& closest_hit_rec, Sphere& sphere)
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
			sphere = e;
		}
	}

	return HitAnything;
}

RGB GetRayColor(const Ray& ray, int ray_depth)
{
	Sphere hit_sphere;
	RayHitRecord closest_record;

	Ray new_ray = ray;
	glm::vec3 total_color = GetGradientColorAtRay(ray).ToVec3();
	int hit_count = 0;
	std::vector<glm::vec3> diffuse_colors;

	while (hit_count < 6)
	{
		if (IntersectSceneSpheres(new_ray, 0.001f, _INFINITY, closest_record, hit_sphere))
		{
			glm::vec3 S = closest_record.Normal + GeneratePointInUnitSphere();
			new_ray = Ray(closest_record.Point, S);

			glm::vec3 Color = hit_sphere.Color;
			glm::vec3 TotalColorUntilNow = glm::vec3(1.0);

			for (int i = 0; i < diffuse_colors.size(); i++)
			{
				TotalColorUntilNow *= diffuse_colors[i] * 0.5f;
			}

			total_color = (Color * 0.5f) * TotalColorUntilNow;
			diffuse_colors.push_back(Color);
		}

		hit_count++;
	}

	return ToRGB(total_color);
}

/*Camera g_SceneCamera(glm::vec3(-2.0f, 2.0f, 1.0f),
	glm::vec3(0.0f, 0.0f, -1.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	90.0f);*/ 

Camera g_SceneCamera(glm::vec3(0.0f),
	glm::vec3(0.0f, 0.0f, -1.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	90.0f);

const int SPP = 20;
const int RAY_DEPTH = 5;

void TraceThreadFunction(int xstart, int ystart, int xsize, int ysize)
{
	for (int i = xstart; i < xstart + xsize; i++)
	{
		//std::this_thread::sleep_for(std::chrono::microseconds(8));

		for (int j = ystart; j < ystart + ysize; j++)
		{
			glm::ivec3 FinalColor;

			for (int s = 0; s < SPP; s++)
			{
				// Calculate the UV Coordinates

				float u = ((float)i + RandomFloat()) / (float)g_Width;
				float v = ((float)j + RandomFloat()) / (float)g_Height;

				Ray ray = g_SceneCamera.GetRay(u, v);
				RGB ray_color = GetRayColor(ray, RAY_DEPTH);

				FinalColor.r += ray_color.r;
				FinalColor.g += ray_color.g;
				FinalColor.b += ray_color.b;
			}
			
			FinalColor.r = (FinalColor.r / SPP);
			FinalColor.g = (FinalColor.g / SPP);
			FinalColor.b = (FinalColor.b / SPP);

			PutPixel(glm::ivec2(i, j), ToRGB(FinalColor));
		}
	}
}

void TraceScene()
{
	const int sizex = g_Width / THREAD_SPAWN_COUNT;
	const int sizey = g_Height;

	for (int t = 0; t < THREAD_SPAWN_COUNT; t++)
	{
		std::thread thread(TraceThreadFunction, sizex * t, 0, sizex, sizey);
		thread.detach();

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

void WritePixelData()
{
	std::cout << std::endl << "Writing Pixel Data.." << std::endl;
	std::cout << "Ray Tracing.." << std::endl;
	TraceScene();
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

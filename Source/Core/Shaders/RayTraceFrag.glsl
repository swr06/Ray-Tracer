#version 330 core

#define MAX_RAY_HIT_DISTANCE 100000.0f
#define PI 3.14159265354f
#define RAY_BOUNCE_LIMIT 50
#define SAMPLES_PER_PIXEL 100
#define MAX_SPHERES 25

out vec3 o_Color;

in vec2 v_TexCoords;

// Structures

// Ray
struct Ray
{
	vec3 Origin;
	vec3 Direction;
};

vec3 Ray_GetAt(Ray ray, float scale)
{
	return ray.Origin + (ray.Direction * vec3(scale));
}

// "Enums"
const uint MATERIAL_METAL = 0x00000001u;
const uint MATERIAL_DIFFUSE = 0x00000002u;
const uint MATERIAL_INVALID = 0x00000004u;

// Sphere
struct Sphere
{
	vec3 Center;
	vec3 Color;
	float Radius;
	uint Material;
	float FuzzLevel;
};

// Ray hit record
struct RayHitRecord
{
	vec3 Point;
	vec3 Normal;
	float T;
	bool Inside;
	bool Hit;
};

// Uniforms
uniform vec2 u_ViewportDimensions;

// Camera related uniforms
uniform vec3 u_CameraBottomLeft;
uniform vec3 u_CameraHorizontal;
uniform vec3 u_CameraVertical;
uniform vec3 u_CameraOrigin;

// Textures
uniform sampler2D u_UnitSpherePoints; // A 32x32 floating point texture to store noise

// Spheres
uniform Sphere u_Sphere;
uniform Sphere u_SceneSpheres[MAX_SPHERES];

// Functions

vec3 lerp(vec3 v1, vec3 v2, float t);
vec3 GetGradientColorAtRay(Ray ray);

vec3 lerp(vec3 v1, vec3 v2, float t)
{
	return (1.0f - t) * v1 + t * v2;
}

vec3 GetGradientColorAtRay(Ray ray)
{
	vec3 v = lerp(vec3(255.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f),
			vec3(128.0f / 255.0f, 178.0f / 255.0f, 255.0f / 255.0f), ray.Direction.y * 1.25f);
	return v;
}

RayHitRecord RaySphereIntersectionTest(const Sphere sphere, Ray ray, float tmin, float tmax)
{
	RayHitRecord hit_record;

	vec3 oc = ray.Origin - sphere.Center;
	float A = dot(ray.Direction, ray.Direction);
	float B = 2.0 * dot(oc, ray.Direction);
	float C = dot(oc, oc) - sphere.Radius * sphere.Radius;
	float Discriminant = B * B - 4 * A * C;

	if (Discriminant < 0)
	{
		hit_record.Hit = false;
		return hit_record;
	}

	else
	{
		float root = (-B - sqrt(Discriminant)) / (2.0f * A); // T

		if (root < tmin || root > tmax)
		{
			root = (-B + sqrt(Discriminant)) / (2.0f * A);

			if (root < tmin || root > tmax)
			{
				hit_record.Hit = false;
				return hit_record;
			}
		}

		hit_record.Hit = true;

		// The root was found successfully 
		hit_record.T = root;
		hit_record.Point = Ray_GetAt(ray, root);

		hit_record.Normal = (hit_record.Point - sphere.Center) / sphere.Radius;

		if (dot(ray.Direction, hit_record.Normal) > 0.0f)
		{
			hit_record.Normal = -hit_record.Normal;
			hit_record.Inside = true;
		}

		return hit_record;
	}
}

void main()
{
	Ray ray;
	ray.Origin = u_CameraOrigin;
	ray.Direction = u_CameraBottomLeft + (u_CameraHorizontal * v_TexCoords.x) + (u_CameraVertical * v_TexCoords.y) - u_CameraOrigin;
	
	RayHitRecord record;
	record = RaySphereIntersectionTest(u_Sphere, ray, 0.0f, MAX_RAY_HIT_DISTANCE);

	o_Color = mix(GetGradientColorAtRay(ray), record.Normal, float(record.Hit));
}
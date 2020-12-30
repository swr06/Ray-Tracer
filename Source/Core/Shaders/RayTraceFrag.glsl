#version 330 core

#define MAX_RAY_HIT_DISTANCE 100000.0f
#define PI 3.14159265354f
#define RAY_BOUNCE_LIMIT 50
#define SAMPLES_PER_PIXEL 100
#define MAX_SPHERES 5

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
	Sphere sphere;
};

// Uniforms
uniform vec2 u_ViewportDimensions;
uniform float u_Time;

// Camera related uniforms
uniform vec3 u_CameraBottomLeft;
uniform vec3 u_CameraHorizontal;
uniform vec3 u_CameraVertical;
uniform vec3 u_CameraOrigin;

// Spheres
uniform Sphere u_SceneSpheres[MAX_SPHERES];
uniform int u_SceneSphereCount;

// Functions

vec3 lerp(vec3 v1, vec3 v2, float t);
vec3 GetGradientColorAtRay(Ray ray);

// Utility

int g_RNG_SEED = 0;

vec3 lerp(vec3 v1, vec3 v2, float t)
{
	return (1.0f - t) * v1 + t * v2;
}

int MIN = -2147483648;
int MAX = 2147483647;

int xorshift(in int value) {
    // Xorshift*32
    // Based on George Marsaglia's work: http://www.jstatsoft.org/v08/i14/paper
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

int nextInt(inout int seed) {
    seed = xorshift(seed);
    return seed;
}

float nextFloat(inout int seed) {
    seed = xorshift(seed);
    // FIXME: This should have been a seed mapped from MIN..MAX to 0..1 instead
    return abs(fract(float(seed) / 3141.592653));
}

float nextFloat(inout int seed, in float max) {
    return nextFloat(seed) * max;
}

// ---- 

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

RayHitRecord IntersectSceneSpheres(Ray ray, float tmin, float tmax)
{
	RayHitRecord TempRecord;
	RayHitRecord ClosestRecord;
	bool HitAnything = false;
	float ClosestDistance = tmax;

	for (int i = 0 ; i < u_SceneSphereCount ; i++)
	{
		TempRecord = RaySphereIntersectionTest(u_SceneSpheres[i], ray, tmin, ClosestDistance);

		if (TempRecord.Hit)
		{
			HitAnything = true;
			ClosestDistance = TempRecord.T;
			ClosestRecord = TempRecord;
			ClosestRecord.sphere = u_SceneSpheres[i];
		}
	}

	return ClosestRecord;
}

float ConvertValueRange(float v, vec2 r1, vec2 r2)
{
	float ret = (((v - r1.x) * (r2.y - r2.x)) / (r1.y - r1.x)) + r2.x;
	return ret;
}

Ray GetRay(vec2 uv)
{
	Ray ray;
	ray.Origin = u_CameraOrigin;
	ray.Direction = u_CameraBottomLeft + (u_CameraHorizontal * uv.x) + (u_CameraVertical * uv.y) - u_CameraOrigin;
	
	return ray;
}

vec3 GetRayColor(Ray ray)
{
	RayHitRecord record = IntersectSceneSpheres(ray, 0.0f, MAX_RAY_HIT_DISTANCE);

	if (record.Hit)
	{
		return record.Normal;
	}

	else
	{
		return GetGradientColorAtRay(ray);
	}
}

void main()
{
	const int SPP = 50;
	g_RNG_SEED = int(gl_FragCoord.x) * int(u_ViewportDimensions.x) + int(gl_FragCoord.y) * int(u_Time * u_Time * 1000);
	
	vec3 FinalColor = vec3(0.0f);
	vec2 Pixel;

	Pixel.x = v_TexCoords.x * u_ViewportDimensions.x;
	Pixel.y = v_TexCoords.y * u_ViewportDimensions.y;

	for (int s = 0 ; s < SPP ; s++)
	{
		int seed = g_RNG_SEED + s;

		float u = (Pixel.x + nextFloat(seed)) / u_ViewportDimensions.x;
		float v = (Pixel.y + nextFloat(seed)) / u_ViewportDimensions.y;

		FinalColor += GetRayColor(GetRay(vec2(u, v)));
	}

	FinalColor = FinalColor / float(SPP);

	o_Color = FinalColor;
}
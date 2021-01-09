#version 330 core

// Compile time flags
#define USE_HEMISPHERICAL_DIFFUSE_SCATTERING
//#define ANIMATE_NOISE

#define MAX_RAY_HIT_DISTANCE 100000.0f
#define PI 3.14159265354f
#define RAY_BOUNCE_LIMIT 4
#define SAMPLES_PER_PIXEL 20
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
const int MATERIAL_INVALID = -1;
const int MATERIAL_DIFFUSE = 1;
const int MATERIAL_METAL = 2;

// Sphere
struct Sphere
{
	vec3 Center;
	vec3 Color;
	float Radius;
	int Material;
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

struct SceneIntersectionTestResult
{
	RayHitRecord Record;
	bool HitAnything;
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

int RNG_SEED = 0;

vec3 lerp(vec3 v1, vec3 v2, float t)
{
	return (1.0f - t) * v1 + t * v2;
}

int MIN = -2147483648;
int MAX = 2147483647;

int xorshift(in int value) 
{
    // Xorshift*32
    // Based on George Marsaglia's work: http://www.jstatsoft.org/v08/i14/paper
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}

int nextInt(inout int seed) 
{
    seed = xorshift(seed);
    return seed;
}

float nextFloat(inout int seed) 
{
    seed = xorshift(seed);
    // FIXME: This should have been a seed mapped from MIN..MAX to 0..1 instead
    return abs(fract(float(seed) / 3141.592653));
}

float nextFloat(inout int seed, in float max) 
{
    return nextFloat(seed) * max;
}

float nextFloat(inout int seed, in float min, in float max) 
{
    return min + (max - min) * nextFloat(seed);
}

vec3 cosWeightedRandomHemisphereDirection(const vec3 n) 
{
  	vec2 r = vec2(nextFloat(RNG_SEED), nextFloat(RNG_SEED));
    
	vec3  uu = normalize(cross(n, vec3(0.0,1.0,1.0)));
	vec3  vv = cross(uu, n);
	float ra = sqrt(r.y);
	float rx = ra * cos(6.2831 * r.x); 
	float ry = ra * sin(6.2831 * r.x);
	float rz = sqrt(1.0 - r.y);
	vec3  rr = vec3(rx * uu + ry * vv + rz * n );
    
    return normalize(rr);
}

bool PointIsInSphere(vec3 point, float radius)
{
	return ((point.x * point.x) + (point.y * point.y) + (point.z * point.z)) < (radius * radius);
}

// ----------------------------------------

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
		float sq_root = sqrt(Discriminant);

		float root = (-B - sq_root) / (2.0f * A); // T

		if (root < tmin || root > tmax)
		{
			root = (-B + sq_root) / (2.0f * A);

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

SceneIntersectionTestResult IntersectSceneSpheres(Ray ray, float tmin, float tmax)
{
	SceneIntersectionTestResult Result;
	RayHitRecord TempRecord;
	bool HitAnything = false;
	float ClosestDistance = tmax;

	for (int i = 0 ; i < u_SceneSphereCount ; i++)
	{
		TempRecord = RaySphereIntersectionTest(u_SceneSpheres[i], ray, tmin, ClosestDistance);

		if (TempRecord.Hit)
		{
			HitAnything = true;
			ClosestDistance = TempRecord.T;
			Result.Record = TempRecord;
			Result.Record.sphere = u_SceneSpheres[i];
		}
	}

	Result.HitAnything = HitAnything;
	return Result;
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
	Ray new_ray = ray;
	Sphere hit_sphere;
	Sphere first_sphere;

	RayHitRecord ClosestSphere;
	vec3 FinalColor = GetGradientColorAtRay(new_ray);

	int diffuse_hit_count = 0 ;

	for (int i = 0; i < RAY_BOUNCE_LIMIT ; i++)
	{
		SceneIntersectionTestResult Result = IntersectSceneSpheres(new_ray, 0.001f, MAX_RAY_HIT_DISTANCE);
		hit_sphere = Result.Record.sphere;
		ClosestSphere = Result.Record;

		if (Result.HitAnything) 
		{
			if (i == 0)
			{
				first_sphere = hit_sphere;
			}
		}

		else
		{
			break;
		}

		if (hit_sphere.Material == MATERIAL_DIFFUSE)
		{
			// Get the final ray direction

			#ifdef USE_HEMISPHERICAL_DIFFUSE_SCATTERING
				vec3 R = cosWeightedRandomHemisphereDirection(ClosestSphere.Normal);
			#else
				vec3 R;
				R.x = nextFloat(RNG_SEED, -1.0f, 1.0f);
				R.y = nextFloat(RNG_SEED, -1.0f, 1.0f);
				R.z = nextFloat(RNG_SEED, -1.0f, 1.0f);
			#endif

			vec3 S = ClosestSphere.Normal + R;
			new_ray.Origin = ClosestSphere.Point;
			new_ray.Direction = normalize(S);

			FinalColor = hit_sphere.Color;
			FinalColor /= 2.0f;
			FinalColor = FinalColor / float(diffuse_hit_count + 1);
		}

		if (hit_sphere.Material == MATERIAL_METAL)
		{
			vec3 R = cosWeightedRandomHemisphereDirection(ClosestSphere.Normal);

			vec3 ReflectedRayDirection = reflect(ray.Direction, ClosestSphere.Normal);
			ReflectedRayDirection += first_sphere.FuzzLevel * R;
			
			new_ray.Origin = ClosestSphere.Point;
			new_ray.Direction = ReflectedRayDirection;
		}

		diffuse_hit_count++;
	}

	if (first_sphere.Material == MATERIAL_DIFFUSE)
	{
		FinalColor = first_sphere.Color;
		FinalColor /= 2.0f;
		FinalColor = FinalColor / float(diffuse_hit_count + 1);
	}

	else if (first_sphere.Material == MATERIAL_METAL)
	{
		FinalColor = FinalColor * first_sphere.Color;
	}

	return FinalColor;
}

void main()
{
	#ifdef ANIMATE_NOISE
		RNG_SEED = int(gl_FragCoord.x) + int(gl_FragCoord.y) * int(u_ViewportDimensions.x) * int(u_Time * 1000);
	#else
		RNG_SEED = int(gl_FragCoord.x) + int(gl_FragCoord.y) * int(u_ViewportDimensions.x);
	#endif

	vec3 FinalColor = vec3(0.0f);
	vec2 Pixel;

	Pixel.x = v_TexCoords.x * u_ViewportDimensions.x;
	Pixel.y = v_TexCoords.y * u_ViewportDimensions.y;

	for (int s = 0 ; s < SAMPLES_PER_PIXEL ; s++)
	{
		float u = (Pixel.x + nextFloat(RNG_SEED)) / u_ViewportDimensions.x;
		float v = (Pixel.y + nextFloat(RNG_SEED)) / u_ViewportDimensions.y;

		FinalColor += GetRayColor(GetRay(vec2(u, v)));
	}

	FinalColor = FinalColor / float(SAMPLES_PER_PIXEL);
	o_Color = FinalColor;
}
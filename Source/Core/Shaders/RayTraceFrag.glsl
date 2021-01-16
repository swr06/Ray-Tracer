#version 330 core

// Compile time flags
#define USE_HEMISPHERICAL_DIFFUSE_SCATTERING
#define ANIMATE_NOISE

#define MAX_RAY_HIT_DISTANCE 100000.0f
#define PI 3.14159265354f
#define RAY_BOUNCE_LIMIT 4
#define MAX_SPHERES 5

layout(location = 0) out vec3 o_Color;

in vec2 v_TexCoords;
uniform samplerCube u_Skybox;
uniform int SAMPLES_PER_PIXEL = 1;

// Structures

// Ray
struct Ray
{
	vec3 Origin;
	vec3 Direction;
	bool Reflected; // Whether the ray was reflected off a metal surface or not
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
vec3 GetEnvironmentColorAtRay(Ray ray);

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

vec3 RandomPoint()
{
	vec3 ret;
	ret.x = nextFloat(RNG_SEED, -1.0f, 1.0f);
	ret.y = nextFloat(RNG_SEED, -1.0f, 1.0f);
	ret.z = nextFloat(RNG_SEED, -1.0f, 1.0f);

	return ret;
}

vec3 RandomPointInUnitSphere()
{
	float theta = nextFloat(RNG_SEED, 0.0f, 2.0f * PI);
	float v = nextFloat(RNG_SEED, 0.0f, 1.0f);
	float phi = acos((2.0f * v) - 1.0f);
	float r = pow(nextFloat(RNG_SEED, 0.0f, 1.0f), 1.0f / 3.0f);
	float x = r * sin(phi) * cos(theta);
	float y = r * sin(phi) * sin(theta);
	float z = r * cos(phi); 

	return vec3(x, y, z);
}

bool PointIsInSphere(vec3 point, float radius)
{
	return ((point.x * point.x) + (point.y * point.y) + (point.z * point.z)) < (radius * radius);
}

vec3 RandomPointInUnitSphereRejective()
{
	float x, y, z;

	for (int i = 0 ; i < 20 ; i++)
	{
		x = nextFloat(RNG_SEED, -1.0f, 1.0f);
		y = nextFloat(RNG_SEED, -1.0f, 1.0f);
		z = nextFloat(RNG_SEED, -1.0f, 1.0f);

		if (PointIsInSphere(vec3(x, y, z), 1.0f))
		{
			return vec3(x, y, z);
		}
	}

	return vec3(x, y, z);
}



// ----------------------------------------

vec3 GetEnvironmentColorAtRay(Ray ray)
{
	return texture(u_Skybox, ray.Direction).rgb;
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
	Sphere hit_sphere;
	RayHitRecord closest_record;

	Ray new_ray = ray;
	vec3 total_color = GetEnvironmentColorAtRay(ray);
	int hit_count = 0;
	vec3 diffuse_colors[RAY_BOUNCE_LIMIT + 2];
	int current_color = 0;

	while (hit_count < RAY_BOUNCE_LIMIT)
	{
		SceneIntersectionTestResult res = IntersectSceneSpheres(new_ray, 0.001f, MAX_RAY_HIT_DISTANCE);
		hit_sphere = res.Record.sphere;
		closest_record = res.Record;

		if (res.HitAnything)
		{
			if (hit_sphere.Material == MATERIAL_DIFFUSE)
			{
				vec3 S = closest_record.Normal + RandomPointInUnitSphereRejective();
				new_ray.Origin = closest_record.Point;
				new_ray.Direction = S;
				new_ray.Reflected = false;

				vec3 Color = hit_sphere.Color;
				vec3 TotalColorUntilNow = vec3(1.0);

				for (int i = 0; i < current_color; i++)
				{
					TotalColorUntilNow *= diffuse_colors[i] * 0.5f;
				}

				total_color = (Color) * TotalColorUntilNow;
				diffuse_colors[current_color] = Color;
				current_color++;
			}

			else if (hit_sphere.Material == MATERIAL_METAL)
			{
				vec3 ReflectedRayDirection = reflect(new_ray.Direction, closest_record.Normal);
				ReflectedRayDirection += hit_sphere.FuzzLevel * RandomPointInUnitSphere();

				new_ray.Origin = closest_record.Point;
				new_ray.Direction = ReflectedRayDirection;
				new_ray.Reflected = true;

				vec3 TotalColorUntilNow = vec3(1.0);

				for (int i = 0; i < current_color; i++)
				{
					TotalColorUntilNow *= diffuse_colors[i] * 0.5f;
				}

				total_color = hit_sphere.Color * TotalColorUntilNow;
			}
		}

		else if (new_ray.Reflected)
		{
			vec3 TotalColorUntilNow = vec3(1.0);

			for (int i = 0; i < current_color; i++)
			{
				TotalColorUntilNow *= diffuse_colors[i] * 0.5f;
			}

			new_ray.Reflected = false;
			total_color = TotalColorUntilNow * GetEnvironmentColorAtRay(new_ray);
		}

		hit_count++;
	}

	return total_color;
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
#version 450
#extension GL_ARB_separate_shader_objects: enable

struct AabbTreeNode {
	vec4 leftAabbMin;
	vec4 leftAabbMax;
	vec4 rightAabbMin;
	vec4 rightAabbMax;
	int leftChild;
	int rightChild;
};
struct Triangle {
	vec4 p1;
	vec4 p2;
	vec4 p3;
};

layout (binding = 0) uniform sampler2D uniAlbedo;
layout (binding = 1) uniform sampler2D uniNormal;
layout (binding = 2) uniform sampler2D uniDepth;

layout (binding = 3) buffer AabbTreeNodes {
	int root;
	AabbTreeNode data[];
} nodes;

layout (binding = 4) buffer Triangles {
	Triangle data[];
} triangles;

layout (binding = 5) uniform Uniforms {
	mat4 inverseViewMatrix;
	vec4 tempLightPoint;
	float cameraNear;
	float cameraFar;
	float tanHalfFovY;
	float aspectRatio;
} uniforms;

layout (location = 0) in vec2 inUv;

layout (location = 0) out vec4 outColor;


float max3(vec3 xyz) {
	return max(xyz.x, max(xyz.y, xyz.z));
}
float min3(vec3 xyz) {
	return min(xyz.x, min(xyz.y, xyz.z));
}
bool rayAabIntersection(vec3 origin, vec3 dir, vec3 aabbMin, vec3 aabbMax) {
	aabbMin = (aabbMin - origin) / dir;
	aabbMax = (aabbMax - origin) / dir;
	float rmin = max3(min(aabbMin, aabbMax)), rmax = min3(max(aabbMin, aabbMax));
	return rmin < 1.0f && rmax >= rmin && rmax > 0.0f;
}
bool rayTriangleIntersection(Triangle tri, vec3 origin, vec3 dir) {
	vec3 e1 = tri.p2.xyz - tri.p1.xyz;
	vec3 e2 = tri.p3.xyz - tri.p1.xyz;

	vec3 p = cross(dir, e2);

	float f = 1.0f / dot(e1, p);

	vec3 s = origin - tri.p1.xyz;
	float baryX = f * dot(s, p);
	if (baryX < 0.0f || baryX > 1.0f) {
		return false;
	}

	vec3 q = cross(s, e1);
	float baryY = f * dot(dir, q);
	if (baryY < 0.0f || baryY + baryX > 1.0f) {
		return false;
	}

	f *= dot(e2, q);
	return f > 0.0 && f < 1.0f;
}

const int geomTestInterval = 8;
const int aabbTreeStackSize = 32;

bool traverseAabbTree(vec3 origin, vec3 dir) {
	int stack[aabbTreeStackSize], top = 1;
	stack[0] = nodes.root;
	int candidates[geomTestInterval * 2], numCandidates = 0;
	int counter = 0;
	while (top > 0) {
		AabbTreeNode node = nodes.data[stack[--top]];
		bool
			leftIsect = rayAabIntersection(origin, dir, node.leftAabbMin.xyz, node.leftAabbMax.xyz),
			rightIsect = rayAabIntersection(origin, dir, node.rightAabbMin.xyz, node.rightAabbMax.xyz);
		if (leftIsect) {
			if (node.leftChild < 0) {
				candidates[numCandidates++] = ~node.leftChild;
			} else {
				stack[top++] = node.leftChild;
			}
		}
		if (rightIsect) {
			if (node.rightChild < 0) {
				candidates[numCandidates++] = ~node.rightChild;
			} else {
				stack[top++] = node.rightChild;
			}
		}

		if (++counter == geomTestInterval) {
			for (int i = 0; i < numCandidates; ++i) {
				int geomId = candidates[i];
				if (rayTriangleIntersection(triangles.data[geomId], origin, dir)) {
					return false;
				}
			}
			numCandidates = 0;
			counter = 0;
		}
	}
	for (int i = 0; i < numCandidates; ++i) {
		int geomId = candidates[i];
		if (rayTriangleIntersection(triangles.data[geomId], origin, dir)) {
			return false;
		}
	}
	return true;
}


float sceneDepthToWorldDepth(float depth, float near, float far) {
	return near * far / (far + depth * (near - far));
}
vec3 fragCoordDepthToViewPos(vec2 uv, float worldDepth, float tanHalfFovY, float aspectRatio) {
	uv = uv * 2.0f - 1.0f; // [0, 1] to [-1, 1]
	uv.x *= aspectRatio;
	return vec3(uv * worldDepth * tanHalfFovY, worldDepth);
}


void main() {
	vec3 albedo = texture(uniAlbedo, inUv).xyz;
	vec3 normal = texture(uniNormal, inUv).xyz;
	float depth = texture(uniDepth, inUv).x;

	/*outColor = vec4((vec3(normal) + 1.0f) * 0.5f, 1.0f);*/
	/*outColor = vec4(depth, depth, depth, 1.0f);*/
	/*outColor = albedo;*/

	float worldDepth = sceneDepthToWorldDepth(depth, uniforms.cameraNear, uniforms.cameraFar);
	vec3 viewPos = fragCoordDepthToViewPos(inUv, worldDepth, uniforms.tanHalfFovY, uniforms.aspectRatio);
	vec3 worldPos = (uniforms.inverseViewMatrix * vec4(viewPos, 1.0f)).xyz;

	/*outColor = vec4(vec3(worldDepth / 10.0f), 1.0f);*/
	outColor = vec4(worldPos / 10.0f + 0.5f, 1.0f);

	vec3 rayDir = uniforms.tempLightPoint.xyz - worldPos;
	bool visible = traverseAabbTree(worldPos + 0.01 * rayDir, rayDir * 0.98);
	if (!visible) {
		outColor.xyz *= 0.5f;
	}
}

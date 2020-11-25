// Usage: Define NODE_BUFFER and TRIANGLE_BUFFER as the name of the shader storage buffers before including this file

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

bool raytrace(vec3 origin, vec3 dir) {
	int stack[aabbTreeStackSize], top = 1;
	stack[0] = NODE_BUFFER.root;
	int candidates[geomTestInterval * 2], numCandidates = 0;
	int counter = 0;
	while (top > 0) {
		AabbTreeNode node = NODE_BUFFER.data[stack[--top]];
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
				if (rayTriangleIntersection(TRIANGLE_BUFFER.data[geomId], origin, dir)) {
					return false;
				}
			}
			numCandidates = 0;
			counter = 0;
		}
	}
	for (int i = 0; i < numCandidates; ++i) {
		int geomId = candidates[i];
		if (rayTriangleIntersection(TRIANGLE_BUFFER.data[geomId], origin, dir)) {
			return false;
		}
	}
	return true;
}

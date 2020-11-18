#include "aabbTreeBuilder.h"

#include <cmath>
#include <deque>

#include <nvmath.h>

void aabbForTriangle(const Triangle &tri, nvmath::vec3f &min, nvmath::vec3f &max) {
	min = max = tri.p1;
	min = nvmath::nv_min(min, nvmath::vec3f(tri.p2));
	max = nvmath::nv_max(max, nvmath::vec3f(tri.p2));
	min = nvmath::nv_min(min, nvmath::vec3f(tri.p3));
	max = nvmath::nv_max(max, nvmath::vec3f(tri.p3));
}
float surfaceAreaHeuristic(nvmath::vec3f min, nvmath::vec3f max) {
	nvmath::vec3f size = max - min;
	return size.x * size.y + size.x * size.z + size.y * size.z;
}

struct BuildStep {
	BuildStep() = default;
	BuildStep(int32_t *parent, std::size_t beg, std::size_t end) : parentPtr(parent), rangeBeg(beg), rangeEnd(end) {
	}

	int32_t *parentPtr;
	std::size_t rangeBeg, rangeEnd;
};
struct Leaf {
	nvmath::vec3f centroid, aabbMin, aabbMax;
	int32_t geomIndex;
	std::size_t bucket;
};
struct Bucket {
	nvmath::vec3f
		aabbMin{ std::numeric_limits<float>::max() },
		aabbMax{ -std::numeric_limits<float>::max() };
	std::size_t count = 0;

	float heuristic() const {
		return count * surfaceAreaHeuristic(aabbMin, aabbMax);
	}

	static Bucket merge(Bucket lhs, Bucket rhs) {
		lhs.count += rhs.count;
		
		lhs.aabbMin = nvmath::nv_min(lhs.aabbMin, rhs.aabbMin);
		lhs.aabbMax = nvmath::nv_max(lhs.aabbMax, rhs.aabbMax);
		return lhs;
	}
};

AabbTree AabbTree::build(const nvh::GltfScene &scene) {
	constexpr std::size_t numBuckets = 12;

	AabbTree result;

	// collect triangles & leaves
	std::vector<Leaf> leaves;
	for (const nvh::GltfNode &node : scene.m_nodes) {
		const nvh::GltfPrimMesh &mesh = scene.m_primMeshes[node.primMesh];

		const uint32_t *indices = scene.m_indices.data() + mesh.firstIndex;
		const nvmath::vec3 *pos = scene.m_positions.data() + mesh.vertexOffset;
		for (uint32_t i = 0; i < mesh.indexCount; i += 3, indices += 3) {
			// triangle
			Triangle &tri = result.triangles.emplace_back();
			tri.p1 = node.worldMatrix * nvmath::vec4(pos[indices[0]], 1.0f);
			tri.p2 = node.worldMatrix * nvmath::vec4(pos[indices[1]], 1.0f);
			tri.p3 = node.worldMatrix * nvmath::vec4(pos[indices[2]], 1.0f);

			// leaf
			Leaf &cur = leaves.emplace_back();
			aabbForTriangle(tri, cur.aabbMin, cur.aabbMax);
			cur.geomIndex = static_cast<int32_t>(result.triangles.size() - 1);
			cur.centroid = 0.5f * (cur.aabbMin + cur.aabbMax);
		}
	}

	result.nodes.resize(result.triangles.size() - 1);
	int32_t alloc = 0;
	std::deque<BuildStep> q;
	q.emplace_back(&result.root, 0, result.triangles.size());
	while (!q.empty()) {
		BuildStep step = q.front();
		q.pop_front();

		switch (step.rangeEnd - step.rangeBeg) {
		case 1:
			*step.parentPtr = ~leaves[step.rangeBeg].geomIndex;
			break;
		case 2:
			{
				Leaf &left = leaves[step.rangeBeg], &right = leaves[step.rangeBeg + 1];
				int32_t nodeIndex = alloc++;
				*step.parentPtr = nodeIndex;
				AabbNode &node = result.nodes[nodeIndex];
				node.leftChild = ~left.geomIndex;
				node.rightChild = ~right.geomIndex;
				node.leftAabbMin = left.aabbMin;
				node.leftAabbMax = left.aabbMax;
				node.rightAabbMin = right.aabbMin;
				node.rightAabbMax = right.aabbMax;
			}
			break;
		default:
			{
				// compute centroid & aabb bounds
				nvmath::vec3f centroidMin = leaves[step.rangeBeg].centroid;
				nvmath::vec3f centroidMax = centroidMin;
				float outerHeuristic;
				{
					nvmath::vec3f
						aabbMin = leaves[step.rangeBeg].aabbMin,
						aabbMax = leaves[step.rangeBeg].aabbMax;
					for (std::size_t i = step.rangeBeg + 1; i < step.rangeEnd; ++i) {
						const Leaf &cur = leaves[i];
						centroidMin = nvmath::nv_min(centroidMin, cur.centroid);
						centroidMax = nvmath::nv_max(centroidMax, cur.centroid);
						aabbMin = nvmath::nv_min(aabbMin, cur.aabbMin);
						aabbMax = nvmath::nv_max(aabbMax, cur.aabbMax);
						if (abs(aabbMin.x) > 1000) {
							__debugbreak();
						}
					}
					outerHeuristic = surfaceAreaHeuristic(aabbMin, aabbMax);
				}
				// find split direction
				nvmath::vec3f centroidSpan = centroidMax - centroidMin;
				int splitDim = centroidSpan.x > centroidSpan.y ? 0 : 1;
				if (centroidSpan.z > centroidSpan[splitDim]) {
					splitDim = 2;
				}
				// bucket nodes
				Bucket buckets[numBuckets];
				float bucketRange = centroidSpan[splitDim] / numBuckets;
				for (std::size_t i = step.rangeBeg; i < step.rangeEnd; ++i) {
					leaves[i].bucket = static_cast<std::size_t>(nvmath::nv_clamp(
						(leaves[i].centroid[splitDim] - centroidMin[splitDim]) / bucketRange, 0.5f, numBuckets - 0.5f
					));
					Bucket &buck = buckets[leaves[i].bucket];
					Leaf &cur = leaves[i];
					buck.aabbMin = nvmath::nv_min(buck.aabbMin, cur.aabbMin);
					buck.aabbMax = nvmath::nv_max(buck.aabbMax, cur.aabbMax);
					++buck.count;
				}
				// find optimal split point
				Bucket boundCache[numBuckets - 1];
				{
					Bucket current = buckets[numBuckets - 1];
					for (std::size_t i = numBuckets - 1; i > 0; ) {
						boundCache[--i] = current;
						current = Bucket::merge(current, buckets[i]);
					}
				}
				std::size_t optSplitPoint = 0;
				nvmath::vec3f leftMin, leftMax, rightMin, rightMax;
				{
					float minHeuristic = std::numeric_limits<float>::max();
					Bucket sumLeft;
					for (std::size_t splitPoint = 0; splitPoint < numBuckets - 1; ++splitPoint) {
						sumLeft = Bucket::merge(sumLeft, buckets[splitPoint]);
						Bucket sumRight = boundCache[splitPoint];
						float heuristic =
							0.125f + (sumLeft.heuristic() + sumRight.heuristic()) / outerHeuristic;
						if (heuristic < minHeuristic) {
							minHeuristic = heuristic;
							optSplitPoint = splitPoint;
							leftMin = sumLeft.aabbMin;
							leftMax = sumLeft.aabbMax;
							rightMin = sumRight.aabbMin;
							rightMax = sumRight.aabbMax;
						}
					}
				}
				// split
				std::size_t pivot = step.rangeBeg;
				for (std::size_t i = step.rangeBeg; i < step.rangeEnd; ++i) {
					if (leaves[i].bucket <= optSplitPoint) {
						std::swap(leaves[i], leaves[pivot++]);
					}
				}
				// handle objects with overlapping centroids
				if (pivot == step.rangeBeg || pivot == step.rangeEnd) {
					pivot = (step.rangeBeg + step.rangeEnd) / 2;

					leftMin = leaves[step.rangeBeg].aabbMin;
					leftMax = leaves[step.rangeBeg].aabbMax;
					for (std::size_t i = step.rangeBeg + 1; i < pivot; ++i) {
						leftMin = nvmath::nv_min(leftMin, leaves[i].aabbMin);
						leftMax = nvmath::nv_max(leftMax, leaves[i].aabbMax);
					}

					rightMin = leaves[pivot].aabbMin;
					rightMax = leaves[pivot].aabbMax;
					for (std::size_t i = pivot; i < step.rangeEnd; ++i) {
						rightMin = nvmath::nv_min(rightMin, leaves[i].aabbMin);
						rightMax = nvmath::nv_max(rightMax, leaves[i].aabbMax);
					}
				}

				int32_t nodeIndex = alloc++;
				*step.parentPtr = nodeIndex;
				AabbNode &n = result.nodes[nodeIndex];
				n.leftAabbMin = leftMin;
				n.leftAabbMax = leftMax;
				n.rightAabbMin = rightMin;
				n.rightAabbMax = rightMax;
				q.emplace_back(&n.leftChild, step.rangeBeg, pivot);
				q.emplace_back(&n.rightChild, pivot, step.rangeEnd);
			}
			break;
		}
	}

	return result;
}
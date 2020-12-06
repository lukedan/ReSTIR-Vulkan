#pragma once
#include "gltfscene.h"
#include "nvmath.h"
#include "mikktspace.h"
#include <vector>
// https://github.com/teared/mikktspace-for-houdini
// http://www.mikktspace.com/


bool genTangents(const nvh::GltfPrimMesh* resultMesh, 
	const uint32_t* m_indices, const nvmath::vec3f* m_positions, const nvmath::vec3f* m_normals, const nvmath::vec2f* m_texcoords0,
	nvmath::vec4f* m_tangent, bool basic = true);

// Returns the number of faces (triangles/quads) on the mesh to be processed
int getNumFaces(const SMikkTSpaceContext* context);

// Returns the number of vertices on face number iFace
// iFace is a number in the range {0, 1, ..., getNumFaces()-1}
int getNumVerticesOfFace(const SMikkTSpaceContext* pContext, const int iFace);

// returns the position/normal/texcoord of the referenced face of vertex number iVert.
// iVert is in the range {0,1,2} for triangles and {0,1,2,3} for quads.

// Write 3-float position of the vertex's point.
static void getPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert);

// Write 3-float vertex normal.
static void getNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert);

// Write 2-float vertex uv.
static void getTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert);

// Compute and set attributes on the geometry vertex.
static void setTSpaceBasic(const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert);

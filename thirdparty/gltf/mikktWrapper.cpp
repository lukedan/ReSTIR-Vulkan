#include "mikktWrapper.h"


typedef struct batchData {
	const nvh::GltfPrimMesh* pResultMesh;
	const uint32_t* pIndices;
	const nvmath::vec3f* pPositions;
	const nvmath::vec3f* pNormals;
	const nvmath::vec2f* mTexcoords0;
	nvmath::vec4f* pTangents;
} batchData;

bool genTangents(const nvh::GltfPrimMesh* resultMesh,
	const uint32_t* m_indices, const nvmath::vec3f* m_positions, const nvmath::vec3f* m_normals, const nvmath::vec2f* m_texcoords0,
	nvmath::vec4f* m_tangent, bool basic) {
	
	// Initialize MikkTSpaceInterface with callbacks and run tangents calculator.
	SMikkTSpaceInterface iface;
	iface.m_getNumFaces = getNumFaces;
	iface.m_getNumVerticesOfFace = getNumVerticesOfFace;
	iface.m_getPosition = getPosition;
	iface.m_getNormal = getNormal;
	iface.m_getTexCoord = getTexCoord;
	iface.m_setTSpaceBasic = setTSpaceBasic;
	iface.m_setTSpace = NULL;

	batchData batchDataInstance{ resultMesh, m_indices, m_positions,  m_normals, m_texcoords0, m_tangent };

	SMikkTSpaceContext context;
	context.m_pInterface = &iface;
	context.m_pUserData = &batchDataInstance;
	int idxCount = resultMesh->indexCount;

	return genTangSpaceDefault(&context);
}

// Returns the number of faces (triangles/quads) on the mesh to be processed
int getNumFaces(const SMikkTSpaceContext* pContext)
{
	batchData* pBatchData = static_cast <batchData*> (pContext->m_pUserData);
	return pBatchData->pResultMesh->indexCount / 3;
}

// Returns the number of vertices on face number iFace
// iFace is a number in the range {0, 1, ..., getNumFaces()-1}
int getNumVerticesOfFace(const SMikkTSpaceContext* pContext, const int iFace) {
	return 3;
}

// returns the position/normal/texcoord of the referenced face of vertex number iVert.
// iVert is in the range {0,1,2} for triangles and {0,1,2,3} for quads.

// Write 3-float position of the vertex's point.
static void getPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) {
	batchData* pBatchData = static_cast <batchData*> (pContext->m_pUserData);
	// local index
	uint32_t l_idx = pBatchData->pIndices[pBatchData->pResultMesh->firstIndex + iFace + iVert];
	// global index
	uint32_t g_idx = l_idx + pBatchData->pResultMesh->vertexOffset;
	// Output pos
	fvPosOut[0] = pBatchData->pPositions[g_idx][0];
	fvPosOut[1] = pBatchData->pPositions[g_idx][1];
	fvPosOut[2] = pBatchData->pPositions[g_idx][2];
}

// Write 3-float vertex normal.
static void getNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) {
	batchData* pBatchData = static_cast <batchData*> (pContext->m_pUserData);
	// local index
	uint32_t l_idx = pBatchData->pIndices[pBatchData->pResultMesh->firstIndex + iFace + iVert];
	// global index
	uint32_t g_idx = l_idx + pBatchData->pResultMesh->vertexOffset;
	// Output normal
	fvNormOut[0] = pBatchData->pNormals[g_idx][0];
	fvNormOut[1] = pBatchData->pNormals[g_idx][1];
	fvNormOut[2] = pBatchData->pNormals[g_idx][2];
}

// Write 2-float vertex uv.
static void getTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) {
	batchData* pBatchData = static_cast <batchData*> (pContext->m_pUserData);
	// local index
	uint32_t l_idx = pBatchData->pIndices[pBatchData->pResultMesh->firstIndex + iFace + iVert];
	// global index
	uint32_t g_idx = l_idx + pBatchData->pResultMesh->vertexOffset;
	// Output UV
	fvTexcOut[0] = pBatchData->mTexcoords0[g_idx][0];
	fvTexcOut[1] = pBatchData->mTexcoords0[g_idx][1];
}

// Compute and set attributes on the geometry vertex.
static void setTSpaceBasic(const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
	batchData* pBatchData = static_cast <batchData*> (pContext->m_pUserData);
	// local index
	uint32_t l_idx = pBatchData->pIndices[pBatchData->pResultMesh->firstIndex + iFace * 3 + iVert];
	// Set Tangent and sign
	pBatchData->pTangents[l_idx] = nvmath::vec4f(fvTangent[0], fvTangent[1], fvTangent[2], fSign);
}


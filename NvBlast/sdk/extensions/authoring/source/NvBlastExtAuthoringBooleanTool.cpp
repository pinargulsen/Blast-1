/*
* Copyright (c) 2016-2017, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "NvBlastExtAuthoringBooleanTool.h"
#include "NvBlastExtAuthoringMesh.h"
#include "NvBlastExtAuthoringAccelerator.h"

#include <math.h>
#include <set>
#include <algorithm>

using physx::PxVec3;
using physx::PxVec2;
using physx::PxBounds3;


namespace Nv
{
namespace Blast
{

/* Linear interpolation of vectors */

NV_FORCE_INLINE void vec3Lerp(const PxVec3& a, const PxVec3& b, PxVec3& out, float t)
{
	out.x = (b.x - a.x) * t + a.x;
	out.y = (b.y - a.y) * t + a.y;
	out.z = (b.z - a.z) * t + a.z;
}

NV_FORCE_INLINE void vec2Lerp(const PxVec2& a, const PxVec2& b, PxVec2& out, float t)
{
	out.x = (b.x - a.x) * t + a.x;
	out.y = (b.y - a.y) * t + a.y;
}


NV_FORCE_INLINE int32_t BooleanEvaluator::addIfNotExist(Vertex& p)
{
	mVerticesAggregate.push_back(p);
	return static_cast<int32_t>(mVerticesAggregate.size()) - 1;
}

NV_FORCE_INLINE void BooleanEvaluator::addEdgeIfValid(EdgeWithParent& ed)
{
	mEdgeAggregate.push_back(ed);
}

/**
Vertex level shadowing functions
*/
NV_FORCE_INLINE int32_t vertexShadowing(const PxVec3& a, const PxVec3& b)
{
	return (b.x >= a.x) ? 1 : 0;
}
/**
Vertex-edge status functions
*/
NV_FORCE_INLINE int32_t veStatus01(const PxVec3& sEdge, const PxVec3& eEdge, const PxVec3& p)
{
	return vertexShadowing(p, eEdge) - vertexShadowing(p, sEdge);
}

NV_FORCE_INLINE int32_t veStatus10(const PxVec3& sEdge, const PxVec3& eEdge, const PxVec3& p)
{
	return -vertexShadowing(eEdge, p) + vertexShadowing(sEdge, p);
}

/**
Vertex-edge shadowing functions
*/
int32_t shadowing01(const Vertex& sEdge, const Vertex& eEdge, const PxVec3& p, Vertex& onEdgePoint, bool& hasOnEdge)
{
	int32_t winding = veStatus01(sEdge.p, eEdge.p, p);
	if (winding != 0)
	{
		float t = (p.x - sEdge.p.x) / (eEdge.p.x - sEdge.p.x);
		if (t >= 1)
		{
			onEdgePoint = eEdge;
		}
		else if (t <= 0)
		{
			onEdgePoint = sEdge;
		}
		else
		{
			vec3Lerp(sEdge.p, eEdge.p, onEdgePoint.p, t);
			vec3Lerp(sEdge.n, eEdge.n, onEdgePoint.n, t);
			vec2Lerp(sEdge.uv[0], eEdge.uv[0], onEdgePoint.uv[0], t);
		}
		hasOnEdge = true;
		if (onEdgePoint.p.y >= p.y)
		{
			return winding;
		}
	}
	else
	{
		hasOnEdge = false;
	}
	return 0;
}
int32_t shadowing10(const Vertex& sEdge, const Vertex& eEdge, const PxVec3& p, Vertex& onEdgePoint, bool& hasOnEdge)
{
	int32_t winding = veStatus10(sEdge.p, eEdge.p, p);
	if (winding != 0)
	{
		float t = (p.x - sEdge.p.x) / (eEdge.p.x - sEdge.p.x);
		if (t >= 1)
		{
			onEdgePoint = eEdge;
		}
		else if (t <= 0)
		{
			onEdgePoint = sEdge;
		}
		else
		{
			vec3Lerp(sEdge.p, eEdge.p, onEdgePoint.p, t);
			vec3Lerp(sEdge.n, eEdge.n, onEdgePoint.n, t);
			vec2Lerp(sEdge.uv[0], eEdge.uv[0], onEdgePoint.uv[0], t);
		}
		hasOnEdge = true;
		if (onEdgePoint.p.y < p.y)
		{
			return winding;
		}
	}
	else
	{
		hasOnEdge = false;
	}
	return 0;
}

int32_t shadowing01(const PxVec3& sEdge, const PxVec3& eEdge, const PxVec3& p)
{
	int32_t winding = veStatus01(sEdge, eEdge, p);
	if (winding != 0)
	{
		float t = ((p.x - sEdge.x) / (eEdge.x - sEdge.x));
		PxVec3 onEdgePoint;
		if (t >= 1)
			onEdgePoint = eEdge;
		else if (t <= 0)
			onEdgePoint = sEdge;
		else
			vec3Lerp(sEdge, eEdge, onEdgePoint, t);
		if (onEdgePoint.y >= p.y)
		{
			return winding;
		}
	}
	return 0;
}

int32_t shadowing10(const PxVec3& sEdge, const PxVec3& eEdge, const PxVec3& p)
{
	int32_t winding = veStatus10(sEdge, eEdge, p);
	if (winding != 0)
	{
		float t = ((p.x - sEdge.x) / (eEdge.x - sEdge.x));
		PxVec3 onEdgePoint;
		if (t >= 1)
			onEdgePoint = eEdge;
		else if (t <= 0)
			onEdgePoint = sEdge;
		else
			vec3Lerp(sEdge, eEdge, onEdgePoint, t);
		if (onEdgePoint.y < p.y)
		{
			return winding;
		}
	}
	return 0;
}

/**
Vertex-facet shadowing functions
*/

int32_t vfStatus02(const PxVec3& p, const Vertex* points, const Edge* edges, int32_t edgesCount, Vertex& out1, Vertex& out2)
{
	int32_t val = 0;
	Vertex pnt;
	bool hasOnEdge = false;
	for (int32_t i = 0; i < edgesCount; ++i)
	{
		val -= shadowing01(points[edges->s], points[edges->e], p, pnt, hasOnEdge);
		if (hasOnEdge != 0)
		{
			out2 = out1;
			out1 = pnt;
		}
		++edges;
	}
	return val;
}


int32_t shadowing02(const PxVec3& p, const Vertex* points, const Edge* edges, int edgesCount, bool& hasOnFacetPoint, Vertex& onFacetPoint)
{
	Vertex p1, p2;
	int32_t stat = vfStatus02(p, points, edges, edgesCount, p1, p2);
	float z = 0;
	hasOnFacetPoint = false;
	if (stat != 0)
	{
		PxVec3 vc = p2.p - p1.p;
		float t = 0;
		t = (abs(vc.x) > abs(vc.y)) ? (p.x - p1.p.x) / vc.x : (p.y - p1.p.y) / vc.y;
		t = (t < 0) ? 0 : t;
		t = (t > 1) ? 1 : t;
		z = t * vc.z + p1.p.z;

		hasOnFacetPoint = true;
		onFacetPoint.p.x = p.x;
		onFacetPoint.p.y = p.y;
		onFacetPoint.p.z = z;

		vec2Lerp(p1.uv[0], p2.uv[0], onFacetPoint.uv[0], t);
		vec3Lerp(p1.n, p2.n, onFacetPoint.n, t);

		if (z >= p.z)
		{
			return stat;
		}
	}
	return 0;
}

int32_t vfStatus20(const PxVec3& p, const Vertex* points, const Edge* edges, int32_t edgesCount, Vertex& out1, Vertex& out2)
{
	int32_t val = 0;
	Vertex pnt;
	bool hasOnEdge = false;
	for (int32_t i = 0; i < edgesCount; ++i)
	{
		val += shadowing10(points[edges->s], points[edges->e], p, pnt, hasOnEdge);
		if (hasOnEdge != 0)
		{
			out2 = out1;
			out1 = pnt;
		}
		++edges;
	}
	return val;
}

int32_t shadowing20(const PxVec3& p, const Vertex* points, const Edge* edges, int edgesCount, bool& hasOnFacetPoint, Vertex& onFacetPoint)
{
	Vertex p1, p2;
	int32_t stat = vfStatus20(p, points, edges, edgesCount, p1, p2);
	hasOnFacetPoint = false;
	if (stat != 0)
	{
		PxVec3 vc = p2.p - p1.p;
		float t = 0;
		t = (abs(vc.x) > abs(vc.y)) ? (p.x - p1.p.x) / vc.x : (p.y - p1.p.y) / vc.y;
		t = (t < 0) ? 0 : t;
		t = (t > 1) ? 1 : t;

		hasOnFacetPoint = true;
		onFacetPoint.p.x = p.x;
		onFacetPoint.p.y = p.y;

		onFacetPoint.p.z = t * vc.z + p1.p.z;
		
		vec2Lerp(p1.uv[0], p2.uv[0], onFacetPoint.uv[0], t);
		vec3Lerp(p1.n, p2.n, onFacetPoint.n, t);

		if (onFacetPoint.p.z < p.z)
		{
			return stat;
		}
	}
	return 0;
}


NV_FORCE_INLINE int32_t edgesCrossCheck(const PxVec3& eAs, const PxVec3& eAe, const PxVec3& eBs, const PxVec3& eBe)
{
	return shadowing01(eBs, eBe, eAe) - shadowing01(eBs, eBe, eAs) + shadowing10(eAs, eAe, eBe) - shadowing10(eAs, eAe, eBs);
}

int32_t edgesIntersection(const Vertex& eAs, const Vertex& eAe, const Vertex& eBs, const Vertex& eBe, Vertex& intersectionA, Vertex& intersectionB, bool& hasPoints)
{
	int32_t status = edgesCrossCheck(eAs.p, eAe.p, eBs.p, eBe.p);
	hasPoints = false;
	if (status == 0)
		return 0;
	Vertex tempPoint;

	Vertex bShadowingPair[2];
	Vertex aShadowingPair[2];
	bool hasOnEdge = false;
	int32_t shadowingType = shadowing10(eAs, eAe, eBs.p, tempPoint, hasOnEdge);

	bool aShadowing = false;
	bool bShadowing = false;


	if (shadowingType == 0 && hasOnEdge)
	{
		aShadowing = true;
		aShadowingPair[0] = eBs;
		aShadowingPair[1] = tempPoint;
	}
	else
	{
		if (shadowingType == 1 || shadowingType == -1)
		{
			bShadowing = true;
			bShadowingPair[0] = eBs;
			bShadowingPair[1] = tempPoint;
		}
	}

	shadowingType = shadowing10(eAs, eAe, eBe.p, tempPoint, hasOnEdge);

	if (shadowingType == 0 && !aShadowing && hasOnEdge)
	{
		aShadowing = true;
		aShadowingPair[0] = eBe;
		aShadowingPair[1] = tempPoint;
	}
	else
	{
		if ((shadowingType == 1 || shadowingType == -1) && !bShadowing)
		{
			bShadowing = true;
			bShadowingPair[0] = eBe;
			bShadowingPair[1] = tempPoint;
		}
	}
	shadowingType = shadowing01(eBs, eBe, eAe.p, tempPoint, hasOnEdge);

	if (shadowingType == 0 && !aShadowing && hasOnEdge)
	{
		aShadowing = true;
		aShadowingPair[1] = eAe;
		aShadowingPair[0] = tempPoint;
	}
	else
	{
		if ((shadowingType == 1 || shadowingType == -1) && !bShadowing)
		{
			bShadowing = true;
			bShadowingPair[1] = eAe;
			bShadowingPair[0] = tempPoint;
		}
	}

	shadowingType = shadowing01(eBs, eBe, eAs.p, tempPoint, hasOnEdge);

	if (shadowingType == 0 && !aShadowing && hasOnEdge)
	{
		aShadowing = true;
		aShadowingPair[1] = eAs;
		aShadowingPair[0] = tempPoint;
	}
	else
	{
		if ((shadowingType == 1 || shadowingType == -1) && !bShadowing)
		{
			bShadowing = true;
			bShadowingPair[1] = eAs;
			bShadowingPair[0] = tempPoint;
		}
	}
	float deltaPlus = bShadowingPair[0].p.y - bShadowingPair[1].p.y;
	float deltaMinus = aShadowingPair[0].p.y - aShadowingPair[1].p.y;
	float div = 0;
	if (deltaPlus > 0)
		div = deltaPlus / (deltaPlus - deltaMinus);
	else
		div = 0;

	intersectionA.p = bShadowingPair[1].p - div * (bShadowingPair[1].p - aShadowingPair[1].p);
	intersectionA.n = bShadowingPair[1].n - div * (bShadowingPair[1].n - aShadowingPair[1].n);
	intersectionA.uv[0] = bShadowingPair[1].uv[0] - (bShadowingPair[1].uv[0] - aShadowingPair[1].uv[0]) * div;
	intersectionB.p = intersectionA.p;
	intersectionB.p.z = bShadowingPair[0].p.z - div * (bShadowingPair[0].p.z - aShadowingPair[0].p.z);
	intersectionB.n = bShadowingPair[0].n - div * (bShadowingPair[0].n - aShadowingPair[0].n);
	intersectionB.uv[0] = bShadowingPair[0].uv[0] - (bShadowingPair[0].uv[0] - aShadowingPair[0].uv[0]) * div;

	hasPoints = true;
	return status;
}

NV_FORCE_INLINE int32_t edgeEdgeShadowing(const Vertex& eAs, const Vertex& eAe, const Vertex& eBs, const Vertex& eBe, Vertex& intersectionA, Vertex& intersectionB, bool& hasPoints)
{
	int32_t status = edgesIntersection(eAs, eAe, eBs, eBe, intersectionA, intersectionB, hasPoints);
	if (intersectionB.p.z >= intersectionA.p.z)
	{
		return status;
	}
	return 0;
}

int32_t edgeFacetIntersection12(const Vertex& edSt, const Vertex& edEnd, const Vertex* points, const Edge* edges, int edgesCount, Vertex& intersectionA, Vertex& intersectionB)
{
	int32_t status = 0;
	Vertex p1, p2;
	Vertex bShadowingPair[2];
	Vertex aShadowingPair[2];
	bool hasPoint = false;
	int32_t shadowingType = shadowing02(edEnd.p, points, edges, edgesCount, hasPoint, p1);
	status -= shadowingType;
	bool aShadowing = false;
	bool bShadowing = false;

	if (shadowingType == 0 && hasPoint)
	{
		aShadowing = true;
		aShadowingPair[0] = p1;
		aShadowingPair[1] = edEnd;
	}
	else
	{
		if (shadowingType == 1 || shadowingType == -1)
		{
			bShadowing = true;
			bShadowingPair[0] = p1;
			bShadowingPair[1] = edEnd;
		}
	}

	shadowingType = shadowing02(edSt.p, points, edges, edgesCount, hasPoint, p1);
	status += shadowingType;
	if (shadowingType == 0 && !aShadowing && hasPoint)
	{
		aShadowing = true;
		aShadowingPair[0] = p1;
		aShadowingPair[1] = edSt;
	}
	else
	{
		if ((shadowingType == 1 || shadowingType == -1) && !bShadowing)
		{
			bShadowing = true;
			bShadowingPair[0] = p1;
			bShadowingPair[1] = edSt;
		}
	}

	for (int32_t ed = 0; ed < edgesCount; ++ed)
	{
		shadowingType = edgeEdgeShadowing(edSt, edEnd, points[edges[ed].s], points[edges[ed].e], p1, p2, hasPoint);
		status -= shadowingType;
		if (shadowingType == 0 && !aShadowing && hasPoint)
		{
			aShadowing = true;
			aShadowingPair[0] = p2;
			aShadowingPair[1] = p1;
		}
		else
		{
			if ((shadowingType == 1 || shadowingType == -1) && !bShadowing)
			{
				bShadowing = true;
				bShadowingPair[0] = p2;
				bShadowingPair[1] = p1;
			}
		}
	}
	if (status == 0)
	{
		return 0;
	}
	if (!bShadowing || !aShadowing)
	{
		return 0;
	}
	float deltaPlus = bShadowingPair[0].p.z - bShadowingPair[1].p.z;
	float div = 0;
	if (deltaPlus != 0)
	{
		float deltaMinus = aShadowingPair[0].p.z - aShadowingPair[1].p.z;
		div = deltaPlus / (deltaPlus - deltaMinus);
	}
	intersectionA.p = bShadowingPair[1].p - div * (bShadowingPair[1].p - aShadowingPair[1].p);
	intersectionA.n = bShadowingPair[1].n - div * (bShadowingPair[1].n - aShadowingPair[1].n);
	intersectionA.uv[0] = bShadowingPair[1].uv[0] - (bShadowingPair[1].uv[0] - aShadowingPair[1].uv[0]) * div;

	intersectionB.p = intersectionA.p;
	intersectionB.n = bShadowingPair[0].n - div * (bShadowingPair[0].n - aShadowingPair[0].n);
	intersectionB.uv[0] = bShadowingPair[0].uv[0] - (bShadowingPair[0].uv[0] - aShadowingPair[0].uv[0]) * div;


	return status;
}


int32_t edgeFacetIntersection21(const Vertex& edSt, const Vertex& edEnd, const Vertex* points, const Edge* edges, int edgesCount, Vertex& intersectionA, Vertex& intersectionB)
{
	int32_t status = 0;
	Vertex p1, p2;

	Vertex bShadowingPair[2];
	Vertex aShadowingPair[2];
	bool hasPoint = false;
	int32_t shadowingType = shadowing20(edEnd.p, points, edges, edgesCount, hasPoint, p1);
	status = shadowingType;
	bool aShadowing = false;
	bool bShadowing = false;
	if (shadowingType == 0 && hasPoint)
	{
		aShadowing = true;
		aShadowingPair[0] = edEnd;
		aShadowingPair[1] = p1;
	}
	else
	{
		if (shadowingType == 1 || shadowingType == -1)
		{
			bShadowing = true;
			bShadowingPair[0] = edEnd;
			bShadowingPair[1] = p1;
		}
	}

	shadowingType = shadowing20(edSt.p, points, edges, edgesCount, hasPoint, p1);
	status -= shadowingType;
	if (shadowingType == 0 && !aShadowing && hasPoint)
	{
		aShadowing = true;
		aShadowingPair[0] = edSt;
		aShadowingPair[1] = p1;
	}
	else
	{
		if ((shadowingType == 1 || shadowingType == -1) && !bShadowing)
		{
			bShadowing = true;
			bShadowingPair[0] = edSt;
			bShadowingPair[1] = p1;
		}
	}

	for (int32_t ed = 0; ed < edgesCount; ++ed)
	{
		shadowingType = edgeEdgeShadowing(points[edges[ed].s], points[edges[ed].e], edSt, edEnd, p1, p2, hasPoint);
		status -= shadowingType;
		if (shadowingType == 0)
		{
			if (!aShadowing && hasPoint)
			{
				aShadowing = true;
				aShadowingPair[0] = p2;
				aShadowingPair[1] = p1;
			}
		}
		else
		{
			if ((shadowingType == 1 || shadowingType == -1) && !bShadowing)
			{
				bShadowing = true;
				bShadowingPair[0] = p2;
				bShadowingPair[1] = p1;
			}
		}
	}
	if (status == 0)
	{
		return 0;
	}
	if (!bShadowing || !aShadowing)
	{
		return 0;
	}
	float deltaPlus = bShadowingPair[0].p.z - bShadowingPair[1].p.z;
	float div = 0;
	if (deltaPlus != 0)
	{
		float deltaMinus = aShadowingPair[0].p.z - aShadowingPair[1].p.z;
		div = deltaPlus / (deltaPlus - deltaMinus);
	}
	intersectionA.p = bShadowingPair[1].p - div * (bShadowingPair[1].p - aShadowingPair[1].p);
	intersectionA.n = bShadowingPair[1].n - div * (bShadowingPair[1].n - aShadowingPair[1].n);
	intersectionA.uv[0] = bShadowingPair[1].uv[0] - (bShadowingPair[1].uv[0] - aShadowingPair[1].uv[0]) * div;

	intersectionB.p = intersectionA.p;
	intersectionB.n = bShadowingPair[0].n - div * (bShadowingPair[0].n - aShadowingPair[0].n);
	intersectionB.uv[0] = bShadowingPair[0].uv[0] - (bShadowingPair[0].uv[0] - aShadowingPair[0].uv[0]) * div;

	return status;
}

int32_t BooleanEvaluator::vertexMeshStatus03(const PxVec3& p, Mesh* mesh)
{
	int32_t status = 0;
	Vertex pnt;
	bool hasPoint = false;
	mAcceleratorB->setState(p);
	int32_t facet = mAcceleratorB->getNextFacet();
	while (facet != -1)
	{
		Edge* ed = mesh->getEdges() + mesh->getFacet(facet)->firstEdgeNumber;
		status += shadowing02(p, mesh->getVertices(), ed, mesh->getFacet(facet)->edgesCount, hasPoint, pnt);
		facet = mAcceleratorB->getNextFacet();
	}

	//for (int32_t facet = 0; facet < mesh->getFacetCount(); ++facet) 
	//{
	//	Edge* ed = mesh->getEdges() + mesh->getFacet(facet)->firstEdgeNumber;
	//	status += shadowing02(p, mesh->getVertices(), ed, mesh->getFacet(facet)->edgesCount, hasPoint, pnt);
	//}

	return status;
}

int32_t BooleanEvaluator::vertexMeshStatus30(const PxVec3& p, Mesh* mesh)
{
	int32_t status = 0;
	bool hasPoints = false;
	Vertex point;
	mAcceleratorA->setState(p);
	int32_t facet = mAcceleratorA->getNextFacet();
	while ( facet != -1)
	{
		Edge* ed = mesh->getEdges() + mesh->getFacet(facet)->firstEdgeNumber;
		status -= shadowing20(p, mesh->getVertices(), ed, mesh->getFacet(facet)->edgesCount, hasPoints, point);
		facet = mAcceleratorA->getNextFacet();
	}

	//for (int32_t facet = 0; facet < mesh->getFacetCount(); ++facet)
	//{
	//	Edge* ed = mesh->getEdges() + mesh->getFacet(facet)->firstEdgeNumber;
	//	status -= shadowing20(p, mesh->getVertices(), ed, mesh->getFacet(facet)->edgesCount, hasPoints, point);
	//}
	return status;
}

NV_FORCE_INLINE int32_t inclusionValue03(BooleanConf& conf, int32_t xValue)
{
	return conf.ca + conf.ci * xValue;
}

NV_FORCE_INLINE int32_t inclusionValueEdgeFace(BooleanConf& conf, int32_t xValue)
{
	return conf.ci * xValue;
}

NV_FORCE_INLINE int32_t inclusionValue30(BooleanConf& conf, int32_t xValue)
{
	return conf.cb + conf.ci * xValue;
}

struct VertexComparator
{
	VertexComparator(PxVec3 base = PxVec3()) : basePoint(base) {};
	PxVec3 basePoint;
	bool operator()(const Vertex& a, const Vertex& b)
	{
		return (b.p - a.p).dot(basePoint) > 0.0;
	}
};

struct VertexPairComparator
{
	VertexPairComparator(PxVec3 base = PxVec3()) : basePoint(base) {};
	PxVec3 basePoint;
	bool operator()(const std::pair<Vertex, Vertex>& a, const std::pair<Vertex, Vertex>& b)
	{
		return (b.first.p - a.first.p).dot(basePoint) > 0.0;
	}
};

int32_t BooleanEvaluator::isPointContainedInMesh(Mesh* msh, const PxVec3& point)
{
	if (msh == nullptr)
	{
		return 0;
	}
	DummyAccelerator dmAccel(msh->getFacetCount());
	mAcceleratorA = &dmAccel;
	return vertexMeshStatus30(point, msh);

}

int32_t BooleanEvaluator::isPointContainedInMesh(Mesh* msh, SpatialAccelerator* spAccel, const PxVec3& point)
{
	if (msh == nullptr)
	{
		return 0;
	}
	mAcceleratorA = spAccel;
	return vertexMeshStatus30(point, msh);
}


bool shouldSwap(const PxVec3& a, const PxVec3& b)
{
	if (a.x < b.x) return false;
	if (a.x > b.x) return true;

	if (a.y < b.y) return false;
	if (a.y > b.y) return true;

	if (a.z < b.z) return false;
	if (a.z > b.z) return true;

	return false;
}

void BooleanEvaluator::buildFaceFaceIntersections(BooleanConf mode)
{
	int32_t statusValue = 0;
	int32_t inclusionValue = 0;

	std::vector<std::pair<Vertex, Vertex> > retainedStarts;
	std::vector<std::pair<Vertex, Vertex>> retainedEnds;
	VertexPairComparator comp;

	Vertex newPointA;
	Vertex newPointB;

	Vertex* meshAPoints = mMeshA->getVertices();
	Vertex* meshBPoints = mMeshB->getVertices();
	EdgeWithParent newEdge;
	mEdgeFacetIntersectionData12.clear();
	mEdgeFacetIntersectionData21.clear();
	
	mEdgeFacetIntersectionData12.resize(mMeshA->getFacetCount());
	mEdgeFacetIntersectionData21.resize(mMeshB->getFacetCount());

	for (uint32_t facetB = 0; facetB < mMeshB->getFacetCount(); ++facetB)
	{
		mAcceleratorA->setState(mMeshB->getVertices(), mMeshB->getEdges(), *mMeshB->getFacet(facetB));
		int32_t facetA = mAcceleratorA->getNextFacet();
		while (facetA != -1)
		{
			Edge* facetBEdges = mMeshB->getEdges() + mMeshB->getFacet(facetB)->firstEdgeNumber;
			Edge* facetAEdges = mMeshA->getEdges() + mMeshA->getFacet(facetA)->firstEdgeNumber;
			Edge* fbe = facetBEdges;
			Edge* fae = facetAEdges;
			retainedStarts.clear();
			retainedEnds.clear();
			PxVec3 compositeEndPoint(0, 0, 0);
			PxVec3 compositeStartPoint(0, 0, 0);
			uint32_t facetAEdgeCount = mMeshA->getFacet(facetA)->edgesCount;
			uint32_t facetBEdgeCount = mMeshB->getFacet(facetB)->edgesCount;
			int32_t ic = 0;
			for (uint32_t i = 0; i < facetAEdgeCount; ++i)
			{
				if (shouldSwap(meshAPoints[fae->e].p, meshAPoints[fae->s].p))
				{
					statusValue = -edgeFacetIntersection12(meshAPoints[fae->e], meshAPoints[fae->s], mMeshB->getVertices(), facetBEdges, facetBEdgeCount, newPointA, newPointB);
				}
				else
				{
					statusValue = edgeFacetIntersection12(meshAPoints[fae->s], meshAPoints[fae->e], mMeshB->getVertices(), facetBEdges, facetBEdgeCount, newPointA, newPointB);
				}
				inclusionValue = -inclusionValueEdgeFace(mode, statusValue);
				if (inclusionValue > 0)
				{
					for (ic = 0; ic < inclusionValue; ++ic)
					{
						retainedEnds.push_back(std::make_pair(newPointA, newPointB));
						compositeEndPoint += newPointA.p;
					}
					mEdgeFacetIntersectionData12[facetA].push_back(EdgeFacetIntersectionData(i, statusValue, newPointA));
				}
				if (inclusionValue < 0)
				{
					for (ic = 0; ic < -inclusionValue; ++ic)
					{
						retainedStarts.push_back(std::make_pair(newPointA, newPointB));
						compositeStartPoint += newPointA.p;
					}
					mEdgeFacetIntersectionData12[facetA].push_back(EdgeFacetIntersectionData(i, statusValue, newPointA));
				}
				fae++;
			}
			for (uint32_t i = 0; i < facetBEdgeCount; ++i)
			{
				if (shouldSwap(meshBPoints[fbe->e].p, meshBPoints[fbe->s].p))
				{
					statusValue = -edgeFacetIntersection21(meshBPoints[(fbe)->e], meshBPoints[(fbe)->s], mMeshA->getVertices(), facetAEdges, facetAEdgeCount, newPointA, newPointB);
				}
				else
				{
					statusValue = edgeFacetIntersection21(meshBPoints[(fbe)->s], meshBPoints[(fbe)->e], mMeshA->getVertices(), facetAEdges, facetAEdgeCount, newPointA, newPointB);
				}
				inclusionValue = inclusionValueEdgeFace(mode, statusValue);
				if (inclusionValue > 0)
				{
					for (ic = 0; ic < inclusionValue; ++ic)
					{
						retainedEnds.push_back(std::make_pair(newPointA, newPointB));
						compositeEndPoint += newPointB.p;
					}
					mEdgeFacetIntersectionData21[facetB].push_back(EdgeFacetIntersectionData( i, statusValue, newPointB));
				}
				if (inclusionValue < 0)
				{
					for (ic = 0; ic < -inclusionValue; ++ic)
					{
						retainedStarts.push_back(std::make_pair(newPointA, newPointB));
						compositeStartPoint += newPointB.p;
					}
					mEdgeFacetIntersectionData21[facetB].push_back(EdgeFacetIntersectionData(i, statusValue, newPointB));
				}
				fbe++;
			}
			if (retainedStarts.size() != retainedEnds.size())
			{
				NVBLAST_LOG_ERROR(mLoggingCallback, "Not equal number of starting and ending vertices! Probably input mesh has open edges.");
				return;
			}
			if (retainedStarts.size() > 1)
			{
				comp.basePoint = compositeEndPoint - compositeStartPoint;
				std::sort(retainedStarts.begin(), retainedStarts.end(), comp);
				std::sort(retainedEnds.begin(), retainedEnds.end(), comp);
			}
			for (uint32_t rv = 0; rv < retainedStarts.size(); ++rv)
			{
				newEdge.s = addIfNotExist(retainedStarts[rv].first);
				newEdge.e = addIfNotExist(retainedEnds[rv].first);
				newEdge.parent = facetA;
				addEdgeIfValid(newEdge);
				newEdge.parent = facetB + mMeshA->getFacetCount();
				newEdge.e = addIfNotExist(retainedStarts[rv].second);
				newEdge.s = addIfNotExist(retainedEnds[rv].second);
				addEdgeIfValid(newEdge);
			}
			facetA = mAcceleratorA->getNextFacet();
		} // while (*iter != -1)

	} // for (uint32_t facetB = 0; facetB < mMeshB->getFacetCount(); ++facetB)



}


void BooleanEvaluator::buildFastFaceFaceIntersection(BooleanConf mode)
{
	int32_t statusValue = 0;
	int32_t inclusionValue = 0;

	std::vector<std::pair<Vertex, Vertex> > retainedStarts;
	std::vector<std::pair<Vertex, Vertex>> retainedEnds;
	VertexPairComparator comp;

	Vertex newPointA;
	Vertex newPointB;

	Vertex* meshAPoints = mMeshA->getVertices();
	EdgeWithParent newEdge;

	mEdgeFacetIntersectionData12.clear();
	mEdgeFacetIntersectionData21.clear();

	mEdgeFacetIntersectionData12.resize(mMeshA->getFacetCount());
	mEdgeFacetIntersectionData21.resize(mMeshB->getFacetCount());

	for (uint32_t facetA = 0; facetA < mMeshA->getFacetCount(); ++facetA)
	{
		Edge* facetAEdges = mMeshA->getEdges() + mMeshA->getFacet(facetA)->firstEdgeNumber;
		int32_t facetB = 0;
			Edge* facetBEdges = mMeshB->getEdges() + mMeshB->getFacet(facetB)->firstEdgeNumber;
			Edge* fae = facetAEdges;
			retainedStarts.clear();
			retainedEnds.clear();
			PxVec3 compositeEndPoint(0, 0, 0);
			PxVec3 compositeStartPoint(0, 0, 0);
			uint32_t facetAEdgeCount = mMeshA->getFacet(facetA)->edgesCount;
			uint32_t facetBEdgeCount = mMeshB->getFacet(facetB)->edgesCount;
			int32_t ic = 0;
			for (uint32_t i = 0; i < facetAEdgeCount; ++i)
			{
				if (shouldSwap(meshAPoints[fae->e].p, meshAPoints[fae->s].p))
				{
					statusValue = -edgeFacetIntersection12(meshAPoints[fae->e], meshAPoints[fae->s], mMeshB->getVertices(), facetBEdges, facetBEdgeCount, newPointA, newPointB);
				}
				else
				{
					statusValue = edgeFacetIntersection12(meshAPoints[fae->s], meshAPoints[fae->e], mMeshB->getVertices(), facetBEdges, facetBEdgeCount, newPointA, newPointB);
				}
				inclusionValue = -inclusionValueEdgeFace(mode, statusValue);
				if (inclusionValue > 0)
				{
					for (ic = 0; ic < inclusionValue; ++ic)
					{
						retainedEnds.push_back(std::make_pair(newPointA, newPointB));
						compositeEndPoint += newPointA.p;
					}
					mEdgeFacetIntersectionData12[facetA].push_back(EdgeFacetIntersectionData(i, statusValue, newPointA));
				}
				if (inclusionValue < 0)
				{
					for (ic = 0; ic < -inclusionValue; ++ic)
					{
						retainedStarts.push_back(std::make_pair(newPointA, newPointB));
						compositeStartPoint += newPointA.p;
					}
					mEdgeFacetIntersectionData12[facetA].push_back(EdgeFacetIntersectionData(i, statusValue, newPointA));
				}
				fae++;
			}
			if (retainedStarts.size() != retainedEnds.size())
			{
				NVBLAST_LOG_ERROR(mLoggingCallback, "Not equal number of starting and ending vertices! Probably input mesh has open edges.");
				return;
			}
			if (retainedStarts.size() > 1)
			{
				comp.basePoint = compositeEndPoint - compositeStartPoint;
				std::sort(retainedStarts.begin(), retainedStarts.end(), comp);
				std::sort(retainedEnds.begin(), retainedEnds.end(), comp);
			}
			for (uint32_t rv = 0; rv < retainedStarts.size(); ++rv)
			{
				newEdge.s = addIfNotExist(retainedStarts[rv].first);
				newEdge.e = addIfNotExist(retainedEnds[rv].first);
				newEdge.parent = facetA;
				addEdgeIfValid(newEdge);
				newEdge.parent = facetB + mMeshA->getFacetCount();
				newEdge.e = addIfNotExist(retainedStarts[rv].second);
				newEdge.s = addIfNotExist(retainedEnds[rv].second);
				addEdgeIfValid(newEdge);
			}
	}

}



void BooleanEvaluator::collectRetainedPartsFromA(BooleanConf mode)
{

	int32_t statusValue = 0;
	int32_t inclusionValue = 0;
	Vertex* vertices = mMeshA->getVertices();
	Vertex newPoint;
	VertexComparator comp;
	PxBounds3& bMeshBoudning = mMeshB->getBoundingBox();
	Edge* facetEdges = mMeshA->getEdges();
	std::vector<Vertex> retainedStartVertices;
	std::vector<Vertex> retainedEndVertices;
	retainedStartVertices.reserve(255);
	retainedEndVertices.reserve(255);
	int32_t ic = 0;
	for (uint32_t facetId = 0; facetId < mMeshA->getFacetCount(); ++facetId)
	{
		retainedStartVertices.clear();
		retainedEndVertices.clear();
		for (uint32_t i = 0; i < mMeshA->getFacet(facetId)->edgesCount; ++i)
		{
			PxVec3 compositeEndPoint(0, 0, 0);
			PxVec3 compositeStartPoint(0, 0, 0);

			int32_t lastPos = static_cast<int32_t>(retainedEndVertices.size());
			/* Test start and end point of edge against mesh */
			if (bMeshBoudning.contains(vertices[facetEdges->s].p))
			{
				statusValue = vertexMeshStatus03(vertices[facetEdges->s].p, mMeshB);
			}
			else
			{
				statusValue = 0;
			}
			inclusionValue = -inclusionValue03(mode, statusValue);

			if (inclusionValue > 0)
			{
				for (ic = 0; ic < inclusionValue; ++ic)
				{
					retainedEndVertices.push_back(vertices[facetEdges->s]);
					compositeEndPoint += vertices[facetEdges->s].p;
				}
			}
			else
			{
				if (inclusionValue < 0)
				{
					for (ic = 0; ic < -inclusionValue; ++ic)
					{
						retainedStartVertices.push_back(vertices[facetEdges->s]);
						compositeStartPoint += vertices[facetEdges->s].p;
					}
				}
			}

			if (bMeshBoudning.contains(vertices[facetEdges->e].p))
			{
				statusValue = vertexMeshStatus03(vertices[facetEdges->e].p, mMeshB);
			}
			else
			{
				statusValue = 0;
			}
			inclusionValue = inclusionValue03(mode, statusValue);
			if (inclusionValue > 0)
			{
				for (ic = 0; ic < inclusionValue; ++ic)
				{
					retainedEndVertices.push_back(vertices[facetEdges->e]);
					compositeEndPoint += vertices[facetEdges->e].p;
				}
			}
			else
			{
				if (inclusionValue < 0)
				{
					for (ic = 0; ic < -inclusionValue; ++ic)
					{
						retainedStartVertices.push_back(vertices[facetEdges->e]);
						compositeStartPoint += vertices[facetEdges->e].p;
					}
				}
			}
			/* Test edge intersection with mesh*/
			for (uint32_t intrs = 0; intrs < mEdgeFacetIntersectionData12[facetId].size(); ++intrs)
			{
				EdgeFacetIntersectionData& intr = mEdgeFacetIntersectionData12[facetId][intrs];
				if (intr.edId != (int32_t)i)
					continue;
				newPoint = intr.intersectionPoint;
				inclusionValue = inclusionValueEdgeFace(mode, intr.intersectionType);

				if (inclusionValue > 0)
				{
					for (ic = 0; ic < inclusionValue; ++ic)
					{
						retainedEndVertices.push_back(newPoint);
						compositeEndPoint += newPoint.p;
					}
				}
				else
				{
					if (inclusionValue < 0)
					{
						for (ic = 0; ic < -inclusionValue; ++ic)
						{
							retainedStartVertices.push_back(newPoint);
							compositeStartPoint += newPoint.p;
						}
					}
				}
			}
			facetEdges++;
			if (retainedStartVertices.size() != retainedEndVertices.size())
			{
				NVBLAST_LOG_ERROR(mLoggingCallback, "Not equal number of starting and ending vertices! Probably input mesh has open edges.");
				return;
			}
			if (retainedEndVertices.size() > 1)
			{
				comp.basePoint = compositeEndPoint - compositeStartPoint;
				std::sort(retainedStartVertices.begin() + lastPos, retainedStartVertices.end(), comp);
				std::sort(retainedEndVertices.begin() + lastPos, retainedEndVertices.end(), comp);
			}
		}


		EdgeWithParent newEdge;
		for (uint32_t rv = 0; rv < retainedStartVertices.size(); ++rv)
		{
			newEdge.s = addIfNotExist(retainedStartVertices[rv]);
			newEdge.e = addIfNotExist(retainedEndVertices[rv]);
			newEdge.parent = facetId;
			addEdgeIfValid(newEdge);
		}
	}

	return;
}

void BooleanEvaluator::collectRetainedPartsFromB(BooleanConf mode)
{
	int32_t statusValue = 0;
	int32_t inclusionValue = 0;
	Vertex* vertices = mMeshB->getVertices();
	Vertex newPoint;
	VertexComparator comp;
	PxBounds3& aMeshBoudning = mMeshA->getBoundingBox();
	Edge* facetEdges = mMeshB->getEdges();
	std::vector<Vertex> retainedStartVertices;
	std::vector<Vertex> retainedEndVertices;
	retainedStartVertices.reserve(255);
	retainedEndVertices.reserve(255);
	int32_t ic = 0;
	for (uint32_t facetId = 0; facetId < mMeshB->getFacetCount(); ++facetId)
	{
		retainedStartVertices.clear();
		retainedEndVertices.clear();
		for (uint32_t i = 0; i < mMeshB->getFacet(facetId)->edgesCount; ++i)
		{
			PxVec3 compositeEndPoint(0, 0, 0);
			PxVec3 compositeStartPoint(0, 0, 0);
			int32_t lastPos = static_cast<int32_t>(retainedEndVertices.size());
			if (aMeshBoudning.contains(vertices[facetEdges->s].p))
			{
				statusValue = vertexMeshStatus30(vertices[facetEdges->s].p, mMeshA);
			}
			else
			{
				statusValue = 0;
			}
			inclusionValue = -inclusionValue30(mode, statusValue);

			if (inclusionValue > 0)
			{
				for (ic = 0; ic < inclusionValue; ++ic)
				{
					retainedEndVertices.push_back(vertices[facetEdges->s]);
					compositeEndPoint += vertices[facetEdges->s].p;
				}

			}
			else
			{
				if (inclusionValue < 0)
				{
					for (ic = 0; ic < -inclusionValue; ++ic)
					{
						retainedStartVertices.push_back(vertices[facetEdges->s]);
						compositeStartPoint += vertices[facetEdges->s].p;
					}

				}
			}

			if (aMeshBoudning.contains(vertices[facetEdges->e].p))
			{
				statusValue = vertexMeshStatus30(vertices[facetEdges->e].p, mMeshA);
			}
			else
			{
				statusValue = 0;
			}
			inclusionValue = inclusionValue30(mode, statusValue);
			if (inclusionValue > 0)
			{
				for (ic = 0; ic < inclusionValue; ++ic)
				{
					retainedEndVertices.push_back(vertices[facetEdges->e]);
					compositeEndPoint += vertices[facetEdges->e].p;
				}

			}
			else
			{
				if (inclusionValue < 0)
				{
					for (ic = 0; ic < -inclusionValue; ++ic)
					{
						retainedStartVertices.push_back(vertices[facetEdges->e]);
						compositeStartPoint += vertices[facetEdges->e].p;
					}

				}
			}
			for (uint32_t intrs = 0; intrs < mEdgeFacetIntersectionData21[facetId].size(); ++intrs)
			{
				EdgeFacetIntersectionData& intr = mEdgeFacetIntersectionData21[facetId][intrs];
				if (intr.edId != (int32_t)i)
					continue;
				newPoint = intr.intersectionPoint;
				inclusionValue = inclusionValueEdgeFace(mode, intr.intersectionType);

				if (inclusionValue > 0)
				{
					for (ic = 0; ic < inclusionValue; ++ic)
					{
						retainedEndVertices.push_back(newPoint);
						compositeEndPoint += newPoint.p;
					}
				}
				else
				{
					if (inclusionValue < 0)
					{
						for (ic = 0; ic < -inclusionValue; ++ic)
						{
							retainedStartVertices.push_back(newPoint);
							compositeStartPoint += newPoint.p;
						}
					}
				}
			}
			facetEdges++;
			if (retainedStartVertices.size() != retainedEndVertices.size())
			{
				NVBLAST_LOG_ERROR(mLoggingCallback, "Not equal number of starting and ending vertices! Probably input mesh has open edges.");
				return;
			}
			if (retainedEndVertices.size() - lastPos > 1)
			{
				comp.basePoint = compositeEndPoint - compositeStartPoint;
				std::sort(retainedStartVertices.begin() + lastPos, retainedStartVertices.end(), comp);
				std::sort(retainedEndVertices.begin() + lastPos, retainedEndVertices.end(), comp);
			}
		}
		EdgeWithParent newEdge;
		for (uint32_t rv = 0; rv < retainedStartVertices.size(); ++rv)
		{
			newEdge.s = addIfNotExist(retainedStartVertices[rv]);
			newEdge.e = addIfNotExist(retainedEndVertices[rv]);
			newEdge.parent = facetId + mMeshA->getFacetCount();
			addEdgeIfValid(newEdge);
		}
	}
	return;
}

bool EdgeWithParentSortComp(const EdgeWithParent& a, const EdgeWithParent& b)
{
	return a.parent < b.parent;
}


void BooleanEvaluator::performBoolean(Mesh* meshA, Mesh* meshB, SpatialAccelerator* spAccelA, SpatialAccelerator* spAccelB, BooleanConf mode)
{
	reset();
	mMeshA = meshA;
	mMeshB = meshB;
	mAcceleratorA = spAccelA;
	mAcceleratorB = spAccelB;
	buildFaceFaceIntersections(mode);
	collectRetainedPartsFromA(mode);
	collectRetainedPartsFromB(mode);
	mAcceleratorA = nullptr;
	mAcceleratorB = nullptr;
}

void BooleanEvaluator::performBoolean(Mesh* meshA, Mesh* meshB, BooleanConf mode)
{
	reset();
	mMeshA = meshA;
	mMeshB = meshB;
	DummyAccelerator ac = DummyAccelerator(mMeshA->getFacetCount());
	DummyAccelerator bc = DummyAccelerator(mMeshB->getFacetCount());
	performBoolean(meshA, meshB, &ac, &bc, mode);
}


void BooleanEvaluator::performFastCutting(Mesh* meshA, Mesh* meshB, SpatialAccelerator* spAccelA, SpatialAccelerator* spAccelB, BooleanConf mode)
{
	reset();
	mMeshA = meshA;
	mMeshB = meshB;
	mAcceleratorA = spAccelA;
	mAcceleratorB = spAccelB;
	buildFastFaceFaceIntersection(mode);
	collectRetainedPartsFromA(mode);
	mAcceleratorA = nullptr;
	mAcceleratorB = nullptr;
}

void BooleanEvaluator::performFastCutting(Mesh* meshA, Mesh* meshB, BooleanConf mode)
{
	reset();
	mMeshA = meshA;
	mMeshB = meshB;
	DummyAccelerator ac = DummyAccelerator(mMeshA->getFacetCount());
	DummyAccelerator bc = DummyAccelerator(mMeshB->getFacetCount());
	performFastCutting(meshA, meshB, &ac, &bc, mode);
}




BooleanEvaluator::BooleanEvaluator(NvBlastLog loggingCallback)
{
	mMeshA = nullptr;
	mMeshB = nullptr;
	mAcceleratorA = nullptr;
	mAcceleratorB = nullptr;
	mLoggingCallback = loggingCallback;
}
BooleanEvaluator::~BooleanEvaluator()
{
	reset();
}



Mesh* BooleanEvaluator::createNewMesh()
{
	if (mEdgeAggregate.size() == 0)
	{
		return nullptr;
	}
	std::sort(mEdgeAggregate.begin(), mEdgeAggregate.end(), EdgeWithParentSortComp);
	std::vector<Facet> newFacets;
	std::vector<Edge>  newEdges(mEdgeAggregate.size());
	int32_t lastPos = 0;
	int32_t lastParent = mEdgeAggregate[0].parent;
	uint32_t collected = 0;
	int32_t userData = 0;
	for (uint32_t i = 0; i < mEdgeAggregate.size(); ++i)
	{
		if (mEdgeAggregate[i].parent != lastParent)
		{			
			if (lastParent < (int32_t)mMeshA->getFacetCount())
			{
				userData = mMeshA->getFacet(lastParent)->userData;
			}
			else
			{
				userData = mMeshB->getFacet(lastParent - mMeshA->getFacetCount())->userData;
			}
			newFacets.push_back(Facet(lastPos, collected, userData));
			lastPos = i;
			lastParent = mEdgeAggregate[i].parent;
			collected = 0;
		}
		collected++;
		newEdges[i].s = mEdgeAggregate[i].s;
		newEdges[i].e = mEdgeAggregate[i].e;
	}
	int32_t pr = lastParent - mMeshA->getFacetCount();
	if (lastParent < (int32_t)mMeshA->getFacetCount())
	{
		userData = mMeshA->getFacet(lastParent)->userData;
	}
	else
	{
		userData = mMeshB->getFacet(pr)->userData;
	}
	newFacets.push_back(Facet(lastPos, collected, userData));
	return new Mesh(&mVerticesAggregate[0], &newEdges[0], &newFacets[0], static_cast<uint32_t>(mVerticesAggregate.size()), static_cast<uint32_t>(mEdgeAggregate.size()), static_cast<uint32_t>(newFacets.size()));
}

void BooleanEvaluator::reset()
{
	mMeshA			= nullptr;
	mMeshB			= nullptr;
	mAcceleratorA	= nullptr;
	mAcceleratorB	= nullptr;
	mEdgeAggregate.clear();
	mVerticesAggregate.clear();
	mEdgeFacetIntersectionData12.clear();
	mEdgeFacetIntersectionData21.clear();
}

} // namespace Blast
} // namespace Nv
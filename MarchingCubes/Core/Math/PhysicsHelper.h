#pragma once
#include "Core/Geometry/Mesh/Mesh.h"
#include <stdexcept>
#include <DirectXMath.h>

namespace PhysicsUtil
{
	using namespace DirectX;

	static bool IntersectTriangle(const XMVECTOR& rayOrigin, const XMVECTOR& rayDir, const XMVECTOR& v0, const XMVECTOR& v1, const XMVECTOR& v2, float& outDist)
	{
		//Möller–Trumbore Algorithm
		const float EPS = 1e-6f;
		XMVECTOR edge1 = XMVectorSubtract(v1, v0);
		XMVECTOR edge2 = XMVectorSubtract(v2, v0);
		XMVECTOR pvec = XMVector3Cross(rayDir, edge2);
		float det = XMVectorGetX(XMVector3Dot(edge1, pvec));
		if (fabsf(det) < EPS)
			return false;  // 평행

		float invDet = 1.0f / det;
		XMVECTOR tvec = XMVectorSubtract(rayOrigin, v0);
		float u = XMVectorGetX(XMVector3Dot(tvec, pvec)) * invDet;
		if (u < 0.0f || u > 1.0f)
			return false;

		XMVECTOR qvec = XMVector3Cross(tvec, edge1);
		float v = XMVectorGetX(XMVector3Dot(rayDir, qvec)) * invDet;
		if (v < 0.0f || u + v > 1.0f)
			return false;

		float tVal = XMVectorGetX(XMVector3Dot(edge2, qvec)) * invDet;
		if (tVal <= EPS)
			return false;

		outDist = tVal;
		return true;
	}
	
	static bool IsHit(const XMVECTOR& rayOrigin, const XMVECTOR& rayDir, const Mesh& mesh, XMFLOAT3& outHitpos)
	{
		// TriBounds는 Local Space
		// World Space Ray -> Local Space Ray 변환
		XMMATRIX invWorld = mesh.GetWorldInvMatrix();
		XMVECTOR rayOriginLocal = XMVector3TransformCoord(rayOrigin, invWorld);
		XMVECTOR rayDirLocal = XMVector3Normalize(XMVector3TransformNormal(rayDir, invWorld));

		float closestDist = FLT_MAX;
		XMVECTOR hitLocal = XMVectorZero();
		bool hitFound = false;

		const GeometryData* meshData = mesh.GetCPUData();
		const auto& bounds = mesh.GetBounds();
		const auto& I = meshData->indices;
		size_t triCount = I.size() / 3;

		for (size_t t = 0; t < triCount; ++t)
		{
			float distFromBoundingBox;

			// Mesh의 worldSpace Transform이 적용되지 않음.
			if (!bounds[t].Intersects(rayOriginLocal, rayDirLocal, distFromBoundingBox) || distFromBoundingBox > closestDist)
			{
				continue;  // 이 삼각형은 볼 필요 없음
			}

			XMVECTOR v0 = XMLoadFloat3(&meshData->vertices[I[3 * t + 0]].pos);
			XMVECTOR v1 = XMLoadFloat3(&meshData->vertices[I[3 * t + 1]].pos);
			XMVECTOR v2 = XMLoadFloat3(&meshData->vertices[I[3 * t + 2]].pos);

			float dist;
			if (IntersectTriangle(rayOriginLocal, rayDirLocal, v0, v1, v2, dist) && dist < closestDist)
			{
				closestDist = dist;
				hitLocal = XMVectorAdd(rayOriginLocal, XMVectorScale(rayDirLocal, dist));
				hitFound = true;
			}
		}

		if (!hitFound)
			return false;

		XMVECTOR hitWorld = XMVector3TransformCoord(hitLocal, mesh.GetWorldMatrix());
		XMStoreFloat3(&outHitpos, hitWorld);

		return true;
	}

	/*
	* 여러 MeshData의 집합체에 대한 RayCast
	* NOTE : 좌표계 통일 할 것
	*/
	static bool IsHit(const XMVECTOR& rayOrigin, const XMVECTOR& rayDir, const std::vector<const GeometryData*>& meshData, const std::vector<BoundingBox>& bounds, XMFLOAT3& outHitpos)
	{
		float closestDist = FLT_MAX;
		XMVECTOR hitLocal = XMVectorZero();
		bool hitFound = false;

		for (size_t i = 0; i < meshData.size(); ++i)
		{
			float distFromBoundingBox;

			if (!bounds[i].Intersects(rayOrigin, rayDir, distFromBoundingBox) || distFromBoundingBox > closestDist)
			{
				continue;  // 이 MeshData는 볼 필요 없음
			}

			const GeometryData* md = meshData[i];
			const auto& I = md->indices;
			size_t triCount = I.size() / 3;

			for (size_t t = 0; t < triCount; ++t)
			{
				XMVECTOR v0 = XMLoadFloat3(&md->vertices[I[3 * t + 0]].pos);
				XMVECTOR v1 = XMLoadFloat3(&md->vertices[I[3 * t + 1]].pos);
				XMVECTOR v2 = XMLoadFloat3(&md->vertices[I[3 * t + 2]].pos);

				float dist;
				if (IntersectTriangle(rayOrigin, rayDir, v0, v1, v2, dist) && dist < closestDist)
				{
					closestDist = dist;
					hitLocal = XMVectorAdd(rayOrigin, XMVectorScale(rayDir, dist));
					hitFound = true;
				}
			}
		}

		if (!hitFound)
			return false;

		XMStoreFloat3(&outHitpos, hitLocal);
		return true;
	}

	static void MakeRay(const float mouseX, const float mouseY, const Camera& cam, XMVECTOR& outRayOrigin, XMVECTOR& outRayDir)
	{
		// ScreenSpace -> NDC Space
		const float viewportWidth = cam.GetViewportWidth();
		const float viewportHeight = cam.GetViewportHeight();
		float ndcX = (2.0f * mouseX / viewportWidth) - 1.0f;
		float ndcY = 1.0f - (2.0f * mouseY / viewportHeight);

		// NDC Space -> World Space
		XMMATRIX invViewProj = XMMatrixInverse(nullptr, cam.GetViewProjMatrix());
		XMVECTOR nearNDC = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
		XMVECTOR farNDC = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);
		XMVECTOR worldNear = XMVector3TransformCoord(nearNDC, invViewProj);
		XMVECTOR worldFar = XMVector3TransformCoord(farNDC, invViewProj);
		outRayOrigin = worldNear;
		outRayDir = XMVector3Normalize(worldFar - worldNear);
	}
}
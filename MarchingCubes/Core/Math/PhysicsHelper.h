#pragma once
#include "Core/Geometry/Mesh/Mesh.h"
#include <DirectXMath.h>
#include <Core\Rendering\Camera.h>

namespace PhysicsUtil
{
	using namespace DirectX;

	struct RaycastTarget
	{
		const GeometryData* data = nullptr;
		BoundingBox bounds;
		const XMMATRIX& worldMatrix;
	};

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
	
	static bool IsHit(const Mesh* mesh, const XMVECTOR& rayOriginWorld, const XMVECTOR& rayDirWorld, const XMMATRIX& worldMatrix, XMFLOAT3& outHitposWorld)
	{
		if (!mesh) return false;

		// Ray를 로컬 공간으로 변환
		XMMATRIX invWorld = XMMatrixInverse(nullptr, worldMatrix);
		XMVECTOR rayOriginLocal = XMVector3TransformCoord(rayOriginWorld, invWorld);
		XMVECTOR rayDirLocal = XMVector3TransformNormal(rayDirWorld, invWorld);
		rayDirLocal = XMVector3Normalize(rayDirLocal);

		float closestDist = FLT_MAX;
		bool hitFound = false;

		const GeometryData* meshData = mesh->GetCPUData();
		const auto& bounds = mesh->GetBounds();
		const auto& submeshes = mesh->GetSubmeshes();
		for (size_t i = 0; i < submeshes.size(); ++i)
		{
			float distFromBoundingBox;
			if (i < bounds.size() && (!bounds[i].Intersects(rayOriginLocal, rayOriginWorld, distFromBoundingBox) || distFromBoundingBox > closestDist))
				continue;

			const auto& sm = submeshes[i];
			const auto& indices = meshData->indices;
			const auto& vertices = meshData->vertices;

			for (size_t k = 0; k < sm.indexCount / 3; ++k)
			{
				uint32_t idx0 = indices[sm.indexOffset + k * 3 + 0];
				uint32_t idx1 = indices[sm.indexOffset + k * 3 + 1];
				uint32_t idx2 = indices[sm.indexOffset + k * 3 + 2];

				XMVECTOR v0 = XMLoadFloat3(&vertices[idx0].pos);
				XMVECTOR v1 = XMLoadFloat3(&vertices[idx1].pos);
				XMVECTOR v2 = XMLoadFloat3(&vertices[idx2].pos);

				float dist;
				if (IntersectTriangle(rayOriginLocal, rayOriginWorld, v0, v1, v2, dist))
				{
					if (dist < closestDist)
					{
						closestDist = dist;
						hitFound = true;
					}
				}
			}
		}
	
		if (!hitFound) return false;

		XMVECTOR hitLocal = XMVectorAdd(rayOriginLocal, XMVectorScale(rayDirLocal, closestDist));
		XMVECTOR hitWorld = XMVector3TransformCoord(hitLocal, worldMatrix);
		XMStoreFloat3(&outHitposWorld, hitWorld);
		return true;
	}

	/*
	* 여러 MeshData의 집합체에 대한 RayCast
	* NOTE : 좌표계 통일 할 것
	*/
	static bool IsHit(const std::vector<RaycastTarget>& targets, const XMVECTOR& rayOriginWorld, const XMVECTOR& rayDirWorld, XMFLOAT3& outHitposWorld)
	{
		float closestDist = FLT_MAX;
		int hitIndex = -1;

		for (int i = 0; i < targets.size(); ++i)
		{
			const auto& target = targets[i];
			if (!target.data) continue;

			float distFromBox;
			if (!target.bounds.Intersects(rayOriginWorld, rayDirWorld, distFromBox) || distFromBox > closestDist) continue;

			const auto& vertices = target.data->vertices;
			const auto& indices = target.data->indices;
			for (size_t t = 0; t < indices.size() / 3; ++t)
			{
				XMVECTOR v0 = XMLoadFloat3(&vertices[indices[3 * t + 0]].pos);
				XMVECTOR v1 = XMLoadFloat3(&vertices[indices[3 * t + 1]].pos);
				XMVECTOR v2 = XMLoadFloat3(&vertices[indices[3 * t + 2]].pos);

				float dist;
				if (IntersectTriangle(rayOriginWorld, rayDirWorld, v0, v1, v2, dist))
				{
					if (dist < closestDist)
					{
						closestDist = dist;
						hitIndex = i;
					}
				}
			}
		}

		if (hitIndex < 0) return false;

		XMVECTOR hitLocal = XMVectorAdd(rayOriginWorld, XMVectorScale(rayDirWorld, closestDist));
		XMVECTOR hitWorld = XMVector3TransformCoord(hitLocal, targets[hitIndex].worldMatrix);
		XMStoreFloat3(&outHitposWorld, hitWorld);
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
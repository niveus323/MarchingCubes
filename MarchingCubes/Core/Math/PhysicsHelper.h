#pragma once
#include "Core/Geometry/Mesh/Mesh.h"
#include <DirectXMath.h>

namespace PhysicsUtil
{
	using namespace DirectX;
	
	struct Ray
	{
		DirectX::XMVECTOR origin;
		DirectX::XMVECTOR direction;
	};

	struct RaycastTarget
	{
		const GeometryData* data = nullptr;
		BoundingBox bounds;
		const XMMATRIX& worldMatrix;
		void* userData = nullptr; //어떤 오브젝트가 맞았는지 체크를 위한 객체 정보
	};

	struct RaycastHitResult
	{
		bool hasHit = false;            // 충돌 여부
		float distance = FLT_MAX;       // 충돌 거리
		XMFLOAT3 hitPos = { 0, 0, 0 };  // 월드 공간 충돌 좌표
		void* userData = nullptr;       // 충돌한 대상
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
	
	// NOTE : 해당 함수는 Local Space 계산에 유의.
	static bool IsHitRaw(const GeometryData& data, 
		const std::vector<MeshSubmesh>& submeshes,
		const std::vector<DirectX::BoundingBox>& bounds, 
		const DirectX::XMVECTOR& rayOriginLS,
		const DirectX::XMVECTOR& rayDirLS,
		float& outDist)
	{
		outDist = FLT_MAX;
		bool hitFound = false;

		for (size_t i = 0; i < submeshes.size(); ++i)
		{
			// Bounding Box AABB 체크
			float distFromBoundingBox;
			if (i < bounds.size() && (!bounds[i].Intersects(rayOriginLS, rayDirLS, distFromBoundingBox) || distFromBoundingBox > outDist))
				continue;

			const auto& sm = submeshes[i];
			const auto& indices = data.indices;
			const auto& vertices = data.vertices;

			for (size_t k = 0; k < sm.indexCount / 3; ++k)
			{
				uint32_t idx0 = indices[sm.indexOffset + k * 3 + 0];
				uint32_t idx1 = indices[sm.indexOffset + k * 3 + 1];
				uint32_t idx2 = indices[sm.indexOffset + k * 3 + 2];

				XMVECTOR v0 = XMLoadFloat3(&vertices[idx0].pos);
				XMVECTOR v1 = XMLoadFloat3(&vertices[idx1].pos);
				XMVECTOR v2 = XMLoadFloat3(&vertices[idx2].pos);

				float dist = FLT_MAX;
				if (IntersectTriangle(rayOriginLS, rayDirLS, v0, v1, v2, dist))
				{
					if (dist < outDist)
					{
						outDist = dist;
						hitFound = true;
					}
				}
			}
		}

		if (!hitFound) return false;
	}

	/*
	* 여러 MeshData의 집합체에 대한 RayCast
	* NOTE : Target의 GeometryData는 Local Space(MeshAsset 데이터)로 통일
	*/
	static bool IsHit(const std::vector<RaycastTarget>& targets, const Ray& rayWS, RaycastHitResult& outResult)
	{
		outResult = RaycastHitResult(); // 결과 초기화
		float closestDist = FLT_MAX;
		int hitTargetIndex = -1;

		for (int i = 0; i < targets.size(); ++i)
		{
			const auto& target = targets[i];
			if (!target.data) continue;

			// Ray를 로컬 공간으로 변환
			XMMATRIX invWorld = XMMatrixInverse(nullptr, target.worldMatrix);
			XMVECTOR rayOriginLS = XMVector3TransformCoord(rayWS.origin, invWorld);
			XMVECTOR rayDirLS = XMVector3TransformNormal(rayWS.direction, invWorld);
			rayDirLS = XMVector3Normalize(rayDirLS);

			// Bound AABB 체크
			float distBox;
			if (!target.bounds.Intersects(rayOriginLS, rayDirLS, distBox) || distBox > closestDist) continue;

			// Triangle 체크
			const auto& vertices = target.data->vertices;
			const auto& indices = target.data->indices;

			// 인덱스 전체 순회 (서브메쉬 구분 없이 통으로 검사)
			size_t triCount = indices.size() / 3;
			for (size_t t = 0; t < triCount; ++t)
			{
				XMVECTOR v0 = XMLoadFloat3(&vertices[indices[3 * t + 0]].pos);
				XMVECTOR v1 = XMLoadFloat3(&vertices[indices[3 * t + 1]].pos);
				XMVECTOR v2 = XMLoadFloat3(&vertices[indices[3 * t + 2]].pos);

				float distLocal;
				if (IntersectTriangle(rayOriginLS, rayDirLS, v0, v1, v2, distLocal))
				{
					// 로컬 -> 월드 변환 후 체크
					XMVECTOR hitLocal = XMVectorAdd(rayOriginLS, XMVectorScale(rayDirLS, distLocal));
					XMVECTOR hitWorld = XMVector3TransformCoord(hitLocal, target.worldMatrix);

					float distWorld = XMVectorGetX(XMVector3Length(XMVectorSubtract(hitWorld, rayWS.origin)));
					if (distWorld < closestDist)
					{
						closestDist = distWorld;
						hitTargetIndex = i;

						// 결과 갱신
						outResult.hasHit = true;
						outResult.distance = distWorld;
						XMStoreFloat3(&outResult.hitPos, hitWorld);
						outResult.userData = target.userData;
					}
				}
			}
		}

		return outResult.hasHit;
	}

	static Ray MakeRay(const float mouseX, const float mouseY, const float viewportWidth, const float viewportHeight, const DirectX::XMMATRIX& viewproj)
	{
		// ScreenSpace -> NDC Space
		float ndcX = (2.0f * mouseX / viewportWidth) - 1.0f;
		float ndcY = 1.0f - (2.0f * mouseY / viewportHeight);

		// NDC Space -> World Space
		XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewproj);
		XMVECTOR nearNDC = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
		XMVECTOR farNDC = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);
		XMVECTOR worldNear = XMVector3TransformCoord(nearNDC, invViewProj);
		XMVECTOR worldFar = XMVector3TransformCoord(farNDC, invViewProj);

		return Ray{
			.origin = worldNear,
			.direction = XMVector3Normalize(worldFar - worldNear)
		};
	}
}
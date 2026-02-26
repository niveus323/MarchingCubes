#pragma once
#include "Core/Geometry/Mesh/Class/Mesh.h"
#include "Core/Geometry/MarchingCubes/SdfField.h"
#include <DirectXMath.h>

namespace PhysicsUtil
{
	using namespace DirectX;
	
	struct Ray
	{
		DirectX::XMVECTOR origin;
		DirectX::XMVECTOR direction;
	};

	struct HitResult
	{
		bool hasHit = false;						// 충돌 여부
		float distance = FLT_MAX;					// 충돌 거리
		XMFLOAT3 hitPos = { 0, 0, 0 };				// 월드 공간 충돌 좌표
		XMFLOAT3 hitNormal = { 1.0f, 1.0f, 1.0f };	// 월드 공간 충돌 법선
		void* userData = nullptr;					// 충돌한 대상
	};

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

	struct RaycastTarget
	{
		const GeometryData* data = nullptr;
		BoundingBox bounds;
		const XMMATRIX& worldMatrix;
		void* userData = nullptr; //어떤 오브젝트가 맞았는지 체크를 위한 객체 정보
	};

	/*
	* 여러 MeshData의 집합체에 대한 RayCast
	* NOTE : Target의 GeometryData는 Local Space(MeshAsset 데이터)로 통일
	*/
	static bool IsHit(const std::vector<RaycastTarget>& targets, const Ray& rayWS, HitResult& outResult)
	{
		outResult = HitResult(); // 결과 초기화
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
						XMStoreFloat3(&outResult.hitNormal, XMVector3Cross(v1 - v0, v2 - v0));
						outResult.userData = target.userData;
					}
				}
			}
		}

		return outResult.hasHit;
	}

	struct RaymarchingTarget
	{
		const SdfField* data = nullptr;
		const XMUINT3 resolution{};
		const float cellSize = 1.0f;
		const float isoValue = 0.0f;
		const XMMATRIX worldMatrix;
		void* userData = nullptr;
	};

	static float SampleTrilinear(const SdfField* data, const XMUINT3& res, const XMFLOAT3& gridPos)
	{
		// 경계 처리
		float x = std::max(0.0f, std::min(gridPos.x, (float)res.x - 1.001f));
		float y = std::max(0.0f, std::min(gridPos.y, (float)res.y - 1.001f));
		float z = std::max(0.0f, std::min(gridPos.z, (float)res.z - 1.001f));

		int x0 = (int)x; 
		int y0 = (int)y; 
		int z0 = (int)z;

		int x1 = x0 + 1; 
		int y1 = y0 + 1; 
		int z1 = z0 + 1;

		float fx = x - x0; 
		float fy = y - y0; 
		float fz = z - z0;
		
		// 8개 코너 값
		float c000 = data->at(x0, y0, z0); float c100 = data->at(x1, y0, z0);
		float c010 = data->at(x0, y1, z0); float c110 = data->at(x1, y1, z0);
		float c001 = data->at(x0, y0, z1); float c101 = data->at(x1, y0, z1);
		float c011 = data->at(x0, y1, z1); float c111 = data->at(x1, y1, z1);

		// 보간
		float i00 = c000 * (1.0f - fx) + c100 * fx;
		float i10 = c010 * (1.0f - fx) + c110 * fx;
		float i01 = c001 * (1.0f - fx) + c101 * fx;
		float i11 = c011 * (1.0f - fx) + c111 * fx;

		float i0 = i00 * (1.0f - fy) + i10 * fy;
		float i1 = i01 * (1.0f - fy) + i11 * fy;

		return i0 * (1.0f - fz) + i1 * fz;
	}

	static XMVECTOR CalculateSDFNormal(const SdfField* data, const XMUINT3& res, const XMFLOAT3& gridPos)
	{
		const float eps = 0.5f; // 간격
		float dx = SampleTrilinear(data, res, { gridPos.x + eps, gridPos.y, gridPos.z }) - SampleTrilinear(data, res, { gridPos.x - eps, gridPos.y, gridPos.z });
		float dy = SampleTrilinear(data, res, { gridPos.x, gridPos.y + eps, gridPos.z }) - SampleTrilinear(data, res, { gridPos.x, gridPos.y - eps, gridPos.z });
		float dz = SampleTrilinear(data, res, { gridPos.x, gridPos.y, gridPos.z + eps }) - SampleTrilinear(data, res, { gridPos.x, gridPos.y, gridPos.z - eps });

		XMVECTOR n = XMVectorSet(dx, dy, dz, 0.0f);
		return XMVector3Normalize(n);
	}

	static bool RaymarchingSDF(const RaymarchingTarget& target, const Ray& rayLS, float startDist, HitResult& outResult)
	{
		const int MAX_STEPS = 128;
		const float MIN_STEP = target.cellSize * 0.05f;
		float t = startDist;
		XMFLOAT3 boundsSize = {
			target.resolution.x * target.cellSize,
			target.resolution.y * target.cellSize,
			target.resolution.z * target.cellSize
		};
		float maxT = sqrtf(boundsSize.x * boundsSize.x + boundsSize.y * boundsSize.y + boundsSize.z * boundsSize.z) * 1.5f;

		// 시작 위치
		XMVECTOR startPosVec = rayLS.origin + rayLS.direction * t;
		XMFLOAT3 startPosLS;
		XMStoreFloat3(&startPosLS, startPosVec);
		XMFLOAT3 startGridPos = { startPosLS.x / target.cellSize, startPosLS.y / target.cellSize, startPosLS.z / target.cellSize };

		// 루프 시작 전 첫 샘플링
		float prevVal = SampleTrilinear(target.data, target.resolution, startGridPos);
		// SDF 값은 IsoValue 기준 상대 값으로 변환하여 부호 체크를 단순화
		prevVal -= target.isoValue;

		for (int i = 0; i < MAX_STEPS; ++i)
		{
			if (t > maxT) break;

			XMVECTOR posVec = rayLS.origin + rayLS.direction * t;
			XMFLOAT3 posLS;
			DirectX::XMStoreFloat3(&posLS, posVec);
			XMFLOAT3 gridPos = { posLS.x / target.cellSize, posLS.y / target.cellSize, posLS.z / target.cellSize };

			// 범위 체크 (Grid 밖이면 약간 전진하고 continue)
			if (gridPos.x < 0 || gridPos.y < 0 || gridPos.z < 0 ||
				gridPos.x >= target.resolution.x || gridPos.y >= target.resolution.y || gridPos.z >= target.resolution.z)
			{
				t += target.cellSize;
				continue;
			}
			// 샘플링 (보간)
			float rawval = SampleTrilinear(target.data, target.resolution, gridPos);
			float currDistSigned = rawval - target.isoValue; // IsoSurface 기준 거리

			if (std::abs(rawval) < target.isoValue + FLT_EPSILON)
			{
				outResult.hasHit = true;
				outResult.distance = t;

				XMVECTOR hitLocal = posVec;
				XMVECTOR hitWorld = XMVector3TransformCoord(hitLocal, target.worldMatrix);
				XMStoreFloat3(&outResult.hitPos, hitWorld);

				XMVECTOR normalGS = CalculateSDFNormal(target.data, target.resolution, gridPos);
				XMVECTOR normalWS = XMVector3TransformNormal(normalGS, target.worldMatrix);
				XMStoreFloat3(&outResult.hitNormal, XMVector3Normalize(normalWS));

				outResult.userData = target.userData;
				return true;
			}

			// 이전 값과 현재 값의 부호가 다르면 그 사이에 표면이 존재함.
			if (prevVal * currDistSigned < 0.0f)
			{
				outResult.hasHit = true;
				outResult.distance = t;

				// Hit Pos 재계산
				XMVECTOR hitLocal = posVec;
				XMVECTOR hitWorld = XMVector3TransformCoord(hitLocal, target.worldMatrix);
				XMStoreFloat3(&outResult.hitPos, hitWorld);

				XMVECTOR normalGS = CalculateSDFNormal(target.data, target.resolution, gridPos);
				XMVECTOR normalWS = XMVector3TransformNormal(normalGS, target.worldMatrix);
				XMStoreFloat3(&outResult.hitNormal, XMVector3Normalize(normalWS));
				outResult.userData = target.userData;

				return true;
			}

			float distWorld = std::abs(rawval - target.isoValue) * target.cellSize;
			float step = std::max(distWorld * 0.9f, MIN_STEP); // 완화계수 0.9

			t += step;
			prevVal = currDistSigned;
		}

		return false;
	}

	static bool IsHit(const RaymarchingTarget& target, const Ray& rayWS, HitResult& outResult)
	{
		outResult = HitResult();
		const int MAX_STEPS = 64;
		float closestDist = FLT_MAX;

		if (!target.data) return false;

		// Ray를 로컬 공간으로 변환
		XMMATRIX invWorld = XMMatrixInverse(nullptr, target.worldMatrix);
		XMVECTOR rayOriginLS = XMVector3TransformCoord(rayWS.origin, invWorld);
		XMVECTOR rayDirLS = XMVector3TransformNormal(rayWS.direction, invWorld);
		rayDirLS = XMVector3Normalize(rayDirLS);

		// 사전 AABB 검사
		XMFLOAT3 gridSize = {
			target.resolution.x * target.cellSize,
			target.resolution.y * target.cellSize,
			target.resolution.z * target.cellSize
		};
		BoundingBox gridBounds(
			XMFLOAT3(gridSize.x * 0.5f, gridSize.y * 0.5f, gridSize.z * 0.5f), // Center
			XMFLOAT3(gridSize.x * 0.5f, gridSize.y * 0.5f, gridSize.z * 0.5f)  // Extents
		);

		float distBox;
		// 가장 가까운 BoundingBox Hit 체크
		if (!gridBounds.Intersects(rayOriginLS, rayDirLS, distBox) || distBox > outResult.distance) 
			return false;

		HitResult tempResult{};
		float startDist = std::max(0.0f, distBox); // 시작 지점은 BoundingBox 표면

		if (RaymarchingSDF(target, Ray{ .origin = rayOriginLS, .direction = rayDirLS }, startDist, tempResult))
		{
			// Scale이 1이라면 문제없지만, Scale이 있다면 t * Scale을 해야 정확한 World Distance임.
			// 여기서는 Uniform Scale을 가정하고 t (Local Distance)를 비교에 사용하거나
			// 정확하게는 WorldHitPos와 RayOrigin 간의 거리를 재계산하는 것이 안전함.
			float distWorld = XMVectorGetX(XMVector3Length(XMLoadFloat3(&tempResult.hitPos) - rayWS.origin));
			tempResult.distance = distWorld;

			if (distWorld < outResult.distance)
			{
				outResult = tempResult;
			}
		}
		return true;
	}

	static bool IsHit(const std::vector<RaymarchingTarget>& targets, const Ray& rayWS, HitResult& outResult)
	{
		outResult = HitResult(); // 결과 초기화
		const int MAX_STEPS = 64;
		float closestDist = FLT_MAX;
		int hitTargetIndex = -1;

		for (const auto& target : targets)
		{
			if (!target.data) continue;

			// Ray를 로컬 공간으로 변환
			XMMATRIX invWorld = XMMatrixInverse(nullptr, target.worldMatrix);
			XMVECTOR rayOriginLS = XMVector3TransformCoord(rayWS.origin, invWorld);
			XMVECTOR rayDirLS = XMVector3TransformNormal(rayWS.direction, invWorld);
			rayDirLS = XMVector3Normalize(rayDirLS);
			
			// 사전 AABB 검사
			XMFLOAT3 gridSize = {
				target.resolution.x * target.cellSize,
				target.resolution.y * target.cellSize,
				target.resolution.z * target.cellSize
			};
			BoundingBox gridBounds(
				XMFLOAT3(gridSize.x * 0.5f, gridSize.y * 0.5f, gridSize.z * 0.5f), // Center
				XMFLOAT3(gridSize.x * 0.5f, gridSize.y * 0.5f, gridSize.z * 0.5f)  // Extents
			);

			float distBox;
			// 가장 가까운 BoundingBox Hit 체크
			if (!gridBounds.Intersects(rayOriginLS, rayDirLS, distBox) || distBox > outResult.distance) continue;

			HitResult tempResult{};
			float startDist = std::max(0.0f, distBox); // 시작 지점은 BoundingBox 표면

			if (RaymarchingSDF(target, Ray{ .origin = rayOriginLS, .direction = rayDirLS }, startDist, tempResult))
			{
				// Scale이 1이라면 문제없지만, Scale이 있다면 t * Scale을 해야 정확한 World Distance임.
				// 여기서는 Uniform Scale을 가정하고 t (Local Distance)를 비교에 사용하거나
				// 정확하게는 WorldHitPos와 RayOrigin 간의 거리를 재계산하는 것이 안전함.
				float distWorld = XMVectorGetX(XMVector3Length(XMLoadFloat3(&tempResult.hitPos) - rayWS.origin));
				tempResult.distance = distWorld;

				if (distWorld < outResult.distance)
				{
					outResult = tempResult;
				}
			}
		}

		return outResult.hasHit;
	}

}
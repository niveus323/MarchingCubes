#pragma once
namespace MathHelper
{
	constexpr float Epsilon = 1e-5f;            // 일반적인 스케일/위치 오차 판정용
	constexpr float TolerantEpsilon = 1e-3f;    // 방향 벡터나 회전(LookTo) 판정용
}
#pragma once
#include <array>

struct Particle
{
	XMFLOAT3 pos; //r
	XMVECTOR vel; //v
	XMVECTOR force = { 0.0f, 0.0f, 0.0f }; //f
	float mass = 1.0f; //m
	float density = 0.0f; // rho
	float pressure = 0.0f;
	XMFLOAT4 color = { 1.0f, 0.0f, 0.0f, 0.4f };
	XMFLOAT3 normal = { 0.0f, 0.0f, 0.0f };

	Particle() = default;
	Particle(const XMFLOAT3& p, const XMVECTOR& v, float m = 1.0f) : pos(p), vel(v), mass(m) {}
};

struct FluidParams 
{
	float density_zero = 1000.0f; //ρ_0 (기준 밀도)
	float stiffness = 2000.0f; // κ (이상기체 방정식 참고)
	float viscocityFactor = 0.1f; // μ (점성힘 계수)
	float surfaceTension = 0.0f; // σ (표면장력힘 계수)
	float smoothingLength = 0.045f; // h
	XMFLOAT3 gravity = { 0.0f, -9.8f, 0.0f };
	float defaultMass = 0.02f; 
	float boundaryDamping = 0.5f;
	float timeStepLimit = 0.005f;
	bool enableSurfaceTension = false;
	//float restitution = 0.3f;   // 복원계수(반사 강도) 0..1, 물은 낮게
	//float friction = 0.85f;  // 접선 감쇠(마찰)     0..1
	//float penEps = 1e-4f;  // 경계 안쪽으로 밀어넣는 보정 오프셋
	//float velEps = 1e-3f;  // 너무 작은 진동 제거 임계값
};

class FluidSystem
{
public:
	FluidSystem(const XMFLOAT3& min, const XMFLOAT3& max, const FluidParams& params = FluidParams());
	void AddParticle(const XMFLOAT3& pos, const XMVECTOR& vel = { 0.0f,0.0f,0.0f }, float mass = -1.0f);
	void Step(float dt);
	void SetSmoothingLength(float h);
	void ClearParticles();

	std::vector<Particle>& GetParticles() { return m_particles; }

private:
	void BuildGrid();
	void ClearGridBuckets();
	void InsertParticlesToGrid();
	void ComputePressure();
	void ComputeForces();
	void Integrate(float dt);
	void ApplyBoundaryConditions();
	void GetNeighbors(int pi, std::vector<int>& outNeighbors) const;

	inline int CellIndexFromCoord(int ix, int iy, int iz) const {
		if (ix < 0 || iy < 0 || iz < 0) return -1;
		if (ix >= m_gridDim[0] || iy >= m_gridDim[1] || iz >= m_gridDim[2]) return -1;
		return ix + iy * m_gridDim[0] + iz * m_gridDim[0] * m_gridDim[1];
	}

	inline void PositionToCell(const XMFLOAT3& p, int& ix, int& iy, int& iz) const {
		ix = int((p.x - m_domainMin.x) / m_cellSize);
		iy = int((p.y - m_domainMin.y) / m_cellSize);
		iz = int((p.z - m_domainMin.z) / m_cellSize);
	}

	// -------------------- Kernels (3D) --------------------
	inline float Kernel_Poly6(float r2) const 
	{
		if (r2 >= m_h2) return 0.0f;
		float t = (m_h2 - r2);
		return m_poly6Coeff * t * t * t;
	}
	inline XMVECTOR Kernel_Spiky_Grad(const XMVECTOR& rij) const 
	{
		float r = XMVectorGetX(XMVector3Length(rij));
		if (r <= 1e-7f || r >= m_h) return XMVECTOR{ 0, 0, 0 };
		float coef = -m_spikyCoeff * (m_h - r) * (m_h - r);
		XMVECTOR dir = XMVector3Normalize(rij);
		return dir * coef;
	}

	inline float Kernel_Visc_Laplacian(float r) const {
		if (r >= m_h) return 0.0f;
		return m_viscoLaplacianCoeff * (m_h - r);
	}

private:
	// Spatial Hash (TerrainSystem에서 GridDesc와 역할이 동일.
	XMFLOAT3 m_domainMin, m_domainMax;
	std::array<int,3> m_gridDim{ 0,0,0 };
	size_t m_numCells = 0;
	float m_cellSize = 0.04f; // set from h

	float m_h = 0.045f;
	float m_h2 = 0.045f * 0.045f;
	std::vector<std::vector<int>> m_buckets; // per-cell list of particle indices

	// Particles
	std::vector<Particle> m_particles;

	// Kernel Constants
	float m_poly6Coeff = 0.0f;
	float m_spikyCoeff = 0.0f;
	float m_viscoLaplacianCoeff = 0.0f;

	// Params
	FluidParams m_params;


};


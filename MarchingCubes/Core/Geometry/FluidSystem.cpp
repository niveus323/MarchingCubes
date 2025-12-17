#include "pch.h"
#include "FluidSystem.h"
#include <cmath>

FluidSystem::FluidSystem(const XMFLOAT3& min, const XMFLOAT3& max, const FluidParams& params) :
	m_domainMin(min),
	m_domainMax(max),
	m_params(params)
{
	SetSmoothingLength(m_params.smoothingLength);
	BuildGrid();
}

void FluidSystem::AddParticle(const XMFLOAT3& pos, const XMVECTOR& vel, float mass)
{
	Particle p;
	p.pos = pos;
	p.vel = vel;
	p.mass = (mass > 0.0f) ? mass : m_params.defaultMass;
	m_particles.push_back(p);
}

void FluidSystem::Step(float dt)
{
	if (dt <= 0.0f) return;
	if (dt > m_params.timeStepLimit) dt = m_params.timeStepLimit;

	BuildGrid();
	ClearGridBuckets();
	InsertParticlesToGrid();

	ComputePressure();
	ComputeForces();
	Integrate(dt);

	ApplyBoundaryConditions();

	// RasterizeToGrid()
}

void FluidSystem::SetSmoothingLength(float h)
{
	m_params.smoothingLength = h;
	m_h = h;
	m_h2 = h * h;
	const float pi = 3.14159265358979323846f;
	m_poly6Coeff = 315.0f / (64.0f * pi * std::powf(h, 9));
	m_spikyCoeff = 15.0f / (pi * std::powf(h, 6));
	m_viscoLaplacianCoeff = 45.0f / (pi * std::powf(h, 6));
	m_cellSize = h;
}

void FluidSystem::ClearParticles()
{
	m_particles.clear();
}

void FluidSystem::BuildGrid()
{
	m_cellSize = m_params.smoothingLength; // cell size = h
	for (int i = 0; i < 3; ++i) {
		float span = (i == 0 ? m_domainMax.x - m_domainMin.x : (i == 1 ? m_domainMax.y - m_domainMin.y : m_domainMax.z - m_domainMin.z));
		int n = std::max(1, int(std::ceil(span / m_cellSize)));
		m_gridDim[i] = n;
	}
	m_numCells = size_t(m_gridDim[0]) * size_t(m_gridDim[1]) * size_t(m_gridDim[2]);
	m_buckets.clear();
	m_buckets.resize(m_numCells);
}

void FluidSystem::ClearGridBuckets()
{
	for (auto& b : m_buckets) 
		b.clear();
}

void FluidSystem::InsertParticlesToGrid()
{
	for (size_t i = 0; i < m_particles.size(); ++i) {
		int ix, iy, iz;
		PositionToCell(m_particles[i].pos, ix, iy, iz);
		int idx = CellIndexFromCoord(ix, iy, iz);
		if (idx >= 0) m_buckets[idx].push_back(int(i));
		// else particle out of domain -> we'll clamp in boundary step
	}
}

void FluidSystem::ComputePressure()
{
	//논문 3.1.Pressure 참고
	std::vector<int> neighbors;
	for (size_t i = 0; i < m_particles.size(); ++i) 
	{
		Particle& pi = m_particles[i];
		XMVECTOR i_pos = XMLoadFloat3(&pi.pos);
		float density = 0.0f;
		GetNeighbors(int(i), neighbors);
		for (int j : neighbors) 
		{
			const Particle& pj = m_particles[j];

			// 논문 식(3)
			XMVECTOR j_pos = XMLoadFloat3(&pj.pos);
			XMVECTOR rij = i_pos - j_pos;
			XMVECTOR r_pow = XMVector3Dot(rij, rij);

			// 밀도 계산은 Poly6 커널 사용 <논문 식 (20)>
			density += pj.mass * Kernel_Poly6(XMVectorGetX(r_pow));
		}
		// avoid zero density
		pi.density = std::max(density, 1e-6f);
		// 논문 식(12)
		pi.pressure = m_params.stiffness * (pi.density - m_params.density_zero);
	}

}

void FluidSystem::ComputeForces()
{
	std::vector<int> neighbors;
	const float mu = m_params.viscocityFactor;
	const XMVECTOR gravityVec = XMLoadFloat3(&m_params.gravity);

	// reuse neighbors vector (allocated outside)
	for (size_t i = 0; i < m_particles.size(); ++i) {
		Particle& pi = m_particles[i];
		pi.force = XMVectorZero();

		GetNeighbors(int(i), neighbors);

		XMVECTOR fPressure = XMVectorZero();
		XMVECTOR fVisc = XMVectorZero();

		// for surface tension: accumulate color ci, normal ni, laplacian of color lapc
		float ci = 0.0f;
		XMVECTOR ni = XMVectorZero();
		float lapc = 0.0f;

		XMVECTOR pos_i = XMLoadFloat3(&pi.pos);

		// single neighbor loop: compute pressure, viscous, and surface-related accumulators
		for (int j : neighbors) {
			if (j == int(i)) continue;

			const Particle& pj = m_particles[j];

			// load neighbor pos/vel once
			XMVECTOR pos_j = XMLoadFloat3(&pj.pos);

			XMVECTOR rij = pos_i - pos_j;

			float rSq = XMVectorGetX(XMVector3LengthSq(rij));
			if (rSq >= m_h * m_h) continue;
			float r = std::sqrtf(rSq);

			// kernel evaluations (compute once)
			float w_poly6 = Kernel_Poly6(rSq);    
			XMVECTOR gradW = Kernel_Spiky_Grad(rij);
			float lapW = Kernel_Visc_Laplacian(r); 

			// 압력 (식 10)
			float pij = (pi.pressure + pj.pressure) / (2.0f * pj.density);
			XMVECTOR termPressure = XMVectorScale(gradW, -pj.mass * pij);
			fPressure = fPressure + termPressure;

			// 전달 응력 (식 14)
			XMVECTOR velDiff = pj.vel - pi.vel;
			XMVECTOR viscTerm = XMVectorScale(velDiff, mu * pj.mass * (lapW / pj.density));
			fVisc = fVisc + viscTerm;

			// 표면 장력 (식 15)
			if (m_params.enableSurfaceTension && m_params.surfaceTension > 0.0f) {
				ci += (pj.mass / pj.density) * w_poly6;
				// 표면 노말 축적 (식 16)
				ni += XMVectorScale(gradW, (pj.mass / pj.density));

				// laplacian of color uses same lapW (visc laplacian)
				lapc += (pj.mass / pj.density) * lapW;
			}
		} // end neighbor loop

		// external gravity force (as force-density: rho * g)
		XMVECTOR fExt = XMVectorScale(gravityVec, pi.density);

		// surface tension finalization (if enabled)
		XMVECTOR fSurf = XMVectorZero();
		if (m_params.enableSurfaceTension && m_params.surfaceTension > 0.0f) {
			float nlen = XMVectorGetX(XMVector3Length(ni));
			const float N_THRESHOLD = 1e-4f;
			const float EPS = 1e-6f;
			if (nlen > N_THRESHOLD) {
				float kappa = -lapc / (nlen + EPS);
				XMVECTOR n_hat = XMVectorScale(ni, 1.0f / (nlen + EPS));
				// fSurf = - sigma * kappa * n_hat
				fSurf = XMVectorScale(n_hat, (-m_params.surfaceTension * kappa));
			}
		}

		// accumulate all forces and store
		pi.force = fPressure + fVisc + fExt + fSurf;
	}
}
void FluidSystem::Integrate(float dt)
{
	for (auto& p : m_particles) {
		// acceleration = force / rho
		XMVECTOR acc = p.force / p.density;
		// semi-implicit Euler (symplectic)
		p.vel += acc * dt;
		XMVECTOR pos = XMLoadFloat3(&p.pos);
		pos += p.vel * dt;
		XMStoreFloat3(&p.pos, pos);
	}
}

// 지정한 Boundary를 유체가 넘어가지 않도록 AABB로 처리
void FluidSystem::ApplyBoundaryConditions()
{
	// simple AABB bounce with damping
	for (auto& p : m_particles) {
		// X
		if (p.pos.x < m_domainMin.x) {
			p.pos.x = m_domainMin.x;
			p.vel *= XMVECTOR{ -m_params.boundaryDamping, 1.0f, 1.0f };
		}
		else if (p.pos.x > m_domainMax.x) {
			p.pos.x = m_domainMax.x;
			p.vel *= XMVECTOR{ -m_params.boundaryDamping, 1.0f, 1.0f };
		}
		// Y
		if (p.pos.y < m_domainMin.y) {
			p.pos.y = m_domainMin.y;
			p.vel *= XMVECTOR{ 1.0f, -m_params.boundaryDamping, 1.0f };
		}
		else if (p.pos.y > m_domainMax.y) {
			p.pos.y = m_domainMax.y;
			p.vel *= XMVECTOR{ 1.0f, -m_params.boundaryDamping, 1.0f };
		}
		// Z
		if (p.pos.z < m_domainMin.z) {
			p.pos.z = m_domainMin.z;
			p.vel *= XMVECTOR{ 1.0f, 1.0f, -m_params.boundaryDamping };
		}
		else if (p.pos.z > m_domainMax.z) {
			p.pos.z = m_domainMax.z;
			p.vel *= XMVECTOR{ 1.0f, 1.0f, -m_params.boundaryDamping };
		}
	}
}

void FluidSystem::GetNeighbors(int pi, std::vector<int>& outNeighbors) const
{
	outNeighbors.clear();
	const XMFLOAT3& p = m_particles[pi].pos;
	int ic, jc, kc;
	PositionToCell(p, ic, jc, kc);
	for (int dz = -1; dz <= 1; ++dz) {
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				int ix = ic + dx, iy = jc + dy, iz = kc + dz;
				int idx = CellIndexFromCoord(ix, iy, iz);
				if (idx < 0) continue;
				const auto& bucket = m_buckets[idx];
				for (int j : bucket) outNeighbors.push_back(j);
			}
		}
	}
}

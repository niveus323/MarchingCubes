#pragma once

class ISubSystem
{
public:
	virtual ~ISubSystem() = default;

	virtual void Initialize() = 0;
	virtual void Update(float deltaTime) {}
	virtual void OnSceneLoad() {}
	virtual void OnSceneUnLoad() {}
	virtual void ExecuteCompute(uint32_t frameIndex) {}
};


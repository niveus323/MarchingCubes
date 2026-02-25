#pragma once
#include "Serializer.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class JsonSerializer : public Serializer
{
public:
	JsonSerializer(bool isSaving);

	void WriteToFile(const std::string& filepath);
	void LoadFromFile(const std::string& filepath);

	// Serializer을(를) 통해 상속됨
	void Serialize(const std::string& name, int& val) override;
	void Serialize(const std::string& name, float& val) override;
	void Serialize(const std::string& name, std::string& val) override;
	void Serialize(const std::string& name, bool& val) override;
	void Serialize(const std::string& name, uint32_t& val) override;
	void Serialize(const std::string& name, uint64_t& val) override;
	void BeginObject(const std::string& name) override;
	void EndObject() override;
	void BeginArray(const std::string& name, size_t& count) override;
	void EndArray() override;

private:
	json& GetCurrent();

	json m_root;
	std::vector<json*> m_currentNodes;
	std::vector<size_t> m_arrayIndices;
};


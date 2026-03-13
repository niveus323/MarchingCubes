#include "pch.h"
#include "JsonSerializer.h"
#include "Core/Utils/FileUtils.h"
#include <string>
#include <cstdint>

template<typename T>
void ProcessValue(bool isSaving, std::vector<json*>& nodes, std::vector<size_t>& indices, const std::string& name, T& val)
{
	json& current = *nodes.back();

	if (isSaving)
	{
		if (current.is_array())
		{
			current.push_back(val);
		}
		else
		{
			current[name] = val;
		}
	}
	else
	{
		if (current.is_array())
		{
			if (!indices.empty())
			{
				size_t& idx = indices.back();
				if (idx < current.size())
				{
					val = current[idx].get<T>();
					idx++;
				}
				else
				{
					Log::Print(ELogVerbosity::Fatal, "JsonSerializer", "Serialized Index Out of Range!!!!");
				}
			}
		}
		else
		{
			if (current.contains(name))
			{
				val = current[name].get<T>();
			}
		}
	}
}

#define IMPLEMENT_SERIALIZE(Type) \
    void JsonSerializer::Serialize(const std::string& name, Type& val) { \
        ProcessValue(IsSaving(), m_currentNodes, m_arrayIndices, name, val); \
    }

IMPLEMENT_SERIALIZE(int)
IMPLEMENT_SERIALIZE(float)
IMPLEMENT_SERIALIZE(std::string)
IMPLEMENT_SERIALIZE(bool)
IMPLEMENT_SERIALIZE(uint32_t)
IMPLEMENT_SERIALIZE(uint64_t)
#undef IMPLEMENT_SERIALIZE

JsonSerializer::JsonSerializer(bool isSaving) : Serializer(isSaving)
{
	m_currentNodes.push_back(&m_root);
}

void JsonSerializer::WriteToFile(const std::string& filepath)
{
	if (!FileUtils::WriteJSON(filepath, m_root, 4))
    {
        Log::Print(ELogVerbosity::Fatal, "JsonSerializer", "Failed to write file: {}", filepath);
    }
}

void JsonSerializer::LoadFromFile(const std::string& filepath)
{
	if (FileUtils::ReadJSON(filepath, m_root))
	{
		m_currentNodes.clear();
		m_currentNodes.push_back(&m_root);
		m_arrayIndices.clear();
	}
	else
	{
		Log::Print(ELogVerbosity::Fatal, "JsonSerializer", "Failed to load file: {}", filepath);
	}
}

void JsonSerializer::BeginObject(const std::string& name)
{
	json* selectedObj = nullptr;

	if (IsSaving()) 
	{
		if (GetCurrent().is_array())
		{
			// 배열 안에 객체 추가: 이름 무시하고 push_back
			json& newObj = GetCurrent().emplace_back(json::object());
			selectedObj = &newObj;
		}
		else
		{
			json& newObj = GetCurrent()[name];
			newObj = json::object();
			selectedObj = &newObj;
		}
	}
	else 
	{
		if (GetCurrent().is_array())
		{
			// m_arrayIndices를 사용하여 현재 순서의 요소를 가져옴
			if (!m_arrayIndices.empty())
			{
				size_t& idx = m_arrayIndices.back();
				if (idx < GetCurrent().size())
				{
					selectedObj = &GetCurrent()[idx];
				}
			}
		}
		else // 현재 노드가 객체인 경우 (일반적인 멤버 접근)
		{
			if (GetCurrent().contains(name))
			{
				selectedObj = &GetCurrent()[name];
			}
		}

		// 찾지 못했으면 더미 연결 (Crash 방지)
		if (!selectedObj)
		{
			static json dummy = json::object();
			selectedObj = &dummy;
		}
	}
	m_currentNodes.push_back(selectedObj);
}

void JsonSerializer::EndObject()
{
	if (!m_currentNodes.empty())
	{
		m_currentNodes.pop_back();

		if (!m_currentNodes.empty() && GetCurrent().is_array())
		{
			if (!m_arrayIndices.empty())
			{
				m_arrayIndices.back()++;
			}
		}
	}
}

void JsonSerializer::BeginArray(const std::string& name, size_t& count)
{
	json* selectedArray = nullptr;
	if (IsSaving())
	{
		if (GetCurrent().is_array())
		{
			json& newArr = GetCurrent().emplace_back(json::array());
			selectedArray = &newArr;
		}
		else
		{
			json& newArr = GetCurrent()[name];
			newArr = json::array();
			selectedArray = &newArr;
		}
	}
	else
	{
		if (GetCurrent().is_array())
		{
			if (!m_arrayIndices.empty())
			{
				size_t& idx = m_arrayIndices.back();
				if (idx < GetCurrent().size())
				{
					selectedArray = &GetCurrent()[idx];
					count = selectedArray->size();
				}
				else
				{
					static json dummy = json::array();
					selectedArray = &dummy;
					count = 0;
				}
			}
		}
		else
		{
			if (GetCurrent().contains(name))
			{
				selectedArray = &GetCurrent()[name];
				if (selectedArray->is_array())
				{
					count = selectedArray->size();
				}
				else
				{
					count = 0;
					static json dummy = json::array();
					selectedArray = &dummy;
				}
			}
			else
			{
				count = 0;
				static json dummy = json::array();
				selectedArray = &dummy;
			}
		}
	}

	m_currentNodes.push_back(selectedArray);
	m_arrayIndices.push_back(0);
}

void JsonSerializer::EndArray()
{
	if (!m_currentNodes.empty()) m_currentNodes.pop_back();

	if (!m_arrayIndices.empty()) m_arrayIndices.pop_back();

	if (!m_currentNodes.empty() && GetCurrent().is_array())
	{
		if (!m_arrayIndices.empty())
		{
			m_arrayIndices.back()++;
		}
	}
}

json& JsonSerializer::GetCurrent()
{
	return *m_currentNodes.back();
}

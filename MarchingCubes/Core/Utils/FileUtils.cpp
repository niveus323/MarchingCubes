#include "pch.h"
#include "FileUtils.h"
#include <windows.h>
#include <shobjidl.h>
#include <fstream>

std::string FileUtils::FileDialogs::OpenFile(const char* filterName, const char* filterSpec)
{
	std::string resultPath = "";
	ComPtr<IFileOpenDialog> pFileOpen;
	ThrowIfFailed(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pFileOpen)));
	std::wstring wName = StringUtils::ToWString(filterName);
	std::wstring wSpec = StringUtils::ToWString(filterSpec);
	COMDLG_FILTERSPEC fileTypes[] = {
		{ wName.c_str(), wSpec.c_str() },
		{ L"All Files", L"*.*" }
	};
	pFileOpen->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
	pFileOpen->SetDefaultExtension(L"json");

	HRESULT hr = pFileOpen->Show(NULL);
	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) // 취소를 눌렀을 경우 빈 값 반환
	{
		return "";
	}
	ThrowIfFailed(hr);

	ComPtr<IShellItem> pItem;
	ThrowIfFailed(pFileOpen->GetResult(&pItem));
	PWSTR pszFilePath;
	ThrowIfFailed(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));
	resultPath = StringUtils::ToString(pszFilePath);
	CoTaskMemFree(pszFilePath);

	return resultPath;
}

std::string FileUtils::FileDialogs::SaveFile(const char* filterName, const char* filterSpec)
{
	std::string resultPath = "";
	ComPtr<IFileSaveDialog> pFileSave;
	ThrowIfFailed(CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pFileSave)));
	std::wstring wName = StringUtils::ToWString(filterName);
	std::wstring wSpec = StringUtils::ToWString(filterSpec);
	COMDLG_FILTERSPEC fileTypes[] = {
		{ wName.c_str(), wSpec.c_str() },
		{ L"All Files", L"*.*" }
	};
	pFileSave->SetFileTypes(ARRAYSIZE(fileTypes), fileTypes);
	pFileSave->SetDefaultExtension(L"json");

	HRESULT hr = pFileSave->Show(NULL);
	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) // 취소를 눌렀을 경우 빈 값 반환
	{
		return "";
	}
	ThrowIfFailed(hr);

	ComPtr<IShellItem> pItem;
	ThrowIfFailed(pFileSave->GetResult(&pItem));
	PWSTR pszFilePath;
	ThrowIfFailed(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath));
	resultPath = StringUtils::ToString(pszFilePath);
	CoTaskMemFree(pszFilePath);

	return resultPath;
}

bool FileUtils::DuplicateFile(const std::filesystem::path& source, const std::filesystem::path& dest, bool overwrite)
{
	try
	{
		auto options = overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none;
		return std::filesystem::copy_file(source, dest, options);
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		Log::Print("FileUtils", "Failed to duplicate file from %s to %s: %s", source.string().c_str(), dest.string().c_str(), e.what());
		return false;
	}
}

bool FileUtils::WriteJSON(const std::filesystem::path& path, const nlohmann::json& data, int indent)
{
	std::ofstream out(path);
	if (!out.is_open())
	{
		Log::Print("FileUtils", "Failed to open file for writing: %s", path.string().c_str());
		return false;
	}

	try
	{
		out << data.dump(indent);
		out.close();
		return true;
	}
	catch (const std::exception& e)
	{
		Log::Print("FileUtils", "JSON Write Error: %s", e.what());
		return false;
	}
}

bool FileUtils::ReadJSON(const std::filesystem::path& path, nlohmann::json& outData)
{
	if (!std::filesystem::exists(path)) return false;

	std::ifstream in(path);
	if (!in.is_open()) return false;

	try
	{
		outData = nlohmann::json::parse(in);
		in.close();
		return true;
	}
	catch (const std::exception& e)
	{
		Log::Print("FileUtils", "JSON Parse Error in %s: %s", path.string().c_str(), e.what());
		return false;
	}
}

bool FileUtils::BinaryFile::WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& data)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.is_open())
	{
		Log::Print("FileUtils", "Failed to save file : %s", path.string().c_str());
		return false;
	}

	if (!data.empty())
	{
		out.write(reinterpret_cast<const char*>(data.data()), data.size());
	}

	out.close();
	return true;
}

bool FileUtils::BinaryFile::WriteFile(const std::filesystem::path& path, const std::vector<DataChunk>& chunks)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.is_open())
	{
		Log::Print("FileUtils", "Failed to save file : %s", path.string().c_str());
		return false;
	}

	for (const auto& chunk : chunks)
	{
		if (chunk.ptr != nullptr && chunk.size > 0)
		{
			out.write(reinterpret_cast<const char*>(chunk.ptr), chunk.size);
		}
	}

	out.close();
	return true;
}

bool FileUtils::BinaryFile::Readfile(const std::filesystem::path& path, std::vector<uint8_t>& outData)
{
	if (!std::filesystem::exists(path)) return false;

	std::ifstream in(path, std::ios::binary | std::ios::ate); // 파일 끝으로 이동해서 열기
	if (!in.is_open())
	{
		Log::Print("FileUtils", "Failed to open file : %s", path.string().c_str());
		return false;
	}

	// 파일 크기 확인 및 버퍼 할당
	std::streamsize size = in.tellg();
	in.seekg(0, std::ios::beg); // 다시 처음으로

	if (size <= 0) return true; // 빈 파일

	outData.resize(size);
	in.read(reinterpret_cast<char*>(outData.data()), size);

	return true;
}

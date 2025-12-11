#pragma once
#include <filesystem>
#include <DirectXTex.h>

namespace TexImporter
{
	enum class TextureSemantic : uint8_t
	{
		Unknown = 0,
		Albedo,
		Normal,
		Roughness,
		Metalness,
		AO,
		Height,
		Displacement,
		ARM,
		EnvHDR,
		Mask,
		Decal
	};

	struct TextureLoadProfile
	{
		TextureSemantic semantic = TextureSemantic::Unknown;

		DXGI_FORMAT targetFormat = DXGI_FORMAT_UNKNOWN;
		bool bSRGB = false;
		bool bMip = true;
	};

	static TextureSemantic DetectSemanticFromName(const std::filesystem::path& path);
	static TextureLoadProfile MakeProfile(const std::filesystem::path& path);

	static bool IsDDSPath(const std::filesystem::path& path);
	static bool IsBCFormat(DXGI_FORMAT fmt);
};

class TextureAsset
{
public:
	explicit TextureAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& cacheRoot = std::filesystem::path(L"Contents/Textures"));

	static TextureAsset FromFile(const std::filesystem::path& sourcePath, const std::filesystem::path& cacheRoot = std::filesystem::path(L"Contents/Textures"))
	{
		return TextureAsset(sourcePath, cacheRoot);
	}

	const std::filesystem::path& GetSourcePath() const { return m_sourcePath; };
	const DirectX::ScratchImage* GetImage() const { return m_image.get(); }
	DirectX::ScratchImage* GetImage() { return m_image.get(); }
	const DirectX::TexMetadata& GetMetadata() const { return m_image->GetMetadata(); }
	std::unique_ptr<DirectX::ScratchImage> ExtractImage() { return std::move(m_image); }

private:
	std::filesystem::path m_sourcePath;
	std::filesystem::path m_cacheRoot;
	std::unique_ptr<DirectX::ScratchImage> m_image;
};


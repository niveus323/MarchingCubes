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

/* [TextureAsset]
* - LifeTime : Asset Load -> Asset UnLoad
* - OwnerShip : ResourceManager
* - Access : ResourceManager::LoadTextureAsset
*/
class TextureAsset
{
public:
	TextureAsset(const std::filesystem::path& sourcePath);
	
	void ReleasePixelData();
	const std::filesystem::path& GetSourcePath() const { return m_sourcePath; };
	const DirectX::TexMetadata& GetMetadata() const { return m_metadata; }
	const DirectX::ScratchImage* GetImage() const { return m_image.get(); }

private:
	std::filesystem::path m_sourcePath;
	std::filesystem::path m_cacheRoot;
	DirectX::TexMetadata m_metadata{};
	std::unique_ptr<DirectX::ScratchImage> m_image;
};


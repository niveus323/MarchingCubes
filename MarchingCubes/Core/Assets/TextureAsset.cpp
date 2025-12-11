#include "pch.h"
#include "TextureAsset.h"
#include <algorithm>

// TODO : 런타임/에디터/툴 분리 시 TexImporter를 텍스쳐 임포터 툴로 옮기기
namespace TexImporter
{
    bool IsDDSPath(const std::filesystem::path& path)
    {
        std::wstring ext = ToLowerCopy(path.extension().wstring()); // ".dds"
        return (ext == L".dds");
    }

    bool IsWICPath(const std::filesystem::path& path)
    {
        std::wstring ext = ToLowerCopy(path.extension().wstring());
        return (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".tga");
    }

    bool IsBCFormat(DXGI_FORMAT fmt)
    {
        switch (fmt)
        {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
            case DXGI_FORMAT_BC4_UNORM:
            case DXGI_FORMAT_BC4_SNORM:
            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC5_SNORM:
            case DXGI_FORMAT_BC6H_UF16:
            case DXGI_FORMAT_BC6H_SF16:
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                return true;
            default:
                return false;
        }
    }

    TextureSemantic DetectSemanticFromName(const std::filesystem::path& path)
    {
        // 파일명(확장자 제거) 기준으로 의미 추론
        std::wstring stem = ToLowerCopy(path.stem().wstring()); // e.g. "rock_albedo"

        auto hasSuffix = [&](std::wstring_view suffix)
            {
                if (stem.size() < suffix.size()) return false;
                return std::equal(suffix.rbegin(), suffix.rend(), stem.rbegin());
            };

        if (hasSuffix(L"_albedo") || hasSuffix(L"_basecolor") || hasSuffix(L"_diffuse"))
            return TextureSemantic::Albedo;
        if (hasSuffix(L"_normal"))
            return TextureSemantic::Normal;
        if (hasSuffix(L"_rough"))
            return TextureSemantic::Roughness;
        if (hasSuffix(L"_metal"))
            return TextureSemantic::Metalness;
        if (hasSuffix(L"_ao"))
            return TextureSemantic::AO;
        if (hasSuffix(L"_height"))
            return TextureSemantic::Height;
        if (hasSuffix(L"_displace") || hasSuffix(L"_disp"))
            return TextureSemantic::Displacement;
        if (hasSuffix(L"_arm") || hasSuffix(L"_orm"))
            return TextureSemantic::ARM;
        if (hasSuffix(L"_env") || hasSuffix(L"_hdr"))
            return TextureSemantic::EnvHDR;
        if (hasSuffix(L"_mask"))
            return TextureSemantic::Mask;
        if (hasSuffix(L"_decal"))
            return TextureSemantic::Decal;

        return TextureSemantic::Unknown;
    }

    TextureLoadProfile MakeProfile(const std::filesystem::path& path)
    {
        TextureLoadProfile p;
        p.semantic = DetectSemanticFromName(path);

        using TS = TextureSemantic;

        switch (p.semantic)
        {
            case TS::Albedo:
                p.targetFormat = DXGI_FORMAT_BC7_UNORM_SRGB; // sRGB color
                p.bSRGB = true;
                p.bMip = true;
                break;
            case TS::Normal:
                p.targetFormat = DXGI_FORMAT_BC5_UNORM; // XY normal
                p.bSRGB = false;
                p.bMip = true;
                break;
            case TS::Roughness:
            case TS::Metalness:
            case TS::AO:
                p.targetFormat = DXGI_FORMAT_BC4_UNORM; // 단일 채널 기본
                p.bSRGB = false;
                p.bMip = true;
                break;
            case TS::ARM: // packed rough/metal/ao
                p.targetFormat = DXGI_FORMAT_BC7_UNORM; // 채널 패킹용
                p.bSRGB = false;
                p.bMip = true;
                break;
            case TS::Height:
            case TS::Displacement:
                p.targetFormat = DXGI_FORMAT_R16_UNORM;
                p.bSRGB = false;
                p.bMip = true;
                break;
            case TS::EnvHDR:
                p.targetFormat = DXGI_FORMAT_BC6H_UF16;
                p.bSRGB = false;
                p.bMip = true;
                break;
            case TS::Mask:
                p.targetFormat = DXGI_FORMAT_BC3_UNORM;
                p.bSRGB = false;
                p.bMip = true;
                break;
            case TS::Decal:
                p.targetFormat = DXGI_FORMAT_BC3_UNORM;
                p.bSRGB = true;
                p.bMip = true;
                break;
            case TS::Unknown:
            default:
                p.targetFormat = DXGI_FORMAT_UNKNOWN; // meta.format 그대로 사용
                p.bSRGB = true;
                p.bMip = true;
                break;
        }

        return p;
    }
}

TextureAsset::TextureAsset(const std::filesystem::path& sourcePath, const std::filesystem::path& cacheRoot) : 
    m_sourcePath(sourcePath),
    m_cacheRoot(cacheRoot)
{
    m_image = std::make_unique<ScratchImage>();
    auto fullPath = m_cacheRoot / m_sourcePath;

    if (TexImporter::IsDDSPath(fullPath))
    {
        // DDS 파일 로드
        ThrowIfFailed(DirectX::LoadFromDDSFile(fullPath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *m_image));
    }
    else if (TexImporter::IsWICPath(fullPath))
    {
        DirectX::ScratchImage tempImage;
        ThrowIfFailed(DirectX::LoadFromWICFile(fullPath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, tempImage));

        // 밉맵 생성
        DirectX::ScratchImage mipChain;
        HRESULT hr = DirectX::GenerateMipMaps(
            tempImage.GetImages(),
            tempImage.GetImageCount(),
            tempImage.GetMetadata(),
            DirectX::TEX_FILTER_DEFAULT,
            0,
            mipChain
        );

        if (SUCCEEDED(hr))
        {
            *m_image = std::move(mipChain);
        }
        else
        {
            // 밉맵 생성 실패 시(혹은 2D가 아닐 경우) 원본 그대로 사용
            *m_image = std::move(tempImage);
        }
    }
    else
    {
        // 확장자가 없거나 지원하지 않는 경우, 기존 로직처럼 강제로 .dds를 붙여서 시도하거나 에러 처리
        std::filesystem::path ddsPath = fullPath;
        ddsPath.replace_extension(L".dds");
        ThrowIfFailed(DirectX::LoadFromDDSFile(ddsPath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *m_image));
    }
}
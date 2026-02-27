#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wincodec.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <wrl/client.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
constexpr UINT kWindowWidth = 1280;
constexpr UINT kWindowHeight = 720;
constexpr UINT kShadowMapSize = 1024;
constexpr UINT kMsaaSampleCount = 8;
constexpr float kMoveSpeed = 3.0f;
constexpr float kLookSensitivity = 0.0025f;
constexpr float kMaxPitch = 1.4f;
constexpr wchar_t kFaceTexturePath[] = L"cage.jpg";
constexpr wchar_t kSkyboxDirectory0[] = L"Assets/Images/Skybox/Daylight";
constexpr wchar_t kSkyboxDirectory1[] = L"../Assets/Images/Skybox/Daylight";
constexpr wchar_t kSkyboxDirectory2[] = L"../../Assets/Images/Skybox/Daylight";
constexpr char kSponzaModelPath0[] = "Assets/Models/Sponza/sponza.obj";
constexpr char kSponzaModelPath1[] = "../Assets/Models/Sponza/sponza.obj";
constexpr char kSponzaModelPath2[] = "../../Assets/Models/Sponza/sponza.obj";

struct Vertex
{
    float position[3];
    float normal[3];
    float uv[2];
};

struct FrameBufferData
{
    XMFLOAT4X4 viewProjection;
    XMFLOAT4X4 lightViewProjection0;
    XMFLOAT4X4 lightViewProjection1;
    XMFLOAT4X4 lightViewProjection2;
    XMFLOAT4 cameraPosition;
    XMFLOAT4 lightPosition0;
    XMFLOAT4 lightPosition1;
    XMFLOAT4 lightDirection0;
    XMFLOAT4 lightDirection1;
    XMFLOAT4 lightColor0;
    XMFLOAT4 lightColor1;
    XMFLOAT4 spotlightParams0;
    XMFLOAT4 spotlightParams1;
    XMFLOAT4 directionalLightDirection;
    XMFLOAT4 directionalLightColor;
    XMFLOAT4 ambientColor;
    XMFLOAT4 lightingParams;
    XMFLOAT4 shadowParams;
};

struct ObjectBufferData
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 worldInverseTranspose;
    XMFLOAT4 materialColor;
    XMFLOAT4 materialParams;
};

struct ShadowFrameBufferData
{
    XMFLOAT4X4 lightViewProjection;
};

struct DebugBufferData
{
    XMFLOAT4 params;
};

struct SkyboxFrameBufferData
{
    XMFLOAT4X4 viewProjection;
};

enum class MeshType
{
    Cube,
    Plane,
    Model,
};

const char* MeshTypeToString(MeshType meshType)
{
    if (meshType == MeshType::Cube)
    {
        return "Cube";
    }
    if (meshType == MeshType::Model)
    {
        return "Model";
    }
    return "Plane";
}

struct TransformComponent
{
    XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
    XMFLOAT3 rotation = {0.0f, 0.0f, 0.0f};
    XMFLOAT3 scale = {1.0f, 1.0f, 1.0f};
};

struct RendererComponent
{
    XMFLOAT4 materialColor = {1.0f, 1.0f, 1.0f, 1.0f};
    bool useTexture = false;
    bool visible = true;
    bool castsShadow = true;
};

class GameObject
{
public:
    GameObject(std::uint32_t id, std::string name, MeshType meshType)
        : m_id(id), m_name(std::move(name)), m_meshType(meshType)
    {
    }

    std::uint32_t GetId() const
    {
        return m_id;
    }

    const std::string& GetName() const
    {
        return m_name;
    }

    MeshType GetMeshType() const
    {
        return m_meshType;
    }

    std::uint32_t GetParentId() const
    {
        return m_parentId;
    }

    void SetParentId(std::uint32_t parentId)
    {
        m_parentId = parentId;
    }

    const std::vector<std::uint32_t>& GetChildren() const
    {
        return m_children;
    }

    void AddChild(std::uint32_t childId)
    {
        m_children.push_back(childId);
    }

    const XMFLOAT3& GetPosition() const
    {
        return m_transform.position;
    }

    const XMFLOAT3& GetRotation() const
    {
        return m_transform.rotation;
    }

    const XMFLOAT3& GetScale() const
    {
        return m_transform.scale;
    }

    void SetPosition(const XMFLOAT3& position)
    {
        m_transform.position = position;
    }

    void SetRotation(const XMFLOAT3& rotation)
    {
        m_transform.rotation = rotation;
    }

    void SetScale(const XMFLOAT3& scale)
    {
        m_transform.scale = scale;
    }

    TransformComponent& GetTransform()
    {
        return m_transform;
    }

    const TransformComponent& GetTransform() const
    {
        return m_transform;
    }

    const XMFLOAT4& GetMaterialColor() const
    {
        return m_renderer.materialColor;
    }

    void SetMaterialColor(const XMFLOAT4& materialColor)
    {
        m_renderer.materialColor = materialColor;
    }

    bool UsesTexture() const
    {
        return m_renderer.useTexture;
    }

    void SetUsesTexture(bool useTexture)
    {
        m_renderer.useTexture = useTexture;
    }

    bool IsVisible() const
    {
        return m_renderer.visible;
    }

    void SetVisible(bool visible)
    {
        m_renderer.visible = visible;
    }

    bool CastsShadow() const
    {
        return m_renderer.castsShadow;
    }

    void SetCastsShadow(bool castsShadow)
    {
        m_renderer.castsShadow = castsShadow;
    }

    RendererComponent& GetRenderer()
    {
        return m_renderer;
    }

    const RendererComponent& GetRenderer() const
    {
        return m_renderer;
    }

    std::uint32_t GetModelId() const
    {
        return m_modelId;
    }

    void SetModelId(std::uint32_t modelId)
    {
        m_modelId = modelId;
    }

private:
    std::uint32_t m_id = 0;
    std::string m_name;
    MeshType m_meshType = MeshType::Cube;
    std::uint32_t m_parentId = 0;
    std::vector<std::uint32_t> m_children;
    TransformComponent m_transform;
    RendererComponent m_renderer;
    std::uint32_t m_modelId = 0;
};

class SceneGraph
{
public:
    GameObject& CreateGameObject(const std::string& name, MeshType meshType, std::uint32_t parentId = 0)
    {
        const std::uint32_t id = m_nextId++;
        m_objects.emplace_back(id, name, meshType);
        m_indexById[id] = m_objects.size() - 1;

        GameObject& object = m_objects.back();
        if (parentId != 0)
        {
            GameObject* parent = GetById(parentId);
            if (parent != nullptr)
            {
                object.SetParentId(parentId);
                parent->AddChild(id);
            }
            else
            {
                m_rootIds.push_back(id);
            }
        }
        else
        {
            m_rootIds.push_back(id);
        }

        return object;
    }

    GameObject* GetById(std::uint32_t id)
    {
        const auto it = m_indexById.find(id);
        if (it == m_indexById.end())
        {
            return nullptr;
        }
        return &m_objects[it->second];
    }

    const GameObject* GetById(std::uint32_t id) const
    {
        const auto it = m_indexById.find(id);
        if (it == m_indexById.end())
        {
            return nullptr;
        }
        return &m_objects[it->second];
    }

    const std::vector<std::uint32_t>& GetRootIds() const
    {
        return m_rootIds;
    }

    const std::vector<GameObject>& GetObjects() const
    {
        return m_objects;
    }

private:
    std::uint32_t m_nextId = 1;
    std::vector<GameObject> m_objects;
    std::unordered_map<std::uint32_t, std::size_t> m_indexById;
    std::vector<std::uint32_t> m_rootIds;
};

struct MeshRenderData
{
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    UINT indexCount = 0;
    UINT stride = 0;
    UINT offset = 0;
};

struct RenderItem
{
    const GameObject* gameObject = nullptr;
    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX worldInverseTranspose = XMMatrixIdentity();
};

struct ModelMesh
{
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    UINT indexCount = 0;
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ComPtr<ID3D11ShaderResourceView> texture;
    bool hasTexture = false;
    XMFLOAT4 materialColor = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct ModelResource
{
    std::string sourcePath;
    std::vector<ModelMesh> meshes;
};

bool LoadTextureFromImageFile(
    ID3D11Device* device,
    const std::string& filePath,
    ComPtr<ID3D11ShaderResourceView>& shaderResourceView)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr || width <= 0 || height <= 0)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = static_cast<UINT>(width);
    textureDesc.Height = static_cast<UINT>(height);
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = pixels;
    initialData.SysMemPitch = static_cast<UINT>(width * 4);

    ComPtr<ID3D11Texture2D> texture;
    const HRESULT result = device->CreateTexture2D(&textureDesc, &initialData, &texture);
    stbi_image_free(pixels);
    if (FAILED(result))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    return SUCCEEDED(device->CreateShaderResourceView(texture.Get(), &srvDesc, &shaderResourceView));
}

std::string ResolveSponzaPath()
{
    const std::array<std::string, 3> candidates = {
        kSponzaModelPath0,
        kSponzaModelPath1,
        kSponzaModelPath2};

    for (const std::string& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }

    return {};
}

bool LoadModelFromFile(
    ID3D11Device* device,
    const std::string& modelPath,
    ModelResource& outModel)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        modelPath,
        aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality |
            aiProcess_SortByPType);

    if (scene == nullptr || scene->HasMeshes() == false)
    {
        return false;
    }

    const std::filesystem::path modelDir = std::filesystem::path(modelPath).parent_path();
    std::unordered_map<std::string, ComPtr<ID3D11ShaderResourceView>> textureCache;

    outModel.sourcePath = modelPath;
    outModel.meshes.clear();
    outModel.meshes.reserve(scene->mNumMeshes);

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* sourceMesh = scene->mMeshes[meshIndex];
        if (sourceMesh == nullptr || sourceMesh->mNumVertices == 0 || sourceMesh->mNumFaces == 0)
        {
            continue;
        }

        std::vector<Vertex> vertices;
        vertices.resize(sourceMesh->mNumVertices);
        for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex)
        {
            const aiVector3D& p = sourceMesh->mVertices[vertexIndex];
            const aiVector3D n = sourceMesh->HasNormals() ? sourceMesh->mNormals[vertexIndex] : aiVector3D(0.0f, 1.0f, 0.0f);
            const aiVector3D uv = sourceMesh->HasTextureCoords(0) ? sourceMesh->mTextureCoords[0][vertexIndex] : aiVector3D(0.0f, 0.0f, 0.0f);
            vertices[vertexIndex] = {{p.x, p.y, p.z}, {n.x, n.y, n.z}, {uv.x, uv.y}};
        }

        std::vector<std::uint32_t> indices;
        indices.reserve(sourceMesh->mNumFaces * 3);
        for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex)
        {
            const aiFace& face = sourceMesh->mFaces[faceIndex];
            if (face.mNumIndices != 3)
            {
                continue;
            }
            indices.push_back(face.mIndices[0]);
            indices.push_back(face.mIndices[1]);
            indices.push_back(face.mIndices[2]);
        }

        if (indices.empty())
        {
            continue;
        }

        ModelMesh mesh{};
        mesh.indexCount = static_cast<UINT>(indices.size());

        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = vertices.data();

        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &mesh.vertexBuffer)))
        {
            return false;
        }

        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = indices.data();

        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &mesh.indexBuffer)))
        {
            return false;
        }

        if (sourceMesh->mMaterialIndex < scene->mNumMaterials)
        {
            aiMaterial* material = scene->mMaterials[sourceMesh->mMaterialIndex];
            if (material != nullptr)
            {
                aiColor3D diffuse(1.0f, 1.0f, 1.0f);
                material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
                mesh.materialColor = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f);

                aiString texturePath;
                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
                {
                    const std::filesystem::path fullPath = modelDir / texturePath.C_Str();
                    const std::string textureFile = fullPath.string();
                    const auto textureIt = textureCache.find(textureFile);
                    if (textureIt != textureCache.end())
                    {
                        mesh.texture = textureIt->second;
                        mesh.hasTexture = true;
                    }
                    else
                    {
                        ComPtr<ID3D11ShaderResourceView> textureSrv;
                        if (LoadTextureFromImageFile(device, textureFile, textureSrv))
                        {
                            textureCache[textureFile] = textureSrv;
                            mesh.texture = textureSrv;
                            mesh.hasTexture = true;
                        }
                    }
                }
            }
        }

        outModel.meshes.push_back(std::move(mesh));
    }

    return !outModel.meshes.empty();
}

XMMATRIX ComposeTransform(const GameObject& gameObject)
{
    const XMFLOAT3& scale = gameObject.GetScale();
    const XMFLOAT3& rotation = gameObject.GetRotation();
    const XMFLOAT3& position = gameObject.GetPosition();

    return XMMatrixScaling(scale.x, scale.y, scale.z) *
           XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) *
           XMMatrixTranslation(position.x, position.y, position.z);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
    {
        return 1;
    }

    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(window, message, wParam, lParam);
}

bool LoadTextureFromFile(
    ID3D11Device* device,
    const wchar_t* filePath,
    ComPtr<ID3D11ShaderResourceView>& shaderResourceView)
{
    ComPtr<IWICImagingFactory> imagingFactory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&imagingFactory));
    if (FAILED(result))
    {
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    result = imagingFactory->CreateDecoderFromFilename(
        filePath,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(result))
    {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, &frame);
    if (FAILED(result))
    {
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    result = imagingFactory->CreateFormatConverter(&converter);
    if (FAILED(result))
    {
        return false;
    }

    result = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(result))
    {
        return false;
    }

    UINT textureWidth = 0;
    UINT textureHeight = 0;
    result = converter->GetSize(&textureWidth, &textureHeight);
    if (FAILED(result) || textureWidth == 0 || textureHeight == 0)
    {
        return false;
    }

    std::vector<std::uint8_t> pixelData(textureWidth * textureHeight * 4);
    result = converter->CopyPixels(
        nullptr,
        textureWidth * 4,
        static_cast<UINT>(pixelData.size()),
        pixelData.data());
    if (FAILED(result))
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = pixelData.data();
    initialData.SysMemPitch = textureWidth * 4;

    ComPtr<ID3D11Texture2D> texture;
    result = device->CreateTexture2D(&textureDesc, &initialData, &texture);
    if (FAILED(result))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    result = device->CreateShaderResourceView(texture.Get(), &srvDesc, &shaderResourceView);
    return SUCCEEDED(result);
}

bool ResolveSkyboxFacePaths(std::array<std::wstring, 6>& outFacePaths)
{
    const std::array<std::wstring, 3> basePathCandidates = {
        kSkyboxDirectory0,
        kSkyboxDirectory1,
        kSkyboxDirectory2};

    const std::array<std::wstring, 6> fileNames = {
        L"Daylight Box_Right.bmp",
        L"Daylight Box_Left.bmp",
        L"Daylight Box_Top.bmp",
        L"Daylight Box_Bottom.bmp",
        L"Daylight Box_Front.bmp",
        L"Daylight Box_Back.bmp"};

    for (const std::wstring& basePath : basePathCandidates)
    {
        bool allFound = true;
        std::array<std::wstring, 6> candidatePaths{};
        for (size_t faceIndex = 0; faceIndex < fileNames.size(); ++faceIndex)
        {
            std::filesystem::path fullPath = std::filesystem::path(basePath) / fileNames[faceIndex];
            if (!std::filesystem::exists(fullPath))
            {
                allFound = false;
                break;
            }
            candidatePaths[faceIndex] = fullPath.wstring();
        }

        if (allFound)
        {
            outFacePaths = candidatePaths;
            return true;
        }
    }

    return false;
}

bool LoadCubemapFromFiles(
    ID3D11Device* device,
    const std::array<std::wstring, 6>& facePaths,
    ComPtr<ID3D11ShaderResourceView>& shaderResourceView)
{
    ComPtr<IWICImagingFactory> imagingFactory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&imagingFactory));
    if (FAILED(result))
    {
        return false;
    }

    UINT faceWidth = 0;
    UINT faceHeight = 0;
    std::array<std::vector<std::uint8_t>, 6> facePixelData;

    for (size_t faceIndex = 0; faceIndex < facePaths.size(); ++faceIndex)
    {
        ComPtr<IWICBitmapDecoder> decoder;
        result = imagingFactory->CreateDecoderFromFilename(
            facePaths[faceIndex].c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnDemand,
            &decoder);
        if (FAILED(result))
        {
            return false;
        }

        ComPtr<IWICBitmapFrameDecode> frame;
        result = decoder->GetFrame(0, &frame);
        if (FAILED(result))
        {
            return false;
        }

        ComPtr<IWICFormatConverter> converter;
        result = imagingFactory->CreateFormatConverter(&converter);
        if (FAILED(result))
        {
            return false;
        }

        result = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(result))
        {
            return false;
        }

        UINT width = 0;
        UINT height = 0;
        result = converter->GetSize(&width, &height);
        if (FAILED(result) || width == 0 || height == 0)
        {
            return false;
        }

        if (faceIndex == 0)
        {
            faceWidth = width;
            faceHeight = height;
        }
        else if (width != faceWidth || height != faceHeight)
        {
            return false;
        }

        std::vector<std::uint8_t> pixelData(faceWidth * faceHeight * 4);
        result = converter->CopyPixels(
            nullptr,
            faceWidth * 4,
            static_cast<UINT>(pixelData.size()),
            pixelData.data());
        if (FAILED(result))
        {
            return false;
        }

        facePixelData[faceIndex] = std::move(pixelData);
    }

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = faceWidth;
    textureDesc.Height = faceHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 6;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    std::array<D3D11_SUBRESOURCE_DATA, 6> subresources{};
    for (size_t faceIndex = 0; faceIndex < facePixelData.size(); ++faceIndex)
    {
        subresources[faceIndex].pSysMem = facePixelData[faceIndex].data();
        subresources[faceIndex].SysMemPitch = faceWidth * 4;
        subresources[faceIndex].SysMemSlicePitch = 0;
    }

    ComPtr<ID3D11Texture2D> cubeTexture;
    result = device->CreateTexture2D(&textureDesc, subresources.data(), &cubeTexture);
    if (FAILED(result))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;

    result = device->CreateShaderResourceView(cubeTexture.Get(), &srvDesc, &shaderResourceView);
    return SUCCEEDED(result);
}
}

int Run(HINSTANCE instance, int showCommand);

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    return Run(instance, showCommand);
}

int main()
{
    return Run(GetModuleHandleA(nullptr), SW_SHOWDEFAULT);
}

int Run(HINSTANCE instance, int showCommand)
{
    HRESULT coInitResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(coInitResult) && coInitResult != RPC_E_CHANGED_MODE)
    {
        return -1;
    }
    const bool shouldUninitializeCom = SUCCEEDED(coInitResult);

    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "Dx11MinimalWindow";

    if (!RegisterClassExA(&windowClass))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    RECT windowRect{0, 0, static_cast<LONG>(kWindowWidth), static_cast<LONG>(kWindowHeight)};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND window = CreateWindowExA(
        0,
        windowClass.lpszClassName,
        "DirectX 11 Cube + Plane + Phong + Shadow Mapping",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window)
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ShowWindow(window, showCommand);
    RECT initialClientRect{};
    GetClientRect(window, &initialClientRect);
    UINT sceneRenderWidth = static_cast<UINT>((std::max)(1L, initialClientRect.right - initialClientRect.left));
    UINT sceneRenderHeight = static_cast<UINT>((std::max)(1L, initialClientRect.bottom - initialClientRect.top));

    POINT mouseCenterClient{
        static_cast<LONG>(sceneRenderWidth / 2),
        static_cast<LONG>(sceneRenderHeight / 2)};
    POINT mouseCenterScreen = mouseCenterClient;
    ClientToScreen(window, &mouseCenterScreen);
    SetCursorPos(mouseCenterScreen.x, mouseCenterScreen.y);

    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferDesc.Width = sceneRenderWidth;
    swapChainDesc.BufferDesc.Height = sceneRenderHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.OutputWindow = window;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT deviceFlags = 0;
#if defined(_DEBUG)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        deviceFlags,
        featureLevels,
        static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &swapChain,
        &device,
        &createdFeatureLevel,
        &context);

    if (result == E_INVALIDARG)
    {
        D3D_FEATURE_LEVEL fallbackFeatureLevels[] = {D3D_FEATURE_LEVEL_11_0};
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            deviceFlags,
            fallbackFeatureLevels,
            static_cast<UINT>(sizeof(fallbackFeatureLevels) / sizeof(fallbackFeatureLevels[0])),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &swapChain,
            &device,
            &createdFeatureLevel,
            &context);
    }

    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING && (deviceFlags & D3D11_CREATE_DEVICE_DEBUG) != 0)
    {
        deviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            deviceFlags,
            featureLevels,
            static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &swapChain,
            &device,
            &createdFeatureLevel,
            &context);

        if (result == E_INVALIDARG)
        {
            D3D_FEATURE_LEVEL fallbackFeatureLevels[] = {D3D_FEATURE_LEVEL_11_0};
            result = D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                deviceFlags,
                fallbackFeatureLevels,
                static_cast<UINT>(sizeof(fallbackFeatureLevels) / sizeof(fallbackFeatureLevels[0])),
                D3D11_SDK_VERSION,
                &swapChainDesc,
                &swapChain,
                &device,
                &createdFeatureLevel,
                &context);
        }
    }

    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    UINT msaaColorQualityLevels = 0;
    UINT msaaDepthQualityLevels = 0;
    device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, kMsaaSampleCount, &msaaColorQualityLevels);
    device->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT, kMsaaSampleCount, &msaaDepthQualityLevels);

    const bool msaaSupported = (msaaColorQualityLevels > 0) && (msaaDepthQualityLevels > 0);
    const UINT sceneSampleCount = msaaSupported ? kMsaaSampleCount : 1;
    const UINT sceneSampleQuality = msaaSupported ? ((std::min)(msaaColorQualityLevels, msaaDepthQualityLevels) - 1) : 0;

    ComPtr<ID3D11Texture2D> backBuffer;
    ComPtr<ID3D11RenderTargetView> renderTargetView;
    ComPtr<ID3D11Texture2D> gameViewColorBuffer;
    ComPtr<ID3D11RenderTargetView> gameViewRenderTargetView;
    ComPtr<ID3D11ShaderResourceView> gameViewShaderResourceView;
    ComPtr<ID3D11Texture2D> gameViewMsaaColorBuffer;
    ComPtr<ID3D11RenderTargetView> gameViewMsaaRenderTargetView;
    ComPtr<ID3D11Texture2D> depthBuffer;
    ComPtr<ID3D11Texture2D> msaaDepthBuffer;
    ComPtr<ID3D11DepthStencilView> msaaDepthStencilView;
    ComPtr<ID3D11DepthStencilView> depthStencilView;

    auto createSizeDependentResources = [&](UINT width, UINT height) -> bool
    {
        HRESULT createResult = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(createResult))
        {
            return false;
        }

        createResult = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView);
        if (FAILED(createResult))
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC gameViewDesc{};
        gameViewDesc.Width = width;
        gameViewDesc.Height = height;
        gameViewDesc.MipLevels = 1;
        gameViewDesc.ArraySize = 1;
        gameViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        gameViewDesc.SampleDesc.Count = 1;
        gameViewDesc.SampleDesc.Quality = 0;
        gameViewDesc.Usage = D3D11_USAGE_DEFAULT;
        gameViewDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        createResult = device->CreateTexture2D(&gameViewDesc, nullptr, &gameViewColorBuffer);
        if (FAILED(createResult))
        {
            return false;
        }

        createResult = device->CreateRenderTargetView(gameViewColorBuffer.Get(), nullptr, &gameViewRenderTargetView);
        if (FAILED(createResult))
        {
            return false;
        }

        createResult = device->CreateShaderResourceView(gameViewColorBuffer.Get(), nullptr, &gameViewShaderResourceView);
        if (FAILED(createResult))
        {
            return false;
        }

        if (msaaSupported)
        {
            D3D11_TEXTURE2D_DESC sceneColorDesc = gameViewDesc;
            sceneColorDesc.SampleDesc.Count = sceneSampleCount;
            sceneColorDesc.SampleDesc.Quality = sceneSampleQuality;
            sceneColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

            createResult = device->CreateTexture2D(&sceneColorDesc, nullptr, &gameViewMsaaColorBuffer);
            if (FAILED(createResult))
            {
                return false;
            }

            createResult = device->CreateRenderTargetView(gameViewMsaaColorBuffer.Get(), nullptr, &gameViewMsaaRenderTargetView);
            if (FAILED(createResult))
            {
                return false;
            }
        }

        D3D11_TEXTURE2D_DESC depthBufferDesc{};
        depthBufferDesc.Width = width;
        depthBufferDesc.Height = height;
        depthBufferDesc.MipLevels = 1;
        depthBufferDesc.ArraySize = 1;
        depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthBufferDesc.SampleDesc.Count = 1;
        depthBufferDesc.SampleDesc.Quality = 0;
        depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        createResult = device->CreateTexture2D(&depthBufferDesc, nullptr, &depthBuffer);
        if (FAILED(createResult))
        {
            return false;
        }

        if (msaaSupported)
        {
            depthBufferDesc.SampleDesc.Count = sceneSampleCount;
            depthBufferDesc.SampleDesc.Quality = sceneSampleQuality;

            createResult = device->CreateTexture2D(&depthBufferDesc, nullptr, &msaaDepthBuffer);
            if (FAILED(createResult))
            {
                return false;
            }

            createResult = device->CreateDepthStencilView(msaaDepthBuffer.Get(), nullptr, &msaaDepthStencilView);
            if (FAILED(createResult))
            {
                return false;
            }
        }

        createResult = device->CreateDepthStencilView(depthBuffer.Get(), nullptr, &depthStencilView);
        return SUCCEEDED(createResult);
    };

    if (!createSizeDependentResources(sceneRenderWidth, sceneRenderHeight))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    std::array<ComPtr<ID3D11Texture2D>, 3> shadowTextures;
    std::array<ComPtr<ID3D11DepthStencilView>, 3> shadowDepthViews;
    std::array<ComPtr<ID3D11ShaderResourceView>, 3> shadowShaderViews;

    for (size_t lightIndex = 0; lightIndex < 3; ++lightIndex)
    {
        D3D11_TEXTURE2D_DESC shadowTextureDesc{};
        shadowTextureDesc.Width = kShadowMapSize;
        shadowTextureDesc.Height = kShadowMapSize;
        shadowTextureDesc.MipLevels = 1;
        shadowTextureDesc.ArraySize = 1;
        shadowTextureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        shadowTextureDesc.SampleDesc.Count = 1;
        shadowTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        shadowTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        result = device->CreateTexture2D(&shadowTextureDesc, nullptr, &shadowTextures[lightIndex]);
        if (FAILED(result))
        {
            if (shouldUninitializeCom)
            {
                CoUninitialize();
            }
            return -1;
        }

        D3D11_DEPTH_STENCIL_VIEW_DESC shadowDsvDesc{};
        shadowDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        shadowDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

        result = device->CreateDepthStencilView(shadowTextures[lightIndex].Get(), &shadowDsvDesc, &shadowDepthViews[lightIndex]);
        if (FAILED(result))
        {
            if (shouldUninitializeCom)
            {
                CoUninitialize();
            }
            return -1;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC shadowSrvDesc{};
        shadowSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        shadowSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shadowSrvDesc.Texture2D.MostDetailedMip = 0;
        shadowSrvDesc.Texture2D.MipLevels = 1;

        result = device->CreateShaderResourceView(shadowTextures[lightIndex].Get(), &shadowSrvDesc, &shadowShaderViews[lightIndex]);
        if (FAILED(result))
        {
            if (shouldUninitializeCom)
            {
                CoUninitialize();
            }
            return -1;
        }
    }

    const char sceneVertexShaderSource[] = R"(
        cbuffer FrameBuffer : register(b0)
        {
            float4x4 viewProjection;
            float4x4 lightViewProjection0;
            float4x4 lightViewProjection1;
            float4x4 lightViewProjection2;
            float4 cameraPosition;
            float4 lightPosition0;
            float4 lightPosition1;
            float4 lightDirection0;
            float4 lightDirection1;
            float4 lightColor0;
            float4 lightColor1;
            float4 spotlightParams0;
            float4 spotlightParams1;
            float4 directionalLightDirection;
            float4 directionalLightColor;
            float4 ambientColor;
            float4 lightingParams;
            float4 shadowParams;
        };

        cbuffer ObjectBuffer : register(b1)
        {
            float4x4 world;
            float4x4 worldInverseTranspose;
            float4 materialColor;
            float4 materialParams;
        };

        struct VSInput
        {
            float3 position : POSITION;
            float3 normal : NORMAL;
            float2 uv : TEXCOORD0;
        };

        struct VSOutput
        {
            float4 position : SV_POSITION;
            float3 worldPosition : TEXCOORD0;
            float3 worldNormal : TEXCOORD1;
            float2 uv : TEXCOORD2;
            float4 shadowPosition0 : TEXCOORD3;
            float4 shadowPosition1 : TEXCOORD4;
            float4 shadowPosition2 : TEXCOORD5;
        };

        VSOutput main(VSInput input)
        {
            VSOutput output;
            float4 worldPosition = mul(float4(input.position, 1.0f), world);
            output.position = mul(worldPosition, viewProjection);
            output.worldPosition = worldPosition.xyz;
            output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldInverseTranspose).xyz);
            output.uv = input.uv;
            output.shadowPosition0 = mul(worldPosition, lightViewProjection0);
            output.shadowPosition1 = mul(worldPosition, lightViewProjection1);
            output.shadowPosition2 = mul(worldPosition, lightViewProjection2);
            return output;
        }
    )";

    const char scenePixelShaderSource[] = R"(
        Texture2D baseTexture : register(t0);
        Texture2D shadowMap0 : register(t1);
        Texture2D shadowMap1 : register(t2);
        Texture2D shadowMap2 : register(t3);

        SamplerState textureSampler : register(s0);
        SamplerComparisonState shadowSampler : register(s1);
        SamplerState shadowDepthSampler : register(s2);

        cbuffer FrameBuffer : register(b0)
        {
            float4x4 viewProjection;
            float4x4 lightViewProjection0;
            float4x4 lightViewProjection1;
            float4x4 lightViewProjection2;
            float4 cameraPosition;
            float4 lightPosition0;
            float4 lightPosition1;
            float4 lightDirection0;
            float4 lightDirection1;
            float4 lightColor0;
            float4 lightColor1;
            float4 spotlightParams0;
            float4 spotlightParams1;
            float4 directionalLightDirection;
            float4 directionalLightColor;
            float4 ambientColor;
            float4 lightingParams;
            float4 shadowParams;
        };

        cbuffer ObjectBuffer : register(b1)
        {
            float4x4 world;
            float4x4 worldInverseTranspose;
            float4 materialColor;
            float4 materialParams;
        };

        struct PSInput
        {
            float4 position : SV_POSITION;
            float3 worldPosition : TEXCOORD0;
            float3 worldNormal : TEXCOORD1;
            float2 uv : TEXCOORD2;
            float4 shadowPosition0 : TEXCOORD3;
            float4 shadowPosition1 : TEXCOORD4;
            float4 shadowPosition2 : TEXCOORD5;
        };

        float ComputeShadow(Texture2D shadowMap, float4 shadowPosition, float shadowBias)
        {
            float3 projected = shadowPosition.xyz / shadowPosition.w;
            float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);

            if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || projected.z < 0.0f || projected.z > 1.0f)
            {
                return 1.0f;
            }

            return shadowMap.SampleCmpLevelZero(shadowSampler, uv, projected.z - shadowBias);
        }

        static const int kPcssSampleCount = 12;
        static const float2 kPoissonDisk[kPcssSampleCount] = {
            float2(-0.326f, -0.406f), float2(-0.840f, -0.074f), float2(-0.696f, 0.457f), float2(-0.203f, 0.621f),
            float2(0.962f, -0.195f), float2(0.473f, -0.480f), float2(0.519f, 0.767f), float2(0.185f, -0.893f),
            float2(0.507f, 0.064f), float2(0.896f, 0.412f), float2(-0.322f, -0.933f), float2(-0.792f, -0.598f)};

        bool ProjectShadow(float4 shadowPosition, out float2 uv, out float receiverDepth)
        {
            float3 projected = shadowPosition.xyz / shadowPosition.w;
            uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);
            receiverDepth = projected.z;

            return !(uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || receiverDepth < 0.0f || receiverDepth > 1.0f);
        }

        float ComputeShadowPCSS(Texture2D shadowMap, float4 shadowPosition, float shadowBias)
        {
            float2 uv = 0.0f.xx;
            float receiverDepth = 0.0f;
            if (!ProjectShadow(shadowPosition, uv, receiverDepth))
            {
                return 1.0f;
            }

            float blockerSum = 0.0f;
            float blockerCount = 0.0f;
            const float blockerSearchRadius = shadowParams.y;

            [unroll]
            for (int sampleIndex = 0; sampleIndex < kPcssSampleCount; ++sampleIndex)
            {
                float2 sampleUv = uv + kPoissonDisk[sampleIndex] * blockerSearchRadius;
                float blockerDepth = shadowMap.SampleLevel(shadowDepthSampler, sampleUv, 0).r;
                if (blockerDepth < receiverDepth - shadowBias)
                {
                    blockerSum += blockerDepth;
                    blockerCount += 1.0f;
                }
            }

            if (blockerCount < 1.0f)
            {
                return 1.0f;
            }

            float avgBlockerDepth = blockerSum / blockerCount;
            float penumbraRatio = saturate((receiverDepth - avgBlockerDepth) / max(avgBlockerDepth, 0.0001f));
            float filterRadius = clamp(penumbraRatio * shadowParams.y * 4.0f, shadowParams.x, shadowParams.w);

            float visibility = 0.0f;
            [unroll]
            for (int sampleIndex = 0; sampleIndex < kPcssSampleCount; ++sampleIndex)
            {
                float2 sampleUv = uv + kPoissonDisk[sampleIndex] * filterRadius;
                visibility += shadowMap.SampleCmpLevelZero(shadowSampler, sampleUv, receiverDepth - shadowBias);
            }

            return visibility / kPcssSampleCount;
        }

        float ComputeSpotFactor(float3 spotlightDirection, float3 toLightDirection, float innerCosAngle, float outerCosAngle)
        {
            float coneCos = dot(normalize(spotlightDirection), -normalize(toLightDirection));
            float coneRange = max(innerCosAngle - outerCosAngle, 0.0001f);
            return saturate((coneCos - outerCosAngle) / coneRange);
        }

        float4 main(PSInput input) : SV_TARGET
        {
            float3 normal = normalize(input.worldNormal);
            float3 viewDirection = normalize(cameraPosition.xyz - input.worldPosition);

            float useTexture = materialParams.x;
            float3 sampledAlbedo = baseTexture.Sample(textureSampler, input.uv).rgb;
            float3 albedo = lerp(materialColor.rgb, sampledAlbedo, useTexture);

            float specularPower = lightingParams.x;
            float shadowBias = lightingParams.y;
            float pcssEnabled = shadowParams.z;

            float shadow0 = (pcssEnabled > 0.5f) ? ComputeShadowPCSS(shadowMap0, input.shadowPosition0, shadowBias) : ComputeShadow(shadowMap0, input.shadowPosition0, shadowBias);
            float shadow1 = (pcssEnabled > 0.5f) ? ComputeShadowPCSS(shadowMap1, input.shadowPosition1, shadowBias) : ComputeShadow(shadowMap1, input.shadowPosition1, shadowBias);
            float shadow2 = (pcssEnabled > 0.5f) ? ComputeShadowPCSS(shadowMap2, input.shadowPosition2, shadowBias) : ComputeShadow(shadowMap2, input.shadowPosition2, shadowBias);

            float3 totalLight = ambientColor.rgb * ambientColor.a;

            float3 toLight0 = lightPosition0.xyz - input.worldPosition;
            float distance0 = length(toLight0);
            float3 lightDir0 = toLight0 / max(distance0, 0.0001f);
            float attenuation0 = 1.0f / (1.0f + 0.25f * distance0 + 0.08f * distance0 * distance0);
            float rangeFade0 = saturate(1.0f - (distance0 / max(spotlightParams0.z, 0.0001f)));
            float spotFactor0 = ComputeSpotFactor(lightDirection0.xyz, lightDir0, spotlightParams0.x, spotlightParams0.y);
            float diffuse0 = max(dot(normal, lightDir0), 0.0f);
            float3 reflection0 = reflect(-lightDir0, normal);
            float specular0 = pow(max(dot(viewDirection, reflection0), 0.0f), specularPower);
            totalLight += ((diffuse0 * albedo) + specular0.xxx) * lightColor0.rgb * lightColor0.a * attenuation0 * rangeFade0 * rangeFade0 * spotFactor0 * shadow0;

            float3 toLight1 = lightPosition1.xyz - input.worldPosition;
            float distance1 = length(toLight1);
            float3 lightDir1 = toLight1 / max(distance1, 0.0001f);
            float attenuation1 = 1.0f / (1.0f + 0.25f * distance1 + 0.08f * distance1 * distance1);
            float rangeFade1 = saturate(1.0f - (distance1 / max(spotlightParams1.z, 0.0001f)));
            float spotFactor1 = ComputeSpotFactor(lightDirection1.xyz, lightDir1, spotlightParams1.x, spotlightParams1.y);
            float diffuse1 = max(dot(normal, lightDir1), 0.0f);
            float3 reflection1 = reflect(-lightDir1, normal);
            float specular1 = pow(max(dot(viewDirection, reflection1), 0.0f), specularPower);
            totalLight += ((diffuse1 * albedo) + specular1.xxx) * lightColor1.rgb * lightColor1.a * attenuation1 * rangeFade1 * rangeFade1 * spotFactor1 * shadow1;

            float3 directionalToLight = normalize(-directionalLightDirection.xyz);
            float diffuseDirectional = max(dot(normal, directionalToLight), 0.0f);
            float3 reflectionDirectional = reflect(-directionalToLight, normal);
            float specularDirectional = pow(max(dot(viewDirection, reflectionDirectional), 0.0f), specularPower);
            totalLight += ((diffuseDirectional * albedo) + specularDirectional.xxx) * directionalLightColor.rgb * directionalLightColor.a * shadow2;

            return float4(saturate(totalLight), 1.0f);
        }
    )";

    const char shadowVertexShaderSource[] = R"(
        cbuffer ShadowFrameBuffer : register(b0)
        {
            float4x4 lightViewProjection;
        };

        cbuffer ObjectBuffer : register(b1)
        {
            float4x4 world;
            float4x4 worldInverseTranspose;
            float4 materialColor;
            float4 materialParams;
        };

        struct VSInput
        {
            float3 position : POSITION;
            float3 normal : NORMAL;
            float2 uv : TEXCOORD0;
        };

        float4 main(VSInput input) : SV_POSITION
        {
            float4 worldPosition = mul(float4(input.position, 1.0f), world);
            return mul(worldPosition, lightViewProjection);
        }
    )";

    const char debugVertexShaderSource[] = R"(
        struct VSOutput
        {
            float4 position : SV_POSITION;
            float2 uv : TEXCOORD0;
        };

        VSOutput main(uint vertexId : SV_VertexID)
        {
            VSOutput output;

            float2 positions[4] = {
                float2(-1.0f, 1.0f),
                float2(1.0f, 1.0f),
                float2(-1.0f, -1.0f),
                float2(1.0f, -1.0f)};

            float2 uvs[4] = {
                float2(0.0f, 0.0f),
                float2(1.0f, 0.0f),
                float2(0.0f, 1.0f),
                float2(1.0f, 1.0f)};

            output.position = float4(positions[vertexId], 0.0f, 1.0f);
            output.uv = uvs[vertexId];
            return output;
        }
    )";

    const char debugPixelShaderSource[] = R"(
        Texture2D shadowMap0 : register(t0);
        Texture2D shadowMap1 : register(t1);
        Texture2D shadowMap2 : register(t2);
        SamplerState debugSampler : register(s0);

        cbuffer DebugBuffer : register(b0)
        {
            float4 params;
        };

        struct PSInput
        {
            float4 position : SV_POSITION;
            float2 uv : TEXCOORD0;
        };

        float4 main(PSInput input) : SV_TARGET
        {
            float mapIndex = params.x;
            float depthSample = 0.0f;
            if (mapIndex < 0.5f)
            {
                depthSample = shadowMap0.SampleLevel(debugSampler, input.uv, 0).r;
            }
            else
            {
                if (mapIndex < 1.5f)
                {
                    depthSample = shadowMap1.SampleLevel(debugSampler, input.uv, 0).r;
                }
                else
                {
                    depthSample = shadowMap2.SampleLevel(debugSampler, input.uv, 0).r;
                }
            }

            return float4(depthSample.xxx, 1.0f);
        }
    )";

    const char skyboxVertexShaderSource[] = R"(
        cbuffer SkyboxFrameBuffer : register(b0)
        {
            float4x4 viewProjection;
        };

        struct VSInput
        {
            float3 position : POSITION;
        };

        struct VSOutput
        {
            float4 position : SV_POSITION;
            float3 direction : TEXCOORD0;
        };

        VSOutput main(VSInput input)
        {
            VSOutput output;
            output.direction = input.position;
            float4 clipPos = mul(float4(input.position, 1.0f), viewProjection);
            output.position = clipPos.xyww;
            return output;
        }
    )";

    const char skyboxPixelShaderSource[] = R"(
        TextureCube skyboxTexture : register(t0);
        SamplerState skyboxSampler : register(s0);

        struct PSInput
        {
            float4 position : SV_POSITION;
            float3 direction : TEXCOORD0;
        };

        float4 main(PSInput input) : SV_TARGET
        {
            float3 sampleDirection = normalize(input.direction);
            return skyboxTexture.Sample(skyboxSampler, sampleDirection);
        }
    )";

    UINT shaderCompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    shaderCompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> sceneVertexShaderBytecode;
    ComPtr<ID3DBlob> scenePixelShaderBytecode;
    ComPtr<ID3DBlob> shadowVertexShaderBytecode;
    ComPtr<ID3DBlob> debugVertexShaderBytecode;
    ComPtr<ID3DBlob> debugPixelShaderBytecode;
    ComPtr<ID3DBlob> skyboxVertexShaderBytecode;
    ComPtr<ID3DBlob> skyboxPixelShaderBytecode;
    ComPtr<ID3DBlob> errorBlob;

    result = D3DCompile(
        sceneVertexShaderSource,
        sizeof(sceneVertexShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        shaderCompileFlags,
        0,
        &sceneVertexShaderBytecode,
        &errorBlob);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    result = D3DCompile(
        skyboxVertexShaderSource,
        sizeof(skyboxVertexShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        shaderCompileFlags,
        0,
        &skyboxVertexShaderBytecode,
        &errorBlob);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    result = D3DCompile(
        skyboxPixelShaderSource,
        sizeof(skyboxPixelShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        shaderCompileFlags,
        0,
        &skyboxPixelShaderBytecode,
        &errorBlob);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    result = D3DCompile(
        scenePixelShaderSource,
        sizeof(scenePixelShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        shaderCompileFlags,
        0,
        &scenePixelShaderBytecode,
        &errorBlob);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    result = D3DCompile(
        shadowVertexShaderSource,
        sizeof(shadowVertexShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        shaderCompileFlags,
        0,
        &shadowVertexShaderBytecode,
        &errorBlob);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    result = D3DCompile(
        debugVertexShaderSource,
        sizeof(debugVertexShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        shaderCompileFlags,
        0,
        &debugVertexShaderBytecode,
        &errorBlob);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    result = D3DCompile(
        debugPixelShaderSource,
        sizeof(debugPixelShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        shaderCompileFlags,
        0,
        &debugPixelShaderBytecode,
        &errorBlob);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11VertexShader> sceneVertexShader;
    result = device->CreateVertexShader(
        sceneVertexShaderBytecode->GetBufferPointer(),
        sceneVertexShaderBytecode->GetBufferSize(),
        nullptr,
        &sceneVertexShader);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11PixelShader> scenePixelShader;
    result = device->CreatePixelShader(
        scenePixelShaderBytecode->GetBufferPointer(),
        scenePixelShaderBytecode->GetBufferSize(),
        nullptr,
        &scenePixelShader);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11VertexShader> shadowVertexShader;
    result = device->CreateVertexShader(
        shadowVertexShaderBytecode->GetBufferPointer(),
        shadowVertexShaderBytecode->GetBufferSize(),
        nullptr,
        &shadowVertexShader);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11VertexShader> debugVertexShader;
    result = device->CreateVertexShader(
        debugVertexShaderBytecode->GetBufferPointer(),
        debugVertexShaderBytecode->GetBufferSize(),
        nullptr,
        &debugVertexShader);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11PixelShader> debugPixelShader;
    result = device->CreatePixelShader(
        debugPixelShaderBytecode->GetBufferPointer(),
        debugPixelShaderBytecode->GetBufferSize(),
        nullptr,
        &debugPixelShader);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11VertexShader> skyboxVertexShader;
    result = device->CreateVertexShader(
        skyboxVertexShaderBytecode->GetBufferPointer(),
        skyboxVertexShaderBytecode->GetBufferSize(),
        nullptr,
        &skyboxVertexShader);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11PixelShader> skyboxPixelShader;
    result = device->CreatePixelShader(
        skyboxPixelShaderBytecode->GetBufferPointer(),
        skyboxPixelShaderBytecode->GetBufferSize(),
        nullptr,
        &skyboxPixelShader);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ComPtr<ID3D11InputLayout> inputLayout;
    result = device->CreateInputLayout(
        inputLayoutDesc,
        static_cast<UINT>(sizeof(inputLayoutDesc) / sizeof(inputLayoutDesc[0])),
        sceneVertexShaderBytecode->GetBufferPointer(),
        sceneVertexShaderBytecode->GetBufferSize(),
        &inputLayout);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_INPUT_ELEMENT_DESC skyboxInputLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ComPtr<ID3D11InputLayout> skyboxInputLayout;
    result = device->CreateInputLayout(
        skyboxInputLayoutDesc,
        static_cast<UINT>(sizeof(skyboxInputLayoutDesc) / sizeof(skyboxInputLayoutDesc[0])),
        skyboxVertexShaderBytecode->GetBufferPointer(),
        skyboxVertexShaderBytecode->GetBufferSize(),
        &skyboxInputLayout);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    const Vertex cubeVertices[] = {
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

        {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

        {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

        {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
    };

    const std::uint16_t cubeIndices[] = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };

    const Vertex planeVertices[] = {
        {{-6.0f, -1.0f, 6.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{6.0f, -1.0f, 6.0f}, {0.0f, 1.0f, 0.0f}, {6.0f, 0.0f}},
        {{6.0f, -1.0f, -6.0f}, {0.0f, 1.0f, 0.0f}, {6.0f, 6.0f}},
        {{-6.0f, -1.0f, -6.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 6.0f}},
    };

    const std::uint16_t planeIndices[] = {
        0, 1, 2,
        0, 2, 3,
    };

    struct SkyboxVertex
    {
        float position[3];
    };

    const SkyboxVertex skyboxVertices[] = {
        {{-1.0f, 1.0f, -1.0f}},
        {{1.0f, 1.0f, -1.0f}},
        {{1.0f, -1.0f, -1.0f}},
        {{-1.0f, -1.0f, -1.0f}},
        {{-1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}},
    };

    const std::uint16_t skyboxIndices[] = {
        0, 1, 2, 0, 2, 3,
        4, 7, 6, 4, 6, 5,
        4, 5, 1, 4, 1, 0,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        4, 0, 3, 4, 3, 7,
    };

    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(cubeVertices));
    vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = cubeVertices;

    ComPtr<ID3D11Buffer> cubeVertexBuffer;
    result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &cubeVertexBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(cubeIndices));
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = cubeIndices;

    ComPtr<ID3D11Buffer> cubeIndexBuffer;
    result = device->CreateBuffer(&indexBufferDesc, &indexData, &cubeIndexBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(planeVertices));
    vertexData.pSysMem = planeVertices;

    ComPtr<ID3D11Buffer> planeVertexBuffer;
    result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &planeVertexBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(planeIndices));
    indexData.pSysMem = planeIndices;

    ComPtr<ID3D11Buffer> planeIndexBuffer;
    result = device->CreateBuffer(&indexBufferDesc, &indexData, &planeIndexBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_BUFFER_DESC skyboxVertexBufferDesc{};
    skyboxVertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(skyboxVertices));
    skyboxVertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    skyboxVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA skyboxVertexData{};
    skyboxVertexData.pSysMem = skyboxVertices;

    ComPtr<ID3D11Buffer> skyboxVertexBuffer;
    result = device->CreateBuffer(&skyboxVertexBufferDesc, &skyboxVertexData, &skyboxVertexBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_BUFFER_DESC skyboxIndexBufferDesc{};
    skyboxIndexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(skyboxIndices));
    skyboxIndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    skyboxIndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA skyboxIndexData{};
    skyboxIndexData.pSysMem = skyboxIndices;

    ComPtr<ID3D11Buffer> skyboxIndexBuffer;
    result = device->CreateBuffer(&skyboxIndexBufferDesc, &skyboxIndexData, &skyboxIndexBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    const UINT skyboxIndexCount = static_cast<UINT>(sizeof(skyboxIndices) / sizeof(skyboxIndices[0]));

    const UINT cubeIndexCount = static_cast<UINT>(sizeof(cubeIndices) / sizeof(cubeIndices[0]));
    const UINT planeIndexCount = static_cast<UINT>(sizeof(planeIndices) / sizeof(planeIndices[0]));

    MeshRenderData cubeMeshData{};
    cubeMeshData.vertexBuffer = cubeVertexBuffer.Get();
    cubeMeshData.indexBuffer = cubeIndexBuffer.Get();
    cubeMeshData.indexCount = cubeIndexCount;
    cubeMeshData.stride = sizeof(Vertex);
    cubeMeshData.offset = 0;

    MeshRenderData planeMeshData{};
    planeMeshData.vertexBuffer = planeVertexBuffer.Get();
    planeMeshData.indexBuffer = planeIndexBuffer.Get();
    planeMeshData.indexCount = planeIndexCount;
    planeMeshData.stride = sizeof(Vertex);
    planeMeshData.offset = 0;

    SceneGraph sceneGraph;
    GameObject& groundPlane = sceneGraph.CreateGameObject("GroundPlane", MeshType::Plane);
    groundPlane.SetMaterialColor(XMFLOAT4(0.58f, 0.58f, 0.62f, 1.0f));
    groundPlane.SetUsesTexture(false);
    groundPlane.SetCastsShadow(false);

    GameObject& spinningCube = sceneGraph.CreateGameObject("SpinningCube", MeshType::Cube, groundPlane.GetId());
    spinningCube.SetMaterialColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    spinningCube.SetUsesTexture(true);
    spinningCube.SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));

    std::vector<ModelResource> loadedModels;
    auto getModelById = [&](std::uint32_t modelId) -> const ModelResource*
    {
        if (modelId >= loadedModels.size())
        {
            return nullptr;
        }
        return &loadedModels[modelId];
    };

    const std::string sponzaPath = ResolveSponzaPath();
    if (!sponzaPath.empty())
    {
        ModelResource sponzaModel;
        if (LoadModelFromFile(device.Get(), sponzaPath, sponzaModel))
        {
            const std::uint32_t sponzaModelId = static_cast<std::uint32_t>(loadedModels.size());
            loadedModels.push_back(std::move(sponzaModel));

            GameObject& sponzaObject = sceneGraph.CreateGameObject("Sponza", MeshType::Model);
            sponzaObject.SetModelId(sponzaModelId);
            sponzaObject.SetUsesTexture(true);
            sponzaObject.SetPosition(XMFLOAT3(0.0f, -1.0f, 0.0f));
            sponzaObject.SetScale(XMFLOAT3(0.01f, 0.01f, 0.01f));
            sponzaObject.SetMaterialColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
            sponzaObject.SetCastsShadow(true);
        }
    }

    D3D11_BUFFER_DESC frameBufferDesc{};
    frameBufferDesc.ByteWidth = sizeof(FrameBufferData);
    frameBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    frameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> frameBuffer;
    result = device->CreateBuffer(&frameBufferDesc, nullptr, &frameBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_BUFFER_DESC objectBufferDesc{};
    objectBufferDesc.ByteWidth = sizeof(ObjectBufferData);
    objectBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    objectBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> objectBuffer;
    result = device->CreateBuffer(&objectBufferDesc, nullptr, &objectBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_BUFFER_DESC shadowFrameBufferDesc{};
    shadowFrameBufferDesc.ByteWidth = sizeof(ShadowFrameBufferData);
    shadowFrameBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowFrameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> shadowFrameBuffer;
    result = device->CreateBuffer(&shadowFrameBufferDesc, nullptr, &shadowFrameBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_BUFFER_DESC debugBufferDesc{};
    debugBufferDesc.ByteWidth = sizeof(DebugBufferData);
    debugBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    debugBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> debugBuffer;
    result = device->CreateBuffer(&debugBufferDesc, nullptr, &debugBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_BUFFER_DESC skyboxFrameBufferDesc{};
    skyboxFrameBufferDesc.ByteWidth = sizeof(SkyboxFrameBufferData);
    skyboxFrameBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    skyboxFrameBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> skyboxFrameBuffer;
    result = device->CreateBuffer(&skyboxFrameBufferDesc, nullptr, &skyboxFrameBuffer);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11ShaderResourceView> faceTexture;
    if (!LoadTextureFromFile(device.Get(), kFaceTexturePath, faceTexture))
    {
        MessageBoxA(window, "Could not load cage.jpg from the working directory.", "Texture Load Error", MB_OK | MB_ICONERROR);
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    std::array<std::wstring, 6> skyboxFacePaths;
    if (!ResolveSkyboxFacePaths(skyboxFacePaths))
    {
        MessageBoxA(window, "Could not resolve skybox face textures from Assets/Images/Skybox/Daylight.", "Skybox Load Error", MB_OK | MB_ICONERROR);
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    ComPtr<ID3D11ShaderResourceView> skyboxCubemap;
    if (!LoadCubemapFromFiles(device.Get(), skyboxFacePaths, skyboxCubemap))
    {
        MessageBoxA(window, "Could not load cubemap textures for skybox.", "Skybox Load Error", MB_OK | MB_ICONERROR);
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_SAMPLER_DESC textureSamplerDesc{};
    textureSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    textureSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    textureSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    textureSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    textureSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    textureSamplerDesc.MinLOD = 0.0f;
    textureSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> textureSampler;
    result = device->CreateSamplerState(&textureSamplerDesc, &textureSampler);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_SAMPLER_DESC shadowSamplerDesc{};
    shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDesc.BorderColor[0] = 1.0f;
    shadowSamplerDesc.BorderColor[1] = 1.0f;
    shadowSamplerDesc.BorderColor[2] = 1.0f;
    shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.MinLOD = 0.0f;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> shadowSampler;
    result = device->CreateSamplerState(&shadowSamplerDesc, &shadowSampler);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_SAMPLER_DESC shadowDepthSamplerDesc{};
    shadowDepthSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    shadowDepthSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowDepthSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowDepthSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowDepthSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    shadowDepthSamplerDesc.BorderColor[0] = 1.0f;
    shadowDepthSamplerDesc.BorderColor[1] = 1.0f;
    shadowDepthSamplerDesc.BorderColor[2] = 1.0f;
    shadowDepthSamplerDesc.BorderColor[3] = 1.0f;
    shadowDepthSamplerDesc.MinLOD = 0.0f;
    shadowDepthSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> shadowDepthSampler;
    result = device->CreateSamplerState(&shadowDepthSamplerDesc, &shadowDepthSampler);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_SAMPLER_DESC debugSamplerDesc{};
    debugSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    debugSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    debugSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    debugSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    debugSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    debugSamplerDesc.MinLOD = 0.0f;
    debugSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> debugSampler;
    result = device->CreateSamplerState(&debugSamplerDesc, &debugSampler);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_SAMPLER_DESC skyboxSamplerDesc{};
    skyboxSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    skyboxSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    skyboxSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    skyboxSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    skyboxSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    skyboxSamplerDesc.MinLOD = 0.0f;
    skyboxSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> skyboxSampler;
    result = device->CreateSamplerState(&skyboxSamplerDesc, &skyboxSampler);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_RASTERIZER_DESC shadowRasterizerDesc{};
    shadowRasterizerDesc.FillMode = D3D11_FILL_SOLID;
    shadowRasterizerDesc.CullMode = D3D11_CULL_BACK;
    shadowRasterizerDesc.DepthClipEnable = TRUE;
    shadowRasterizerDesc.DepthBias = 1000;
    shadowRasterizerDesc.SlopeScaledDepthBias = 1.0f;
    shadowRasterizerDesc.DepthBiasClamp = 0.0f;

    ComPtr<ID3D11RasterizerState> shadowRasterizerState;
    result = device->CreateRasterizerState(&shadowRasterizerDesc, &shadowRasterizerState);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_RASTERIZER_DESC skyboxRasterizerDesc{};
    skyboxRasterizerDesc.FillMode = D3D11_FILL_SOLID;
    skyboxRasterizerDesc.CullMode = D3D11_CULL_FRONT;
    skyboxRasterizerDesc.DepthClipEnable = TRUE;

    ComPtr<ID3D11RasterizerState> skyboxRasterizerState;
    result = device->CreateRasterizerState(&skyboxRasterizerDesc, &skyboxRasterizerState);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_DEPTH_STENCIL_DESC skyboxDepthStencilDesc{};
    skyboxDepthStencilDesc.DepthEnable = TRUE;
    skyboxDepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyboxDepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    skyboxDepthStencilDesc.StencilEnable = FALSE;

    ComPtr<ID3D11DepthStencilState> skyboxDepthStencilState;
    result = device->CreateDepthStencilState(&skyboxDepthStencilDesc, &skyboxDepthStencilState);
    if (FAILED(result))
    {
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    D3D11_VIEWPORT sceneViewport{};
    sceneViewport.TopLeftX = 0.0f;
    sceneViewport.TopLeftY = 0.0f;
    sceneViewport.Width = static_cast<FLOAT>(sceneRenderWidth);
    sceneViewport.Height = static_cast<FLOAT>(sceneRenderHeight);
    sceneViewport.MinDepth = 0.0f;
    sceneViewport.MaxDepth = 1.0f;

    D3D11_VIEWPORT shadowViewport{};
    shadowViewport.TopLeftX = 0.0f;
    shadowViewport.TopLeftY = 0.0f;
    shadowViewport.Width = static_cast<FLOAT>(kShadowMapSize);
    shadowViewport.Height = static_cast<FLOAT>(kShadowMapSize);
    shadowViewport.MinDepth = 0.0f;
    shadowViewport.MaxDepth = 1.0f;

    D3D11_VIEWPORT debugViewport{};
    debugViewport.TopLeftX = 20.0f;
    debugViewport.TopLeftY = 20.0f;
    debugViewport.Width = 300.0f;
    debugViewport.Height = 300.0f;
    debugViewport.MinDepth = 0.0f;
    debugViewport.MaxDepth = 1.0f;

    XMFLOAT3 cameraPosition = {0.0f, 1.2f, -3.0f};
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;

    const XMVECTOR lightPosition0 = XMVectorSet(-2.0f, 2.5f, 0.0f, 1.0f);
    const XMVECTOR lightPosition1 = XMVectorSet(2.0f, 2.5f, 0.0f, 1.0f);
    const XMVECTOR lightTarget0 = XMVectorSet(0.0f, -0.3f, 0.0f, 1.0f);
    const XMVECTOR lightTarget1 = XMVectorSet(0.0f, -0.3f, 0.0f, 1.0f);
    const XMVECTOR directionalLightTarget = XMVectorSet(0.0f, -0.3f, 0.0f, 1.0f);
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMMATRIX lightProjection = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, 0.1f, 15.0f);
    const XMMATRIX directionalLightProjection = XMMatrixOrthographicLH(16.0f, 16.0f, 0.1f, 30.0f);
    const XMVECTOR spotlightDirection0 = XMVector3Normalize(XMVectorSubtract(lightTarget0, lightPosition0));
    const XMVECTOR spotlightDirection1 = XMVector3Normalize(XMVectorSubtract(lightTarget1, lightPosition1));
    const float spotlightInnerCos = std::cos(XMConvertToRadians(20.0f));
    const float spotlightOuterCos = std::cos(XMConvertToRadians(30.0f));
    const float spotlightRange = 12.0f;

    const auto startTime = std::chrono::steady_clock::now();
    auto previousFrameTime = startTime;
    int shadowDebugMode = 0;
    bool debugToggleWasDown = false;
    bool wasCapturingMouseLook = false;
    bool runtimeMsaaEnabled = msaaSupported;
    bool runtimePcssEnabled = true;
    float runtimePcssSearchRadius = 0.01f;
    float runtimePcssMaxFilterRadius = 0.05f;
    float directionalYaw = XMConvertToRadians(-90.0f);
    float directionalPitch = XMConvertToRadians(-35.0f);
    XMFLOAT3 directionalColor = {1.0f, 1.0f, 1.0f};
    float directionalIntensity = 0.55f;
    float editorLeftPanelRatio = 0.34f;
    float editorInspectorPanelRatio = 0.24f;
    std::uint32_t selectedGameObjectId = groundPlane.GetId();

    auto updateWindowTitle = [window](
        int mode,
        bool msaaActive,
        bool hasMsaaSupport,
        bool pcssActive,
        float pcssSearchRadius,
        float pcssMaxFilterRadius)
    {
        const char* modeLabel = "M: Normal";
        if (mode == 1)
        {
            modeLabel = "M: Shadow Map 0";
        }
        else if (mode == 2)
        {
            modeLabel = "M: Shadow Map 1";
        }
        else if (mode == 3)
        {
            modeLabel = "M: Shadow Map 2 (Directional)";
        }

        const char* msaaLabel = hasMsaaSupport ? (msaaActive ? "ON" : "OFF") : "UNSUPPORTED";
        const char* pcssLabel = pcssActive ? "ON" : "OFF";

        char titleBuffer[320]{};
        std::snprintf(
            titleBuffer,
            sizeof(titleBuffer),
            "DirectX 11 Spotlights + Shadows [%-12s] [MSAA %s] [PCSS %s] [Search %.3f] [Soft %.3f]",
            modeLabel,
            msaaLabel,
            pcssLabel,
            pcssSearchRadius,
            pcssMaxFilterRadius);
        SetWindowTextA(window, titleBuffer);
    };

    updateWindowTitle(
        shadowDebugMode,
        runtimeMsaaEnabled,
        msaaSupported,
        runtimePcssEnabled,
        runtimePcssSearchRadius,
        runtimePcssMaxFilterRadius);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& imguiIo = ImGui::GetIO();
    imguiIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX11_Init(device.Get(), context.Get()))
    {
        ImGui::DestroyContext();
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return -1;
    }

    MSG message{};
    while (message.message != WM_QUIT)
    {
        if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
            continue;
        }

        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
        {
            PostQuitMessage(0);
            continue;
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(currentTime - previousFrameTime).count();
        previousFrameTime = currentTime;
        const float elapsedSeconds = std::chrono::duration<float>(currentTime - startTime).count();

        RECT clientRect{};
        GetClientRect(window, &clientRect);
        const UINT clientWidth = static_cast<UINT>((std::max)(0L, clientRect.right - clientRect.left));
        const UINT clientHeight = static_cast<UINT>((std::max)(0L, clientRect.bottom - clientRect.top));

        if (clientWidth == 0 || clientHeight == 0)
        {
            Sleep(16);
            continue;
        }

        if (clientWidth != sceneRenderWidth || clientHeight != sceneRenderHeight)
        {
            context->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11ShaderResourceView* nullSrvs[4] = {nullptr, nullptr, nullptr, nullptr};
            context->PSSetShaderResources(0, 4, nullSrvs);

            backBuffer.Reset();
            renderTargetView.Reset();
            gameViewColorBuffer.Reset();
            gameViewRenderTargetView.Reset();
            gameViewShaderResourceView.Reset();
            gameViewMsaaColorBuffer.Reset();
            gameViewMsaaRenderTargetView.Reset();
            depthBuffer.Reset();
            msaaDepthBuffer.Reset();
            msaaDepthStencilView.Reset();
            depthStencilView.Reset();

            const HRESULT resizeResult = swapChain->ResizeBuffers(0, clientWidth, clientHeight, DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(resizeResult) || !createSizeDependentResources(clientWidth, clientHeight))
            {
                if (shouldUninitializeCom)
                {
                    CoUninitialize();
                }
                return -1;
            }

            sceneRenderWidth = clientWidth;
            sceneRenderHeight = clientHeight;
            sceneViewport.Width = static_cast<FLOAT>(sceneRenderWidth);
            sceneViewport.Height = static_cast<FLOAT>(sceneRenderHeight);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        auto clampRange = [](float value, float minValue, float maxValue)
        {
            return (std::max)(minValue, (std::min)(value, maxValue));
        };

        const float lightRotationStep = 1.4f * deltaSeconds;
        if ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0)
        {
            directionalYaw -= lightRotationStep;
        }
        if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0)
        {
            directionalYaw += lightRotationStep;
        }
        if ((GetAsyncKeyState(VK_UP) & 0x8000) != 0)
        {
            directionalPitch += lightRotationStep;
        }
        if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0)
        {
            directionalPitch -= lightRotationStep;
        }
        directionalPitch = clampRange(directionalPitch, -1.4f, 1.4f);

        const float lightColorStep = 0.7f * deltaSeconds;
        if ((GetAsyncKeyState('U') & 0x8000) != 0)
        {
            directionalColor.x += lightColorStep;
        }
        if ((GetAsyncKeyState('J') & 0x8000) != 0)
        {
            directionalColor.x -= lightColorStep;
        }
        if ((GetAsyncKeyState('I') & 0x8000) != 0)
        {
            directionalColor.y += lightColorStep;
        }
        if ((GetAsyncKeyState('K') & 0x8000) != 0)
        {
            directionalColor.y -= lightColorStep;
        }
        if ((GetAsyncKeyState('O') & 0x8000) != 0)
        {
            directionalColor.z += lightColorStep;
        }
        if ((GetAsyncKeyState('L') & 0x8000) != 0)
        {
            directionalColor.z -= lightColorStep;
        }
        directionalColor.x = clampRange(directionalColor.x, 0.0f, 2.0f);
        directionalColor.y = clampRange(directionalColor.y, 0.0f, 2.0f);
        directionalColor.z = clampRange(directionalColor.z, 0.0f, 2.0f);

        if ((GetAsyncKeyState('P') & 0x8000) != 0)
        {
            directionalIntensity += lightColorStep;
        }
        if ((GetAsyncKeyState('N') & 0x8000) != 0)
        {
            directionalIntensity -= lightColorStep;
        }
        directionalIntensity = clampRange(directionalIntensity, 0.0f, 2.0f);

        const bool debugToggleDown = (GetAsyncKeyState('M') & 0x8000) != 0;
        if (debugToggleDown && !debugToggleWasDown)
        {
            shadowDebugMode = (shadowDebugMode + 1) % 4;
            updateWindowTitle(
                shadowDebugMode,
                runtimeMsaaEnabled,
                msaaSupported,
                runtimePcssEnabled,
                runtimePcssSearchRadius,
                runtimePcssMaxFilterRadius);
        }
        debugToggleWasDown = debugToggleDown;

        bool uiChanged = false;
        bool isGameViewHovered = false;
        bool isGameViewAreaHovered = false;
        POINT mouseLookCenterClientCurrent = mouseCenterClient;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(imguiIo.DisplaySize, ImGuiCond_Always);
        ImGuiWindowFlags editorWindowFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse;

        if (ImGui::Begin("Engine Editor", nullptr, editorWindowFlags))
        {
            const float splitterWidth = 6.0f;
            const float minLeftPanelWidth = 260.0f;
            const float minGamePanelWidth = 320.0f;
            const float minInspectorPanelWidth = 260.0f;
            const float totalPanelWidth = ImGui::GetContentRegionAvail().x;

            const float maxLeftPanelWidth = (std::max)(
                minLeftPanelWidth,
                totalPanelWidth - minGamePanelWidth - minInspectorPanelWidth - (2.0f * splitterWidth));
            float leftPanelWidth = totalPanelWidth * editorLeftPanelRatio;
            leftPanelWidth = (std::max)(minLeftPanelWidth, (std::min)(leftPanelWidth, maxLeftPanelWidth));

            const float maxInspectorPanelWidth = (std::max)(
                minInspectorPanelWidth,
                totalPanelWidth - leftPanelWidth - minGamePanelWidth - (2.0f * splitterWidth));
            float inspectorPanelWidth = totalPanelWidth * editorInspectorPanelRatio;
            inspectorPanelWidth = (std::max)(minInspectorPanelWidth, (std::min)(inspectorPanelWidth, maxInspectorPanelWidth));
            float gamePanelWidth = totalPanelWidth - leftPanelWidth - inspectorPanelWidth - (2.0f * splitterWidth);
            if (gamePanelWidth < minGamePanelWidth)
            {
                const float deficit = minGamePanelWidth - gamePanelWidth;
                inspectorPanelWidth = (std::max)(minInspectorPanelWidth, inspectorPanelWidth - deficit);
                gamePanelWidth = totalPanelWidth - leftPanelWidth - inspectorPanelWidth - (2.0f * splitterWidth);
            }

            ImGui::BeginChild("LeftPanel", ImVec2(leftPanelWidth, 0.0f), true);

            ImGui::Text("Debug + Scene Graph");
            ImGui::Separator();
            ImGui::Text("M: Shadow debug mode");
            ImGui::Text("Hold Right Mouse Button in Game View for camera look");
            ImGui::Separator();

            if (msaaSupported)
            {
                bool uiMsaa = runtimeMsaaEnabled;
                if (ImGui::Checkbox("MSAA", &uiMsaa))
                {
                    runtimeMsaaEnabled = uiMsaa;
                    uiChanged = true;
                }
            }
            else
            {
                ImGui::Text("MSAA: unsupported on this adapter");
            }

            bool uiPcss = runtimePcssEnabled;
            if (ImGui::Checkbox("PCSS Soft Shadows", &uiPcss))
            {
                runtimePcssEnabled = uiPcss;
                uiChanged = true;
            }

            float uiSearchRadius = runtimePcssSearchRadius;
            if (ImGui::SliderFloat("PCSS Search Radius", &uiSearchRadius, 1.0f / static_cast<float>(kShadowMapSize), 0.05f, "%.4f"))
            {
                runtimePcssSearchRadius = uiSearchRadius;
                uiChanged = true;
            }

            float uiSoftRadius = runtimePcssMaxFilterRadius;
            if (ImGui::SliderFloat("PCSS Soft Radius", &uiSoftRadius, runtimePcssSearchRadius, 0.20f, "%.4f"))
            {
                runtimePcssMaxFilterRadius = uiSoftRadius;
                uiChanged = true;
            }

            ImGui::SliderFloat("Directional Intensity", &directionalIntensity, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat3("Directional Color", &directionalColor.x, 0.0f, 2.0f, "%.2f");
            ImGui::Text("Shadow Debug Mode: %d", shadowDebugMode);

            ImGui::Separator();
            ImGui::Text("Scene Graph (%zu objects)", sceneGraph.GetObjects().size());

            auto drawSceneNode = [&](auto&& self, std::uint32_t objectId) -> void
            {
                const GameObject* object = sceneGraph.GetById(objectId);
                if (object == nullptr)
                {
                    return;
                }

                ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (object->GetChildren().empty())
                {
                    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
                }
                if (object->GetId() == selectedGameObjectId)
                {
                    nodeFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const bool open = ImGui::TreeNodeEx(
                    reinterpret_cast<void*>(static_cast<uintptr_t>(object->GetId())),
                    nodeFlags,
                    "%s",
                    object->GetName().c_str());
                if (ImGui::IsItemClicked())
                {
                    selectedGameObjectId = object->GetId();
                }

                if (open)
                {
                    for (std::uint32_t childId : object->GetChildren())
                    {
                        self(self, childId);
                    }
                    ImGui::TreePop();
                }
            };

            for (std::uint32_t rootId : sceneGraph.GetRootIds())
            {
                drawSceneNode(drawSceneNode, rootId);
            }

            ImGui::EndChild();

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::InvisibleButton("VerticalSplitter", ImVec2(splitterWidth, ImGui::GetContentRegionAvail().y));
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }

            if (ImGui::IsItemActive())
            {
                leftPanelWidth += imguiIo.MouseDelta.x;
                leftPanelWidth = (std::max)(minLeftPanelWidth, (std::min)(leftPanelWidth, maxLeftPanelWidth));
                editorLeftPanelRatio = leftPanelWidth / (std::max)(1.0f, totalPanelWidth);
                gamePanelWidth = totalPanelWidth - leftPanelWidth - inspectorPanelWidth - (2.0f * splitterWidth);
            }

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::BeginChild("GameViewPanel", ImVec2(gamePanelWidth, 0.0f), true);
            isGameViewAreaHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            ImGui::Text("Game View");
            ImGui::Separator();

            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float targetAspect = static_cast<float>(sceneRenderWidth) / static_cast<float>(sceneRenderHeight);
            float imageWidth = (std::max)(1.0f, avail.x);
            float imageHeight = imageWidth / targetAspect;
            if (imageHeight > avail.y)
            {
                imageHeight = (std::max)(1.0f, avail.y);
                imageWidth = imageHeight * targetAspect;
            }

            const ImVec2 cursorPos = ImGui::GetCursorPos();
            const float offsetX = (std::max)(0.0f, (avail.x - imageWidth) * 0.5f);
            const float offsetY = (std::max)(0.0f, (avail.y - imageHeight) * 0.5f);
            ImGui::SetCursorPos(ImVec2(cursorPos.x + offsetX, cursorPos.y + offsetY));

            ImGui::Image(
                static_cast<ImTextureID>(gameViewShaderResourceView.Get()),
                ImVec2(imageWidth, imageHeight),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f));
            isGameViewHovered = ImGui::IsItemHovered();

            const ImVec2 imageMin = ImGui::GetItemRectMin();
            const ImVec2 imageMax = ImGui::GetItemRectMax();
            mouseLookCenterClientCurrent.x = static_cast<LONG>((imageMin.x + imageMax.x) * 0.5f);
            mouseLookCenterClientCurrent.y = static_cast<LONG>((imageMin.y + imageMax.y) * 0.5f);

            ImGui::EndChild();

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::InvisibleButton("InspectorSplitter", ImVec2(splitterWidth, ImGui::GetContentRegionAvail().y));
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if (ImGui::IsItemActive())
            {
                inspectorPanelWidth -= imguiIo.MouseDelta.x;
                inspectorPanelWidth = (std::max)(minInspectorPanelWidth, (std::min)(inspectorPanelWidth, maxInspectorPanelWidth));
                editorInspectorPanelRatio = inspectorPanelWidth / (std::max)(1.0f, totalPanelWidth);
            }

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::BeginChild("InspectorPanel", ImVec2(0.0f, 0.0f), true);
            ImGui::Text("Inspector");
            ImGui::Separator();

            GameObject* selectedObject = sceneGraph.GetById(selectedGameObjectId);
            if (selectedObject == nullptr)
            {
                ImGui::Text("No object selected.");
            }
            else
            {
                ImGui::Text("Name: %s", selectedObject->GetName().c_str());
                ImGui::Text("ID: %u", selectedObject->GetId());
                ImGui::Text("Mesh: %s", MeshTypeToString(selectedObject->GetMeshType()));

                TransformComponent& transform = selectedObject->GetTransform();
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat3("Position", &transform.position.x, 0.02f);
                    ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.02f);
                    ImGui::DragFloat3("Scale", &transform.scale.x, 0.02f);
                }

                RendererComponent& renderer = selectedObject->GetRenderer();
                if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Checkbox("Visible", &renderer.visible);
                    ImGui::Checkbox("Cast Shadows", &renderer.castsShadow);
                    ImGui::Checkbox("Use Texture", &renderer.useTexture);
                    ImGui::ColorEdit3("Material Color", &renderer.materialColor.x);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        if (uiChanged)
        {
            runtimePcssSearchRadius = clampRange(runtimePcssSearchRadius, 1.0f / static_cast<float>(kShadowMapSize), 0.05f);
            runtimePcssMaxFilterRadius = clampRange(runtimePcssMaxFilterRadius, runtimePcssSearchRadius, 0.20f);
            updateWindowTitle(
                shadowDebugMode,
                runtimeMsaaEnabled,
                msaaSupported,
                runtimePcssEnabled,
                runtimePcssSearchRadius,
                runtimePcssMaxFilterRadius);
        }

        const bool captureMouseLook =
            (GetForegroundWindow() == window) &&
            ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0) &&
            (isGameViewHovered || isGameViewAreaHovered || wasCapturingMouseLook);

        if (captureMouseLook)
        {
            if (!wasCapturingMouseLook)
            {
                mouseCenterClient = mouseLookCenterClientCurrent;
                POINT resetMousePosition = mouseCenterClient;
                ClientToScreen(window, &resetMousePosition);
                SetCursorPos(resetMousePosition.x, resetMousePosition.y);
                wasCapturingMouseLook = true;
            }

            POINT currentMousePosition{};
            if (GetCursorPos(&currentMousePosition))
            {
                ScreenToClient(window, &currentMousePosition);
                const float mouseDeltaX = static_cast<float>(currentMousePosition.x - mouseCenterClient.x);
                const float mouseDeltaY = static_cast<float>(currentMousePosition.y - mouseCenterClient.y);

                cameraYaw += mouseDeltaX * kLookSensitivity;
                cameraPitch -= mouseDeltaY * kLookSensitivity;

                if (cameraPitch > kMaxPitch)
                {
                    cameraPitch = kMaxPitch;
                }
                if (cameraPitch < -kMaxPitch)
                {
                    cameraPitch = -kMaxPitch;
                }

                POINT resetMousePosition = mouseCenterClient;
                ClientToScreen(window, &resetMousePosition);
                SetCursorPos(resetMousePosition.x, resetMousePosition.y);
            }
        }
        else
        {
            wasCapturingMouseLook = false;
        }

        const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
            std::cos(cameraPitch) * std::sin(cameraYaw),
            std::sin(cameraPitch),
            std::cos(cameraPitch) * std::cos(cameraYaw),
            0.0f));
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));

        XMVECTOR cameraPositionVector = XMLoadFloat3(&cameraPosition);
        const float moveStep = kMoveSpeed * deltaSeconds;
        if ((GetAsyncKeyState('W') & 0x8000) != 0)
        {
            cameraPositionVector = XMVectorAdd(cameraPositionVector, XMVectorScale(forward, moveStep));
        }
        if ((GetAsyncKeyState('S') & 0x8000) != 0)
        {
            cameraPositionVector = XMVectorSubtract(cameraPositionVector, XMVectorScale(forward, moveStep));
        }
        if ((GetAsyncKeyState('A') & 0x8000) != 0)
        {
            cameraPositionVector = XMVectorSubtract(cameraPositionVector, XMVectorScale(right, moveStep));
        }
        if ((GetAsyncKeyState('D') & 0x8000) != 0)
        {
            cameraPositionVector = XMVectorAdd(cameraPositionVector, XMVectorScale(right, moveStep));
        }
        XMStoreFloat3(&cameraPosition, cameraPositionVector);

        const float aspectRatio = static_cast<float>(sceneRenderWidth) / static_cast<float>(sceneRenderHeight);
        const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 100.0f);
        const XMMATRIX view = XMMatrixLookToLH(cameraPositionVector, forward, worldUp);
        const XMMATRIX viewProjection = view * projection;

        const XMMATRIX lightView0 = XMMatrixLookAtLH(lightPosition0, lightTarget0, worldUp);
        const XMMATRIX lightView1 = XMMatrixLookAtLH(lightPosition1, lightTarget1, worldUp);
        const XMMATRIX lightViewProjection0 = lightView0 * lightProjection;
        const XMMATRIX lightViewProjection1 = lightView1 * lightProjection;
        const XMVECTOR directionalLightDirection = XMVector3Normalize(XMVectorSet(
            std::cos(directionalPitch) * std::sin(directionalYaw),
            std::sin(directionalPitch),
            std::cos(directionalPitch) * std::cos(directionalYaw),
            0.0f));
        const XMVECTOR directionalLightPosition = XMVectorSubtract(directionalLightTarget, XMVectorScale(directionalLightDirection, 10.0f));
        const XMMATRIX lightView2 = XMMatrixLookAtLH(directionalLightPosition, directionalLightTarget, worldUp);
        const XMMATRIX lightViewProjection2 = lightView2 * directionalLightProjection;

        if (GameObject* cubeObject = sceneGraph.GetById(spinningCube.GetId()))
        {
            cubeObject->SetRotation(XMFLOAT3(0.0f, elapsedSeconds, 0.0f));
        }

        std::vector<RenderItem> renderItems;
        renderItems.reserve(sceneGraph.GetObjects().size());

        auto buildRenderItems = [&](auto&& self, std::uint32_t objectId, const XMMATRIX& parentWorld) -> void
        {
            const GameObject* gameObject = sceneGraph.GetById(objectId);
            if (gameObject == nullptr)
            {
                return;
            }

            const XMMATRIX localWorld = ComposeTransform(*gameObject);
            const XMMATRIX world = localWorld * parentWorld;
            const XMMATRIX worldInverseTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
            if (gameObject->IsVisible())
            {
                renderItems.push_back({gameObject, world, worldInverseTranspose});
            }

            for (std::uint32_t childId : gameObject->GetChildren())
            {
                self(self, childId, world);
            }
        };

        for (std::uint32_t rootId : sceneGraph.GetRootIds())
        {
            buildRenderItems(buildRenderItems, rootId, XMMatrixIdentity());
        }

        auto getMeshRenderData = [&](MeshType meshType) -> const MeshRenderData*
        {
            if (meshType == MeshType::Cube)
            {
                return &cubeMeshData;
            }
            if (meshType == MeshType::Plane)
            {
                return &planeMeshData;
            }
            return nullptr;
        };

        FrameBufferData frameData{};
        XMStoreFloat4x4(&frameData.viewProjection, XMMatrixTranspose(viewProjection));
        XMStoreFloat4x4(&frameData.lightViewProjection0, XMMatrixTranspose(lightViewProjection0));
        XMStoreFloat4x4(&frameData.lightViewProjection1, XMMatrixTranspose(lightViewProjection1));
        XMStoreFloat4x4(&frameData.lightViewProjection2, XMMatrixTranspose(lightViewProjection2));
        XMStoreFloat4(&frameData.cameraPosition, XMVectorSet(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f));
        XMStoreFloat4(&frameData.lightPosition0, lightPosition0);
        XMStoreFloat4(&frameData.lightPosition1, lightPosition1);
        XMStoreFloat4(&frameData.lightDirection0, spotlightDirection0);
        XMStoreFloat4(&frameData.lightDirection1, spotlightDirection1);
        frameData.lightColor0 = XMFLOAT4(1.0f, 0.92f, 0.85f, 1.35f);
        frameData.lightColor1 = XMFLOAT4(0.75f, 0.85f, 1.0f, 1.35f);
        frameData.spotlightParams0 = XMFLOAT4(spotlightInnerCos, spotlightOuterCos, spotlightRange, 0.0f);
        frameData.spotlightParams1 = XMFLOAT4(spotlightInnerCos, spotlightOuterCos, spotlightRange, 0.0f);
        XMStoreFloat4(&frameData.directionalLightDirection, directionalLightDirection);
        frameData.directionalLightColor = XMFLOAT4(
            directionalColor.x,
            directionalColor.y,
            directionalColor.z,
            directionalIntensity);
        frameData.ambientColor = XMFLOAT4(0.15f, 0.15f, 0.18f, 1.0f);
        frameData.lightingParams = XMFLOAT4(32.0f, 0.0035f, static_cast<float>(shadowDebugMode), 0.0f);
        frameData.shadowParams = XMFLOAT4(
            1.0f / static_cast<float>(kShadowMapSize),
            runtimePcssSearchRadius,
            runtimePcssEnabled ? 1.0f : 0.0f,
            runtimePcssMaxFilterRadius);

        context->UpdateSubresource(frameBuffer.Get(), 0, nullptr, &frameData, 0, 0);

        ID3D11ShaderResourceView* nullSrvs[4] = {nullptr, nullptr, nullptr, nullptr};
        context->PSSetShaderResources(0, 4, nullSrvs);

        context->IASetInputLayout(inputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(shadowVertexShader.Get(), nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);
        context->VSSetConstantBuffers(1, 1, objectBuffer.GetAddressOf());
        context->RSSetState(shadowRasterizerState.Get());
        context->RSSetViewports(1, &shadowViewport);

        for (size_t lightIndex = 0; lightIndex < 3; ++lightIndex)
        {
            ShadowFrameBufferData shadowFrameData{};
            if (lightIndex == 0)
            {
                XMStoreFloat4x4(&shadowFrameData.lightViewProjection, XMMatrixTranspose(lightViewProjection0));
            }
            else if (lightIndex == 1)
            {
                XMStoreFloat4x4(&shadowFrameData.lightViewProjection, XMMatrixTranspose(lightViewProjection1));
            }
            else
            {
                XMStoreFloat4x4(&shadowFrameData.lightViewProjection, XMMatrixTranspose(lightViewProjection2));
            }

            context->UpdateSubresource(shadowFrameBuffer.Get(), 0, nullptr, &shadowFrameData, 0, 0);
            context->VSSetConstantBuffers(0, 1, shadowFrameBuffer.GetAddressOf());
            context->OMSetRenderTargets(0, nullptr, shadowDepthViews[lightIndex].Get());
            context->ClearDepthStencilView(shadowDepthViews[lightIndex].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

            for (const RenderItem& item : renderItems)
            {
                if (!item.gameObject->CastsShadow())
                {
                    continue;
                }

                if (item.gameObject->GetMeshType() == MeshType::Model)
                {
                    const ModelResource* model = getModelById(item.gameObject->GetModelId());
                    if (model == nullptr)
                    {
                        continue;
                    }

                    for (const ModelMesh& mesh : model->meshes)
                    {
                        ObjectBufferData objectData{};
                        XMStoreFloat4x4(&objectData.world, XMMatrixTranspose(item.world));
                        XMStoreFloat4x4(&objectData.worldInverseTranspose, item.worldInverseTranspose);
                        objectData.materialColor = mesh.materialColor;
                        objectData.materialParams = XMFLOAT4(mesh.hasTexture ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

                        context->UpdateSubresource(objectBuffer.Get(), 0, nullptr, &objectData, 0, 0);
                        context->VSSetConstantBuffers(1, 1, objectBuffer.GetAddressOf());

                        ID3D11Buffer* vertexBuffer = mesh.vertexBuffer.Get();
                        context->IASetVertexBuffers(0, 1, &vertexBuffer, &mesh.stride, &mesh.offset);
                        context->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                        context->DrawIndexed(mesh.indexCount, 0, 0);
                    }
                    continue;
                }

                const MeshRenderData* mesh = getMeshRenderData(item.gameObject->GetMeshType());
                if (mesh == nullptr)
                {
                    continue;
                }

                ObjectBufferData objectData{};
                XMStoreFloat4x4(&objectData.world, XMMatrixTranspose(item.world));
                XMStoreFloat4x4(&objectData.worldInverseTranspose, item.worldInverseTranspose);
                objectData.materialColor = item.gameObject->GetMaterialColor();
                objectData.materialParams = XMFLOAT4(item.gameObject->UsesTexture() ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

                context->UpdateSubresource(objectBuffer.Get(), 0, nullptr, &objectData, 0, 0);
                context->VSSetConstantBuffers(1, 1, objectBuffer.GetAddressOf());

                ID3D11Buffer* vertexBuffer = mesh->vertexBuffer;
                context->IASetVertexBuffers(0, 1, &vertexBuffer, &mesh->stride, &mesh->offset);
                context->IASetIndexBuffer(mesh->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
                context->DrawIndexed(mesh->indexCount, 0, 0);
            }
        }

        const bool activeMsaa = msaaSupported && runtimeMsaaEnabled;
        ID3D11RenderTargetView* activeRenderTargetView = activeMsaa ? gameViewMsaaRenderTargetView.Get() : gameViewRenderTargetView.Get();
        ID3D11DepthStencilView* activeDepthStencilView = activeMsaa ? msaaDepthStencilView.Get() : depthStencilView.Get();

        const float clearColor[] = {0.06f, 0.10f, 0.16f, 1.0f};
        context->RSSetState(nullptr);
        context->OMSetRenderTargets(1, &activeRenderTargetView, activeDepthStencilView);
        context->RSSetViewports(1, &sceneViewport);
        context->ClearRenderTargetView(activeRenderTargetView, clearColor);
        context->ClearDepthStencilView(activeDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

        context->IASetInputLayout(inputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(sceneVertexShader.Get(), nullptr, 0);
        context->PSSetShader(scenePixelShader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, frameBuffer.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, frameBuffer.GetAddressOf());

        ID3D11ShaderResourceView* sceneSrvs[] = {
            faceTexture.Get(),
            shadowShaderViews[0].Get(),
            shadowShaderViews[1].Get(),
            shadowShaderViews[2].Get()};
        context->PSSetShaderResources(0, 4, sceneSrvs);

        ID3D11SamplerState* samplers[] = {textureSampler.Get(), shadowSampler.Get(), shadowDepthSampler.Get()};
        context->PSSetSamplers(0, 3, samplers);

        for (const RenderItem& item : renderItems)
        {
            if (item.gameObject->GetMeshType() == MeshType::Model)
            {
                const ModelResource* model = getModelById(item.gameObject->GetModelId());
                if (model == nullptr)
                {
                    continue;
                }

                for (const ModelMesh& mesh : model->meshes)
                {
                    ObjectBufferData objectData{};
                    XMStoreFloat4x4(&objectData.world, XMMatrixTranspose(item.world));
                    XMStoreFloat4x4(&objectData.worldInverseTranspose, item.worldInverseTranspose);
                    objectData.materialColor = mesh.materialColor;
                    objectData.materialParams = XMFLOAT4(mesh.hasTexture ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

                    ID3D11ShaderResourceView* drawSrvs[] = {
                        mesh.hasTexture && mesh.texture ? mesh.texture.Get() : faceTexture.Get(),
                        shadowShaderViews[0].Get(),
                        shadowShaderViews[1].Get(),
                        shadowShaderViews[2].Get()};
                    context->PSSetShaderResources(0, 4, drawSrvs);

                    context->UpdateSubresource(objectBuffer.Get(), 0, nullptr, &objectData, 0, 0);
                    context->VSSetConstantBuffers(1, 1, objectBuffer.GetAddressOf());
                    context->PSSetConstantBuffers(1, 1, objectBuffer.GetAddressOf());

                    ID3D11Buffer* vertexBuffer = mesh.vertexBuffer.Get();
                    context->IASetVertexBuffers(0, 1, &vertexBuffer, &mesh.stride, &mesh.offset);
                    context->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                    context->DrawIndexed(mesh.indexCount, 0, 0);
                }
                continue;
            }

            const MeshRenderData* mesh = getMeshRenderData(item.gameObject->GetMeshType());
            if (mesh == nullptr)
            {
                continue;
            }

            ObjectBufferData objectData{};
            XMStoreFloat4x4(&objectData.world, XMMatrixTranspose(item.world));
            XMStoreFloat4x4(&objectData.worldInverseTranspose, item.worldInverseTranspose);
            objectData.materialColor = item.gameObject->GetMaterialColor();
            objectData.materialParams = XMFLOAT4(item.gameObject->UsesTexture() ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

            context->UpdateSubresource(objectBuffer.Get(), 0, nullptr, &objectData, 0, 0);
            context->VSSetConstantBuffers(1, 1, objectBuffer.GetAddressOf());
            context->PSSetConstantBuffers(1, 1, objectBuffer.GetAddressOf());

            ID3D11Buffer* vertexBuffer = mesh->vertexBuffer;
            context->IASetVertexBuffers(0, 1, &vertexBuffer, &mesh->stride, &mesh->offset);
            context->IASetIndexBuffer(mesh->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
            context->DrawIndexed(mesh->indexCount, 0, 0);
        }

        XMMATRIX skyboxView = view;
        skyboxView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        const XMMATRIX skyboxViewProjection = skyboxView * projection;

        SkyboxFrameBufferData skyboxFrameData{};
        XMStoreFloat4x4(&skyboxFrameData.viewProjection, XMMatrixTranspose(skyboxViewProjection));
        context->UpdateSubresource(skyboxFrameBuffer.Get(), 0, nullptr, &skyboxFrameData, 0, 0);

        context->RSSetState(skyboxRasterizerState.Get());
        context->OMSetDepthStencilState(skyboxDepthStencilState.Get(), 0);
        context->IASetInputLayout(skyboxInputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(skyboxVertexShader.Get(), nullptr, 0);
        context->PSSetShader(skyboxPixelShader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, skyboxFrameBuffer.GetAddressOf());
        context->PSSetConstantBuffers(0, 0, nullptr);
        context->PSSetShaderResources(0, 1, skyboxCubemap.GetAddressOf());
        context->PSSetSamplers(0, 1, skyboxSampler.GetAddressOf());

        const UINT skyboxStride = sizeof(float) * 3;
        const UINT skyboxOffset = 0;
        ID3D11Buffer* skyboxVertexBuffers[] = {skyboxVertexBuffer.Get()};
        context->IASetVertexBuffers(0, 1, skyboxVertexBuffers, &skyboxStride, &skyboxOffset);
        context->IASetIndexBuffer(skyboxIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->DrawIndexed(skyboxIndexCount, 0, 0);

        context->OMSetDepthStencilState(nullptr, 0);
        context->RSSetState(nullptr);
        ID3D11ShaderResourceView* nullSkyboxSrv[1] = {nullptr};
        context->PSSetShaderResources(0, 1, nullSkyboxSrv);

        if (shadowDebugMode > 0)
        {
            DebugBufferData debugData{};
            debugData.params = XMFLOAT4(static_cast<float>(shadowDebugMode - 1), 0.0f, 0.0f, 0.0f);
            context->UpdateSubresource(debugBuffer.Get(), 0, nullptr, &debugData, 0, 0);

            context->OMSetRenderTargets(1, &activeRenderTargetView, nullptr);
            context->RSSetViewports(1, &debugViewport);
            context->IASetInputLayout(nullptr);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            context->VSSetShader(debugVertexShader.Get(), nullptr, 0);
            context->PSSetShader(debugPixelShader.Get(), nullptr, 0);
            context->VSSetConstantBuffers(0, 0, nullptr);
            context->PSSetConstantBuffers(0, 1, debugBuffer.GetAddressOf());

            ID3D11ShaderResourceView* debugSrvs[] = {
                shadowShaderViews[0].Get(),
                shadowShaderViews[1].Get(),
                shadowShaderViews[2].Get()};
            context->PSSetShaderResources(0, 3, debugSrvs);

            ID3D11SamplerState* debugSamplers[] = {debugSampler.Get()};
            context->PSSetSamplers(0, 1, debugSamplers);
            context->Draw(4, 0);

            ID3D11ShaderResourceView* nullDebugSrvs[3] = {nullptr, nullptr, nullptr};
            context->PSSetShaderResources(0, 3, nullDebugSrvs);
            context->RSSetViewports(1, &sceneViewport);
        }

        if (activeMsaa)
        {
            context->ResolveSubresource(gameViewColorBuffer.Get(), 0, gameViewMsaaColorBuffer.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        }

        const float uiClearColor[] = {0.10f, 0.10f, 0.12f, 1.0f};
        ID3D11RenderTargetView* backBufferRenderTarget = renderTargetView.Get();
        context->OMSetRenderTargets(1, &backBufferRenderTarget, nullptr);
        context->RSSetViewports(1, &sceneViewport);
        context->ClearRenderTargetView(backBufferRenderTarget, uiClearColor);

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (shouldUninitializeCom)
    {
        CoUninitialize();
    }

    return static_cast<int>(message.wParam);
}

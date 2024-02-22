//--------------------------------------------------------------------------------------
// File: Model.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#pragma once

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#include <dxgiformat.h>
#endif

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <vector>

#include <malloc.h>

#include <wrl\client.h>

#include <DirectXMath.h>
#include <DirectXCollision.h>


namespace DirectX
{
    inline namespace DX11
    {
        class IEffect;
        class IEffectFactory;
        class CommonStates;
        class ModelMesh;

        //------------------------------------------------------------------------------
        // Model loading options
        enum ModelLoaderFlags : uint32_t
        {
            ModelLoader_Clockwise = 0x0,
            ModelLoader_CounterClockwise = 0x1,
            ModelLoader_PremultipledAlpha = 0x2,
            ModelLoader_MaterialColorsSRGB = 0x4,
            ModelLoader_AllowLargeModels = 0x8,
            ModelLoader_IncludeBones = 0x10,
            ModelLoader_DisableSkinning = 0x20,
        };

        //------------------------------------------------------------------------------
        // Frame hierarchy for rigid body and skeletal animation
        struct ModelBone
        {
            ModelBone() noexcept :
                parentIndex(c_Invalid),
                childIndex(c_Invalid),
                siblingIndex(c_Invalid)
            {
            }

            ModelBone(uint32_t parent, uint32_t child, uint32_t sibling) noexcept :
                parentIndex(parent),
                childIndex(child),
                siblingIndex(sibling)
            {
            }

            uint32_t            parentIndex;
            uint32_t            childIndex;
            uint32_t            siblingIndex;
            std::wstring        name;

            using Collection = std::vector<ModelBone>;

            static constexpr uint32_t c_Invalid = uint32_t(-1);

            struct aligned_deleter { void operator()(void* p) noexcept { _aligned_free(p); } };

            using TransformArray = std::unique_ptr<XMMATRIX[], aligned_deleter>;

            static TransformArray MakeArray(size_t count)
            {
                void* temp = _aligned_malloc(sizeof(XMMATRIX) * count, 16);
                if (!temp)
                    throw std::bad_alloc();
                return TransformArray(static_cast<XMMATRIX*>(temp));
            }
        };


        //------------------------------------------------------------------------------
        // Each mesh part is a submesh with a single effect
        class ModelMeshPart
        {
        public:
            ModelMeshPart() noexcept;

            ModelMeshPart(ModelMeshPart&&) = default;
            ModelMeshPart& operator= (ModelMeshPart&&) = default;

            ModelMeshPart(ModelMeshPart const&) = default;
            ModelMeshPart& operator= (ModelMeshPart const&) = default;

            virtual ~ModelMeshPart();

            using Collection = std::vector<std::unique_ptr<ModelMeshPart>>;
            using InputLayoutCollection = std::vector<D3D11_INPUT_ELEMENT_DESC>;

            uint32_t                                                indexCount;
            uint32_t                                                startIndex;
            int32_t                                                 vertexOffset;
            uint32_t                                                vertexStride;
            D3D_PRIMITIVE_TOPOLOGY                                  primitiveType;
            DXGI_FORMAT                                             indexFormat;
            Microsoft::WRL::ComPtr<ID3D11InputLayout>               inputLayout;
            Microsoft::WRL::ComPtr<ID3D11Buffer>                    indexBuffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer>                    vertexBuffer;
            std::shared_ptr<IEffect>                                effect;
            std::shared_ptr<InputLayoutCollection>                  vbDecl;
            bool                                                    isAlpha;

            // Draw mesh part with custom effect
            void __cdecl Draw(
                _In_ ID3D11DeviceContext* deviceContext,
                _In_ IEffect* ieffect,
                _In_ ID3D11InputLayout* iinputLayout,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            void __cdecl DrawInstanced(
                _In_ ID3D11DeviceContext* deviceContext,
                _In_ IEffect* ieffect,
                _In_ ID3D11InputLayout* iinputLayout,
                uint32_t instanceCount,
                uint32_t startInstanceLocation = 0,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

           // Create input layout for drawing with a custom effect.
            void __cdecl CreateInputLayout(_In_ ID3D11Device* device, _In_ IEffect* ieffect, _Outptr_ ID3D11InputLayout** iinputLayout) const;

            // Change effect used by part and regenerate input layout (be sure to call Model::Modified as well)
            void __cdecl ModifyEffect(_In_ ID3D11Device* device, _In_ const std::shared_ptr<IEffect>& ieffect, bool isalpha = false);
        };


        //------------------------------------------------------------------------------
        // A mesh consists of one or more model mesh parts
        class ModelMesh
        {
        public:
            ModelMesh() noexcept;

            ModelMesh(ModelMesh&&) = default;
            ModelMesh& operator= (ModelMesh&&) = default;

            ModelMesh(ModelMesh const&) = default;
            ModelMesh& operator= (ModelMesh const&) = default;

            virtual ~ModelMesh();

            BoundingSphere              boundingSphere;
            BoundingBox                 boundingBox;
            ModelMeshPart::Collection   meshParts;
            uint32_t                    boneIndex;
            std::vector<uint32_t>       boneInfluences;
            std::wstring                name;
            bool                        ccw;
            bool                        pmalpha;

            using Collection = std::vector<std::shared_ptr<ModelMesh>>;

            // Setup states for drawing mesh
            void __cdecl PrepareForRendering(
                _In_ ID3D11DeviceContext* deviceContext,
                const CommonStates& states,
                bool alpha = false,
                bool wireframe = false) const;

            // Draw the mesh
            void XM_CALLCONV Draw(
                _In_ ID3D11DeviceContext* deviceContext,
                FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
                bool alpha = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            // Draw the mesh using model bones
            void XM_CALLCONV Draw(
                _In_ ID3D11DeviceContext* deviceContext,
                size_t nbones, _In_reads_(nbones) const XMMATRIX* boneTransforms,
                FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
                bool alpha = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            // Draw the mesh using skinning
            void XM_CALLCONV DrawSkinned(
                _In_ ID3D11DeviceContext* deviceContext,
                size_t nbones, _In_reads_(nbones) const XMMATRIX* boneTransforms,
                FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
                bool alpha = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            static void SetDepthBufferMode(bool reverseZ)
            {
                s_reversez = reverseZ;
            }

        private:
            static bool s_reversez;
        };


        //------------------------------------------------------------------------------
        // A model consists of one or more meshes
        class Model
        {
        public:
            Model() = default;

            Model(Model&&) = default;
            Model& operator= (Model&&) = default;

            Model(Model const& other);
            Model& operator= (Model const& rhs);

            virtual ~Model();

            ModelMesh::Collection       meshes;
            ModelBone::Collection       bones;
            ModelBone::TransformArray   boneMatrices;
            ModelBone::TransformArray   invBindPoseMatrices;
            std::wstring                name;

            // Draw all the meshes in the model
            void XM_CALLCONV Draw(
                _In_ ID3D11DeviceContext* deviceContext,
                const CommonStates& states,
                FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
                bool wireframe = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            // Draw all the meshes using model bones
            void XM_CALLCONV Draw(
                _In_ ID3D11DeviceContext* deviceContext,
                const CommonStates& states,
                size_t nbones, _In_reads_(nbones) const XMMATRIX* boneTransforms,
                FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
                bool wireframe = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            // Draw all the meshes using skinning
            void XM_CALLCONV DrawSkinned(
                _In_ ID3D11DeviceContext* deviceContext,
                const CommonStates& states,
                size_t nbones, _In_reads_(nbones) const XMMATRIX* boneTransforms,
                FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection,
                bool wireframe = false,
                _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) const;

            // Compute bone positions based on heirarchy and transform matrices
            void __cdecl CopyAbsoluteBoneTransformsTo(
                size_t nbones,
                _Out_writes_(nbones) XMMATRIX* boneTransforms) const;

            void __cdecl CopyAbsoluteBoneTransforms(
                size_t nbones,
                _In_reads_(nbones) const XMMATRIX* inBoneTransforms,
                _Out_writes_(nbones) XMMATRIX* outBoneTransforms) const;

            // Set bone matrices to a set of relative tansforms
            void __cdecl CopyBoneTransformsFrom(
                size_t nbones,
                _In_reads_(nbones) const XMMATRIX* boneTransforms);

            // Copies the relative bone matrices to a transform array
            void __cdecl CopyBoneTransformsTo(
                size_t nbones,
                _Out_writes_(nbones) XMMATRIX* boneTransforms) const;

            // Notify model that effects, parts list, or mesh list has changed
            void __cdecl Modified() noexcept { mEffectCache.clear(); }

            // Update all effects used by the model
            void __cdecl UpdateEffects(_In_ std::function<void __cdecl(IEffect*)> setEffect);

            // Loads a model from a Visual Studio Starter Kit .CMO file
            static std::unique_ptr<Model> __cdecl CreateFromCMO(
                _In_ ID3D11Device* device,
                _In_reads_bytes_(dataSize) const uint8_t* meshData, size_t dataSize,
                _In_ IEffectFactory& fxFactory,
                ModelLoaderFlags flags = ModelLoader_CounterClockwise,
                _Out_opt_ size_t* animsOffset = nullptr);
            static std::unique_ptr<Model> __cdecl CreateFromCMO(
                _In_ ID3D11Device* device,
                _In_z_ const wchar_t* szFileName,
                _In_ IEffectFactory& fxFactory,
                ModelLoaderFlags flags = ModelLoader_CounterClockwise,
                _Out_opt_ size_t* animsOffset = nullptr);

            // Loads a model from a DirectX SDK .SDKMESH file
            static std::unique_ptr<Model> __cdecl CreateFromSDKMESH(
                _In_ ID3D11Device* device,
                _In_reads_bytes_(dataSize) const uint8_t* meshData, _In_ size_t dataSize,
                _In_ IEffectFactory& fxFactory,
                ModelLoaderFlags flags = ModelLoader_Clockwise);
            static std::unique_ptr<Model> __cdecl CreateFromSDKMESH(
                _In_ ID3D11Device* device,
                _In_z_ const wchar_t* szFileName,
                _In_ IEffectFactory& fxFactory,
                ModelLoaderFlags flags = ModelLoader_Clockwise);

            // Loads a model from a .VBO file
            static std::unique_ptr<Model> __cdecl CreateFromVBO(
                _In_ ID3D11Device* device,
                _In_reads_bytes_(dataSize) const uint8_t* meshData, _In_ size_t dataSize,
                _In_opt_ std::shared_ptr<IEffect> ieffect = nullptr,
                ModelLoaderFlags flags = ModelLoader_Clockwise);
            static std::unique_ptr<Model> __cdecl CreateFromVBO(
                _In_ ID3D11Device* device,
                _In_z_ const wchar_t* szFileName,
                _In_opt_ std::shared_ptr<IEffect> ieffect = nullptr,
                ModelLoaderFlags flags = ModelLoader_Clockwise);

        private:
            std::set<IEffect*>  mEffectCache;

            void __cdecl ComputeAbsolute(uint32_t index,
                CXMMATRIX local, size_t nbones,
                _In_reads_(nbones) const XMMATRIX* inBoneTransforms,
                _Inout_updates_(nbones) XMMATRIX* outBoneTransforms,
                size_t& visited) const;
        };

    #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
    #endif

        DEFINE_ENUM_FLAG_OPERATORS(ModelLoaderFlags);

    #ifdef __clang__
    #pragma clang diagnostic pop
    #endif
    }
}

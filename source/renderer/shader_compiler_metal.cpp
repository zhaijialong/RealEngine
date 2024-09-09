#include "shader_compiler.h"
#include "gfx/gfx_defines.h"
#include "gfx/metal/metal_shader_reflection.h"
#include "utils/log.h"
#include "magic_enum/magic_enum.hpp"
#include "metal_irconverter/metal_irconverter.h"

inline IRShaderStage ToIRShaderStage(GfxShaderType type)
{
    switch (type)
    {
        case GfxShaderType::AS:
            return IRShaderStageAmplification;
        case GfxShaderType::MS:
            return IRShaderStageMesh;
        case GfxShaderType::VS:
            return IRShaderStageVertex;
        case GfxShaderType::PS:
            return IRShaderStageFragment;
        case GfxShaderType::CS:
            return IRShaderStageCompute;
        default:
            return IRShaderStageInvalid;
    }
}

inline void GetReflectionInfo(IRShaderReflection* pShaderReflection, IRShaderStage stage, MetalShaderReflection& out)
{
    if(stage == IRShaderStageCompute)
    {
        IRVersionedCSInfo csInfo;
        if(IRShaderReflectionCopyComputeInfo(pShaderReflection, IRReflectionVersion_1_0, &csInfo))
        {
            out.threadsPerThreadgroup[0] = csInfo.info_1_0.tg_size[0];
            out.threadsPerThreadgroup[1] = csInfo.info_1_0.tg_size[1];
            out.threadsPerThreadgroup[2] = csInfo.info_1_0.tg_size[2];
            
            IRShaderReflectionReleaseComputeInfo(&csInfo);
        }
    }
    else if(stage == IRShaderStageAmplification)
    {
        IRVersionedASInfo asInfo;
        if(IRShaderReflectionCopyAmplificationInfo(pShaderReflection, IRReflectionVersion_1_0, &asInfo))
        {
            out.threadsPerThreadgroup[0] = asInfo.info_1_0.num_threads[0];
            out.threadsPerThreadgroup[1] = asInfo.info_1_0.num_threads[1];
            out.threadsPerThreadgroup[2] = asInfo.info_1_0.num_threads[2];
            
            IRShaderReflectionReleaseAmplificationInfo(&asInfo);
        }
    }
    else if(stage == IRShaderStageMesh)
    {
        IRVersionedMSInfo msInfo;
        if(IRShaderReflectionCopyMeshInfo(pShaderReflection, IRReflectionVersion_1_0, &msInfo))
        {
            out.threadsPerThreadgroup[0] = msInfo.info_1_0.num_threads[0];
            out.threadsPerThreadgroup[1] = msInfo.info_1_0.num_threads[1];
            out.threadsPerThreadgroup[2] = msInfo.info_1_0.num_threads[2];
            
            IRShaderReflectionReleaseMeshInfo(&msInfo);
        }
    }
}

void ShaderCompiler::CreateMetalCompiler()
{
    m_pMetalCompiler = IRCompilerCreate();
    
    //D3D12Device::CreateRootSignature
    IRRootParameter1 rootParameters[3] = {};
    
    rootParameters[0].ParameterType = IRRootParameterType32BitConstants;
    rootParameters[0].ShaderVisibility = IRShaderVisibilityAll;
    rootParameters[0].Constants.Num32BitValues = GFX_MAX_ROOT_CONSTANTS;
    rootParameters[0].Constants.ShaderRegister = 0;
    
    rootParameters[1].ParameterType = IRRootParameterTypeCBV;
    rootParameters[1].ShaderVisibility = IRShaderVisibilityAll;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    
    rootParameters[2].ParameterType = IRRootParameterTypeCBV;
    rootParameters[2].ShaderVisibility = IRShaderVisibilityAll;
    rootParameters[2].Descriptor.ShaderRegister = 2;
    
    IRVersionedRootSignatureDescriptor desc = {};
    desc.version = IRRootSignatureVersion_1_1;
    desc.desc_1_1.NumParameters = 3;
    desc.desc_1_1.pParameters = rootParameters;
    desc.desc_1_1.Flags = IRRootSignatureFlags(
        IRRootSignatureFlagDenyHullShaderRootAccess |
        IRRootSignatureFlagDenyDomainShaderRootAccess |
        IRRootSignatureFlagDenyGeometryShaderRootAccess |
        IRRootSignatureFlagCBVSRVUAVHeapDirectlyIndexed |
        IRRootSignatureFlagSamplerHeapDirectlyIndexed);
    
    IRError* error = nullptr;
    m_pMetalRootSignature = IRRootSignatureCreateFromDescriptor(&desc, &error);
    
    if(!m_pMetalRootSignature)
    {
        IRErrorCode errorCode = (IRErrorCode)IRErrorGetCode(error);
        RE_ERROR("[ShaderCompiler::CreateMetalCompiler] failed to create the root signature with error : {}", magic_enum::enum_name(errorCode));
        IRErrorDestroy(error);
    }
}

void ShaderCompiler::DestroyMetalCompiler()
{
    IRRootSignatureDestroy(m_pMetalRootSignature);
    IRCompilerDestroy(m_pMetalCompiler);
}

bool ShaderCompiler::CompileMetalIR(const eastl::string& file, const eastl::string& entry_point, GfxShaderType type, const void* data, uint32_t data_size, eastl::vector<uint8_t>& output_blob)
{
    IRCompilerSetGlobalRootSignature(m_pMetalCompiler, m_pMetalRootSignature);
    IRCompilerSetMinimumGPUFamily(m_pMetalCompiler, IRGPUFamilyApple7);
    IRCompilerSetMinimumDeploymentTarget(m_pMetalCompiler, IROperatingSystem_macOS, "15.0.0"); // mac os 15, metal 3.2
    IRCompilerSetEntryPointName(m_pMetalCompiler, entry_point.c_str());
    
    IRObject* pDXIL = IRObjectCreateFromDXIL((const uint8_t*)data, (size_t)data_size, IRBytecodeOwnershipNone);
    
    IRError* pError = nullptr;
    IRObject* pOutIR = IRCompilerAllocCompileAndLink(m_pMetalCompiler, nullptr, pDXIL, &pError);

    if (!pOutIR)
    {
        IRErrorCode errorCode = (IRErrorCode)IRErrorGetCode(pError);
        RE_ERROR("[ShaderCompiler::CompileMetalIR] failed to compile metal ir : {}, {}, {}", file, entry_point, magic_enum::enum_name(errorCode));
        
        IRErrorDestroy( pError );
        IRObjectDestroy(pDXIL);
        return false;
    }
    
    IRShaderReflection* pShaderReflection = IRShaderReflectionCreate();
    IRObjectGetReflection(pOutIR, ToIRShaderStage(type), pShaderReflection);
    
    IRMetalLibBinary* pMetallib = IRMetalLibBinaryCreate();
    IRObjectGetMetalLibBinary(pOutIR, ToIRShaderStage(type), pMetallib);
    
    MetalShaderReflection reflection = {};
    GetReflectionInfo(pShaderReflection, ToIRShaderStage(type), reflection);
    
    output_blob.resize(sizeof(MetalShaderReflection) + IRMetalLibGetBytecodeSize(pMetallib));
    
    memcpy(output_blob.data(), &reflection, sizeof(MetalShaderReflection));
    IRMetalLibGetBytecode(pMetallib, output_blob.data() + sizeof(MetalShaderReflection));
    
    IRShaderReflectionDestroy(pShaderReflection);
    IRMetalLibBinaryDestroy(pMetallib);
    IRObjectDestroy(pDXIL);
    IRObjectDestroy(pOutIR);
    return true;
}

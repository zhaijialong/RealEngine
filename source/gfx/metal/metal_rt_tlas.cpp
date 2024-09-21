#include "metal_rt_tlas.h"
#include "metal_device.h"
#include "metal_rt_blas.h"
#include "utils/log.h"

MetalRayTracingTLAS::MetalRayTracingTLAS(MetalDevice* pDevice, const GfxRayTracingTLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalRayTracingTLAS::~MetalRayTracingTLAS()
{
    m_pAccelerationStructure->release();
    m_pDescriptor->release();
    m_pScratchBuffer->release();
    m_pInstanceBuffer->release();
    m_pGPUHeaderBuffer->release();
}

bool MetalRayTracingTLAS::Create()
{
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    
    m_instanceBufferSize = sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor) * m_desc.instance_count * GFX_MAX_INFLIGHT_FRAMES;
    m_pInstanceBuffer = device->newBuffer(m_instanceBufferSize, MTL::ResourceStorageModeShared);
    m_instanceBufferCpuAddress = m_pInstanceBuffer->contents();
    
    m_pDescriptor = MTL::InstanceAccelerationStructureDescriptor::alloc()->init();
    m_pDescriptor->setUsage(ToAccelerationStructureUsage(m_desc.flags));
    m_pDescriptor->setInstanceCount((NS::UInteger)m_desc.instance_count);
    m_pDescriptor->setInstanceDescriptorBuffer(m_pInstanceBuffer);
    m_pDescriptor->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeUserID);
    m_pDescriptor->setInstanceTransformationMatrixLayout(MTL::MatrixLayoutRowMajor);
    
    MTL::AccelerationStructureSizes asSizes = device->accelerationStructureSizes(m_pDescriptor);
    
    m_pAccelerationStructure = device->newAccelerationStructure(asSizes.accelerationStructureSize);
    m_pScratchBuffer = device->newBuffer(asSizes.buildScratchBufferSize, MTL::ResourceStorageModePrivate);
    
    if(m_pAccelerationStructure == nullptr || m_pScratchBuffer == nullptr)
    {
        RE_ERROR("[MetalRayTracingTLAS] failed to create : {}", m_name);
        return false;
    }
    
    ((MetalDevice*)m_pDevice)->MakeResident(m_pAccelerationStructure);
    ((MetalDevice*)m_pDevice)->MakeResident(m_pScratchBuffer);
    ((MetalDevice*)m_pDevice)->MakeResident(m_pInstanceBuffer);
    
    NS::String* label = NS::String::alloc()->init(m_name.c_str(), NS::StringEncoding::UTF8StringEncoding);
    m_pAccelerationStructure->setLabel(label);
    label->release();

    
    NS::UInteger headerSize = sizeof(IRRaytracingAccelerationStructureGPUHeader) + sizeof(uint32_t) * m_desc.instance_count;
    m_pGPUHeaderBuffer = device->newBuffer(headerSize, MTL::ResourceStorageModeShared);
    ((MetalDevice*)m_pDevice)->MakeResident(m_pGPUHeaderBuffer);
    
    eastl::vector<uint32_t> instanceContributions;
    instanceContributions.resize(m_desc.instance_count, 0);
    
    IRRaytracingSetAccelerationStructure((uint8_t *)m_pGPUHeaderBuffer->contents(),
        m_pAccelerationStructure->gpuResourceID(),
        (uint8_t *)m_pGPUHeaderBuffer->contents() + sizeof(IRRaytracingAccelerationStructureGPUHeader),
        instanceContributions.data(),
        m_desc.instance_count);
    
    return true;
}

void MetalRayTracingTLAS::UpdateInstance(const GfxRayTracingInstance* instances, uint32_t instance_count)
{
    RE_ASSERT(instance_count <= m_desc.instance_count);
    
    if (m_currentInstanceBufferOffset + sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor) * instance_count > m_instanceBufferSize)
    {
        m_currentInstanceBufferOffset = 0;
    }
    
    eastl::vector<MTL::AccelerationStructure*> accelerationStructures;
    accelerationStructures.reserve(instance_count);
    
    MTL::AccelerationStructureUserIDInstanceDescriptor* instanceDescriptors = (MTL::AccelerationStructureUserIDInstanceDescriptor*)((char*)m_instanceBufferCpuAddress + m_currentInstanceBufferOffset);
    
    for(uint32_t i = 0; i < instance_count; ++i)
    {
        const GfxRayTracingInstance& instance = instances[i];
        accelerationStructures.push_back((MTL::AccelerationStructure*)instance.blas->GetHandle());
     
        instanceDescriptors[i].accelerationStructureIndex = i;
        instanceDescriptors[i].options = ToAccelerationStructureInstanceOptions(instance.flags);
        instanceDescriptors[i].userID = instance.instance_id;
        instanceDescriptors[i].mask = instance.instance_mask;
        instanceDescriptors[i].intersectionFunctionTableOffset = 0;
        memcpy(&instanceDescriptors[i].transformationMatrix, instance.transform, sizeof(float) * 12);
    }
    
    NS::Array* instancedAccelerationStructures = NS::Array::alloc()->init((NS::Object**)accelerationStructures.data(), (NS::UInteger)accelerationStructures.size());
    m_pDescriptor->setInstancedAccelerationStructures(instancedAccelerationStructures);
    m_pDescriptor->setInstanceDescriptorBufferOffset((NS::UInteger)m_currentInstanceBufferOffset);
    
    m_currentInstanceBufferOffset += sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor) * instance_count;
    instancedAccelerationStructures->release();
}

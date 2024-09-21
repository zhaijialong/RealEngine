#include "metal_rt_blas.h"
#include "metal_device.h"
#include "metal_buffer.h"
#include "utils/log.h"

MetalRayTracingBLAS::MetalRayTracingBLAS(MetalDevice* pDevice, const GfxRayTracingBLASDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalRayTracingBLAS::~MetalRayTracingBLAS()
{
    m_pAccelerationStructure->release();
    m_pDescriptor->release();
    m_pScratchBuffer->release();
    
    for(size_t i = 0; i < m_geometries.size(); ++i)
    {
        m_geometries[i]->release();
    }
}

bool MetalRayTracingBLAS::Create()
{
    m_geometries.reserve(m_desc.geometries.size());
    
    for (size_t i = 0; i < m_desc.geometries.size(); ++i)
    {
        const GfxRayTracingGeometry& geometry = m_desc.geometries[i];

        MTL::AccelerationStructureTriangleGeometryDescriptor* geometryDescriptor = MTL::AccelerationStructureTriangleGeometryDescriptor::alloc()->init();
        geometryDescriptor->setOpaque(geometry.opaque);
        geometryDescriptor->setVertexBuffer((MTL::Buffer*)geometry.vertex_buffer->GetHandle());
        geometryDescriptor->setVertexBufferOffset((NS::UInteger)geometry.vertex_buffer_offset);
        geometryDescriptor->setVertexStride((NS::UInteger)geometry.vertex_stride);
        geometryDescriptor->setVertexFormat(ToAttributeFormat(geometry.vertex_format));
        geometryDescriptor->setIndexBuffer((MTL::Buffer*)geometry.index_buffer->GetHandle());
        geometryDescriptor->setIndexBufferOffset((NS::UInteger)geometry.index_buffer_offset);
        geometryDescriptor->setIndexType(geometry.index_format == GfxFormat::R16UI ? MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32);
        geometryDescriptor->setTriangleCount((NS::UInteger)geometry.index_count / 3);
        
        m_geometries.push_back(geometryDescriptor);
    }
    
    NS::Array* geometryDescriptors = NS::Array::alloc()->init((NS::Object**)m_geometries.data(), (NS::UInteger)m_geometries.size());
    
    m_pDescriptor = MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
    m_pDescriptor->setGeometryDescriptors(geometryDescriptors);
    m_pDescriptor->setUsage(ToAccelerationStructureUsage(m_desc.flags));
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    MTL::AccelerationStructureSizes asSizes = device->accelerationStructureSizes(m_pDescriptor);
    
    m_pAccelerationStructure = device->newAccelerationStructure(asSizes.accelerationStructureSize);
    m_pScratchBuffer = device->newBuffer(eastl::max(asSizes.buildScratchBufferSize, asSizes.refitScratchBufferSize), MTL::ResourceStorageModePrivate);
    
    geometryDescriptors->release();
    
    if(m_pAccelerationStructure == nullptr || m_pScratchBuffer == nullptr)
    {
        RE_ERROR("[MetalRayTracingBLAS] failed to create : {}", m_name);
        return false;
    }
    
    ((MetalDevice*)m_pDevice)->MakeResident(m_pAccelerationStructure);
    ((MetalDevice*)m_pDevice)->MakeResident(m_pScratchBuffer);
    
    NS::String* label = NS::String::alloc()->init(m_name.c_str(), NS::StringEncoding::UTF8StringEncoding);
    m_pAccelerationStructure->setLabel(label);
    label->release();

    return true;
}


void MetalRayTracingBLAS::UpdateVertexBuffer(IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset)
{
    RE_ASSERT(m_desc.flags & GfxRayTracingASFlagAllowUpdate);
    RE_ASSERT(m_geometries.size() == 1); //todo : suppport more than 1
    
    m_geometries[0]->setVertexBuffer((MTL::Buffer*)vertex_buffer->GetHandle());
    m_geometries[0]->setVertexBufferOffset((NS::UInteger)vertex_buffer_offset);
    
    NS::Array* geometryDescriptors = NS::Array::alloc()->init((NS::Object**)m_geometries.data(), (NS::UInteger)m_geometries.size());
    m_pDescriptor->setGeometryDescriptors(geometryDescriptors);
    geometryDescriptors->release();
}

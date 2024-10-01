#include "metal_shader.h"
#include "metal_device.h"
#include "utils/log.h"
#include "xxHash/xxhash.h"

MetalShader::MetalShader(MetalDevice* pDevice, const GfxShaderDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalShader::~MetalShader()
{
    m_pFunction->release();
}

bool MetalShader::Create(eastl::span<uint8_t> data)
{
    m_pFunction->release(); // this looks dangerous, but sending message to nil is safe in objc runtime
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    
    memcpy(&m_reflection, data.data(), sizeof(MetalShaderReflection));
    dispatch_data_t metalIR = dispatch_data_create(data.data() + sizeof(MetalShaderReflection), data.size() - sizeof(MetalShaderReflection), dispatch_get_main_queue(), NULL);
    
    NS::Error* error;
    MTL::Library* library = device->newLibrary(metalIR, &error);
    CFRelease(metalIR);
    
    if(library == nullptr)
    {
        RE_ERROR("[MetalShader] failed to create {} \r\n{}", m_name, error->localizedDescription()->utf8String());

        error->release();
        return false;
    }
    
    NS::String* functionName = NS::String::alloc()->init(m_desc.entry_point.c_str(), NS::StringEncoding::UTF8StringEncoding);
    m_pFunction = library->newFunction(functionName);
    library->release();
    functionName->release();
    
    SetDebugLabel(m_pFunction, m_name.c_str());
    
    m_hash = XXH3_64bits(data.data(), data.size());

    return true;
}

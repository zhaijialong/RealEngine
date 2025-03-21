#pragma once

#include "../gfx_defines.h"
#include "utils/assert.h"
#include "Foundation/Foundation.hpp"
#include "Metal/Metal.hpp"
#include "MetalKit/MetalKit.hpp"

#define IR_RUNTIME_METALCPP
#include "metal_irconverter_runtime/metal_irconverter_runtime.h"

template<typename T>
inline void SetDebugLabel(T* resource, const char* name)
{
    NS::String* label = NS::String::alloc()->init(name, NS::StringEncoding::UTF8StringEncoding);
    resource->setLabel(label);
    label->release();
}

inline MTL::ResourceOptions ToResourceOptions(GfxMemoryType type)
{
    MTL::ResourceOptions options = MTL::ResourceHazardTrackingModeTracked;
    
    switch (type)
    {
        case GfxMemoryType::GpuOnly:
            options |= MTL::ResourceStorageModePrivate;
            break;
        case GfxMemoryType::CpuOnly:
        case GfxMemoryType::CpuToGpu:
            options |= MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
            break;
        case GfxMemoryType::GpuToCpu:
            options |= MTL::ResourceStorageModeShared;
            break;
        default:
            break;
    }
    
    return options;
}

inline MTL::TextureType ToTextureType(GfxTextureType type)
{
    switch (type) 
    {
        case GfxTextureType::Texture2D:
            return MTL::TextureType2D;
        case GfxTextureType::Texture2DArray:
            return MTL::TextureType2DArray;
        case GfxTextureType::Texture3D:
            return MTL::TextureType3D;
        case GfxTextureType::TextureCube:
            return MTL::TextureTypeCube;
        case GfxTextureType::TextureCubeArray:
            return MTL::TextureTypeCubeArray;
        default:
            return MTL::TextureType2D;
    }
}

inline MTL::PixelFormat ToPixelFormat(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::Unknown:
            return MTL::PixelFormatInvalid;
        case GfxFormat::RGBA32F:
            return MTL::PixelFormatRGBA32Float;
        case GfxFormat::RGBA32UI:
            return MTL::PixelFormatRGBA32Uint;
        case GfxFormat::RGBA32SI:
            return MTL::PixelFormatRGBA32Sint;
        case GfxFormat::RGBA16F:
            return MTL::PixelFormatRGBA16Float;
        case GfxFormat::RGBA16UI:
            return MTL::PixelFormatRGBA16Uint;
        case GfxFormat::RGBA16SI:
            return MTL::PixelFormatRGBA16Sint;
        case GfxFormat::RGBA16UNORM:
            return MTL::PixelFormatRGBA16Unorm;
        case GfxFormat::RGBA16SNORM:
            return MTL::PixelFormatRGBA16Snorm;
        case GfxFormat::RGBA8UI:
            return MTL::PixelFormatRGBA8Uint;
        case GfxFormat::RGBA8SI:
            return MTL::PixelFormatRGBA8Sint;
        case GfxFormat::RGBA8UNORM:
            return MTL::PixelFormatRGBA8Unorm;
        case GfxFormat::RGBA8SNORM:
            return MTL::PixelFormatRGBA8Snorm;
        case GfxFormat::RGBA8SRGB:
            return MTL::PixelFormatRGBA8Unorm_sRGB;
        case GfxFormat::BGRA8UNORM:
            return MTL::PixelFormatBGRA8Unorm;
        case GfxFormat::BGRA8SRGB:
            return MTL::PixelFormatBGRA8Unorm_sRGB;
        case GfxFormat::RGB10A2UI:
            return MTL::PixelFormatRGB10A2Uint;
        case GfxFormat::RGB10A2UNORM:
            return MTL::PixelFormatRGB10A2Unorm;
        case GfxFormat::RGB32F:
            return MTL::PixelFormatInvalid;
        case GfxFormat::RGB32UI:
            return MTL::PixelFormatInvalid;
        case GfxFormat::RGB32SI:
            return MTL::PixelFormatInvalid;
        case GfxFormat::R11G11B10F:
            return MTL::PixelFormatRG11B10Float;
        case GfxFormat::RGB9E5:
            return MTL::PixelFormatRGB9E5Float;
        case GfxFormat::RG32F:
            return MTL::PixelFormatRG32Float;
        case GfxFormat::RG32UI:
            return MTL::PixelFormatRG32Uint;
        case GfxFormat::RG32SI:
            return MTL::PixelFormatRG32Sint;
        case GfxFormat::RG16F:
            return MTL::PixelFormatRG16Float;
        case GfxFormat::RG16UI:
            return MTL::PixelFormatRG16Uint;
        case GfxFormat::RG16SI:
            return MTL::PixelFormatRG16Sint;
        case GfxFormat::RG16UNORM:
            return MTL::PixelFormatRG16Unorm;
        case GfxFormat::RG16SNORM:
            return MTL::PixelFormatRG16Snorm;
        case GfxFormat::RG8UI:
            return MTL::PixelFormatRG8Uint;
        case GfxFormat::RG8SI:
            return MTL::PixelFormatRG8Sint;
        case GfxFormat::RG8UNORM:
            return MTL::PixelFormatRG8Unorm;
        case GfxFormat::RG8SNORM:
            return MTL::PixelFormatRG8Snorm;
        case GfxFormat::R32F:
            return MTL::PixelFormatR32Float;
        case GfxFormat::R32UI:
            return MTL::PixelFormatR32Uint;
        case GfxFormat::R32SI:
            return MTL::PixelFormatR32Sint;
        case GfxFormat::R16F:
            return MTL::PixelFormatR16Float;
        case GfxFormat::R16UI:
            return MTL::PixelFormatR16Uint;
        case GfxFormat::R16SI:
            return MTL::PixelFormatR16Sint;
        case GfxFormat::R16UNORM:
            return MTL::PixelFormatR16Unorm;
        case GfxFormat::R16SNORM:
            return MTL::PixelFormatR16Snorm;
        case GfxFormat::R8UI:
            return MTL::PixelFormatR8Uint;
        case GfxFormat::R8SI:
            return MTL::PixelFormatR8Sint;
        case GfxFormat::R8UNORM:
            return MTL::PixelFormatR8Unorm;
        case GfxFormat::R8SNORM:
            return MTL::PixelFormatR8Snorm;
        case GfxFormat::D32F:
            return MTL::PixelFormatDepth32Float;
        case GfxFormat::D32FS8:
            return MTL::PixelFormatDepth32Float_Stencil8;
        case GfxFormat::D16:
            return MTL::PixelFormatDepth16Unorm;
        case GfxFormat::BC1UNORM:
            return MTL::PixelFormatBC1_RGBA;
        case GfxFormat::BC1SRGB:
            return MTL::PixelFormatBC1_RGBA_sRGB;
        case GfxFormat::BC2UNORM:
            return MTL::PixelFormatBC2_RGBA;
        case GfxFormat::BC2SRGB:
            return MTL::PixelFormatBC2_RGBA_sRGB;
        case GfxFormat::BC3UNORM:
            return MTL::PixelFormatBC3_RGBA;
        case GfxFormat::BC3SRGB:
            return MTL::PixelFormatBC3_RGBA_sRGB;
        case GfxFormat::BC4UNORM:
            return MTL::PixelFormatBC4_RUnorm;
        case GfxFormat::BC4SNORM:
            return MTL::PixelFormatBC4_RSnorm;
        case GfxFormat::BC5UNORM:
            return MTL::PixelFormatBC5_RGUnorm;
        case GfxFormat::BC5SNORM:
            return MTL::PixelFormatBC5_RGSnorm;
        case GfxFormat::BC6U16F:
            return MTL::PixelFormatBC6H_RGBUfloat;
        case GfxFormat::BC6S16F:
            return MTL::PixelFormatBC6H_RGBFloat;
        case GfxFormat::BC7UNORM:
            return MTL::PixelFormatBC7_RGBAUnorm;
        case GfxFormat::BC7SRGB:
            return MTL::PixelFormatBC7_RGBAUnorm_sRGB;
        default:
            return MTL::PixelFormatInvalid;
    }
}

inline MTL::TextureUsage ToTextureUsage(GfxTextureUsageFlags flags)
{
    MTL::TextureUsage usage = MTL::TextureUsageShaderRead | MTL::TextureUsagePixelFormatView;
    
    if(flags & (GfxTextureUsageRenderTarget | GfxTextureUsageDepthStencil))
    {
        usage |= MTL::TextureUsageRenderTarget;
    }
    
    if(flags & GfxTextureUsageUnorderedAccess)
    {
        usage |= MTL::TextureUsageShaderWrite; //todo TextureUsageShaderAtomic
    }
    
    return usage;
}

inline MTL::TextureDescriptor* ToTextureDescriptor(const GfxTextureDesc& desc)
{
    MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
    descriptor->setWidth(desc.width);
    descriptor->setHeight(desc.height);
    descriptor->setDepth(desc.depth);
    descriptor->setMipmapLevelCount(desc.mip_levels);
    if(desc.type == GfxTextureType::TextureCube || desc.type == GfxTextureType::TextureCubeArray)
    {
        RE_ASSERT(desc.array_size % 6 == 0);
        descriptor->setArrayLength(desc.array_size / 6);
    }
    else
    {
        descriptor->setArrayLength(desc.array_size);
    }
    descriptor->setTextureType(ToTextureType(desc.type));
    descriptor->setPixelFormat(ToPixelFormat(desc.format));
    descriptor->setResourceOptions(ToResourceOptions(desc.memory_type));
    descriptor->setUsage(ToTextureUsage(desc.usage));
    
    return descriptor;
}

inline MTL::LoadAction ToLoadAction(GfxRenderPassLoadOp loadOp)
{
    switch (loadOp)
    {
        case GfxRenderPassLoadOp::Load:
            return MTL::LoadActionLoad;
        case GfxRenderPassLoadOp::Clear:
            return MTL::LoadActionClear;
        case GfxRenderPassLoadOp::DontCare:
            return MTL::LoadActionDontCare;
        default:
            return MTL::LoadActionLoad;
    }
}

inline MTL::StoreAction ToStoreAction(GfxRenderPassStoreOp storeOp)
{
    switch (storeOp)
    {
        case GfxRenderPassStoreOp::Store:
            return MTL::StoreActionStore;
        case GfxRenderPassStoreOp::DontCare:
            return MTL::StoreActionDontCare;
        default:
            return MTL::StoreActionStore;
    }
}

inline MTL::PrimitiveTopologyClass ToTopologyClass(GfxPrimitiveType type)
{
    switch (type)
    {
        case GfxPrimitiveType::PointList:
            return MTL::PrimitiveTopologyClassPoint;
        case GfxPrimitiveType::LineList:
        case GfxPrimitiveType::LineStrip:
            return MTL::PrimitiveTopologyClassLine;
        case GfxPrimitiveType::TriangleList:
        case GfxPrimitiveType::TriangleTrip:
            return MTL::PrimitiveTopologyClassTriangle;
        default:
            return MTL::PrimitiveTopologyClassUnspecified;
    }
}

inline MTL::PrimitiveType ToPrimitiveType(GfxPrimitiveType type)
{
    switch (type)
    {
        case GfxPrimitiveType::PointList:
            return MTL::PrimitiveTypePoint;
        case GfxPrimitiveType::LineList:
            return MTL::PrimitiveTypeLine;
        case GfxPrimitiveType::LineStrip:
            return MTL::PrimitiveTypeLineStrip;
        case GfxPrimitiveType::TriangleList:
            return MTL::PrimitiveTypeTriangle;
        case GfxPrimitiveType::TriangleTrip:
            return MTL::PrimitiveTypeTriangleStrip;
        default:
            return MTL::PrimitiveTypeTriangle;
    }
}

inline MTL::BlendFactor ToBlendFactor(GfxBlendFactor blend_factor)
{
    switch (blend_factor)
    {
        case GfxBlendFactor::Zero:
            return MTL::BlendFactorZero;
        case GfxBlendFactor::One:
            return MTL::BlendFactorOne;
        case GfxBlendFactor::SrcColor:
            return MTL::BlendFactorSourceColor;
        case GfxBlendFactor::InvSrcColor:
            return MTL::BlendFactorOneMinusSourceColor;
        case GfxBlendFactor::SrcAlpha:
            return MTL::BlendFactorSourceAlpha;
        case GfxBlendFactor::InvSrcAlpha:
            return MTL::BlendFactorOneMinusSourceAlpha;
        case GfxBlendFactor::DstAlpha:
            return MTL::BlendFactorDestinationAlpha;
        case GfxBlendFactor::InvDstAlpha:
            return MTL::BlendFactorOneMinusDestinationAlpha;
        case GfxBlendFactor::DstColor:
            return MTL::BlendFactorDestinationColor;
        case GfxBlendFactor::InvDstColor:
            return MTL::BlendFactorOneMinusDestinationColor;
        case GfxBlendFactor::SrcAlphaClamp:
            return MTL::BlendFactorSourceAlphaSaturated;
        case GfxBlendFactor::ConstantFactor:
            return MTL::BlendFactorBlendColor;
        case GfxBlendFactor::InvConstantFactor:
            return MTL::BlendFactorOneMinusBlendColor;
        default:
            return MTL::BlendFactorZero;
    }
}

inline MTL::BlendOperation ToBlendOperation(GfxBlendOp blend_op)
{
    switch (blend_op)
    {
        case GfxBlendOp::Add:
            return MTL::BlendOperationAdd;
        case GfxBlendOp::Subtract:
            return MTL::BlendOperationSubtract;
        case GfxBlendOp::ReverseSubtract:
            return MTL::BlendOperationReverseSubtract;
        case GfxBlendOp::Min:
            return MTL::BlendOperationMin;
        case GfxBlendOp::Max:
            return MTL::BlendOperationMax;
        default:
            return MTL::BlendOperationAdd;
    }
}

inline MTL::ColorWriteMask ToColorWriteMask(GfxColorWriteMask mask)
{
    MTL::ColorWriteMask mtlMask = MTL::ColorWriteMaskNone;
    
    if(mask & GfxColorWriteMaskR) mtlMask |= MTL::ColorWriteMaskRed;
    if(mask & GfxColorWriteMaskG) mtlMask |= MTL::ColorWriteMaskGreen;
    if(mask & GfxColorWriteMaskB) mtlMask |= MTL::ColorWriteMaskBlue;
    if(mask & GfxColorWriteMaskA) mtlMask |= MTL::ColorWriteMaskAlpha;
    
    return mtlMask;
}

inline MTL::CompareFunction ToCompareFunction(GfxCompareFunc func)
{
    switch (func)
    {
        case GfxCompareFunc::Never:
            return MTL::CompareFunctionNever;
        case GfxCompareFunc::Less:
            return MTL::CompareFunctionLess;
        case GfxCompareFunc::Equal:
            return MTL::CompareFunctionEqual;
        case GfxCompareFunc::LessEqual:
            return MTL::CompareFunctionLessEqual;
        case GfxCompareFunc::Greater:
            return MTL::CompareFunctionGreater;
        case GfxCompareFunc::NotEqual:
            return MTL::CompareFunctionNotEqual;
        case GfxCompareFunc::GreaterEqual:
            return MTL::CompareFunctionGreaterEqual;
        case GfxCompareFunc::Always:
            return MTL::CompareFunctionAlways;
        default:
            return MTL::CompareFunctionAlways;
    }
}

inline MTL::StencilOperation ToStencilOperation(GfxStencilOp stencil_op)
{
    switch (stencil_op)
    {
        case GfxStencilOp::Keep:
            return MTL::StencilOperationKeep;
        case GfxStencilOp::Zero:
            return MTL::StencilOperationZero;
        case GfxStencilOp::Replace:
            return MTL::StencilOperationReplace;
        case GfxStencilOp::IncreaseClamp:
            return MTL::StencilOperationIncrementClamp;
        case GfxStencilOp::DecreaseClamp:
            return MTL::StencilOperationDecrementClamp;
        case GfxStencilOp::Invert:
            return MTL::StencilOperationInvert;
        case GfxStencilOp::IncreaseWrap:
            return MTL::StencilOperationIncrementWrap;
        case GfxStencilOp::DecreaseWrap:
            return MTL::StencilOperationDecrementWrap;
        default:
            return MTL::StencilOperationKeep;
    }
}

inline MTL::DepthStencilDescriptor* ToDepthStencilDescriptor(GfxDepthStencilState state)
{
    MTL::DepthStencilDescriptor* descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    descriptor->setDepthCompareFunction(state.depth_test ? ToCompareFunction(state.depth_func) : MTL::CompareFunctionAlways);
    descriptor->setDepthWriteEnabled(state.depth_write);
    
    if(state.stencil_test)
    {
        MTL::StencilDescriptor* front = descriptor->frontFaceStencil();
        front->setStencilCompareFunction(ToCompareFunction(state.front.stencil_func));
        front->setStencilFailureOperation(ToStencilOperation(state.front.stencil_fail));
        front->setDepthFailureOperation(ToStencilOperation(state.front.depth_fail));
        front->setDepthStencilPassOperation(ToStencilOperation(state.front.pass));
        front->setReadMask(state.stencil_read_mask);
        front->setWriteMask(state.stencil_write_mask);
        
        MTL::StencilDescriptor* back = descriptor->backFaceStencil();
        back->setStencilCompareFunction(ToCompareFunction(state.back.stencil_func));
        back->setStencilFailureOperation(ToStencilOperation(state.back.stencil_fail));
        back->setDepthFailureOperation(ToStencilOperation(state.back.depth_fail));
        back->setDepthStencilPassOperation(ToStencilOperation(state.back.pass));
        back->setReadMask(state.stencil_read_mask);
        back->setWriteMask(state.stencil_write_mask);
    }
    
    return descriptor;
}

inline MTL::CullMode ToCullMode(GfxCullMode mode)
{
    switch (mode)
    {
        case GfxCullMode::Front:
            return MTL::CullModeFront;
        case GfxCullMode::Back:
            return MTL::CullModeBack;
        case GfxCullMode::None:
        default:
            return MTL::CullModeNone;
    }
}

inline MTL::SamplerMinMagFilter ToSamplerMinMagFilter(GfxFilter filter)
{
    switch (filter)
    {
        case GfxFilter::Point:
            return MTL::SamplerMinMagFilterNearest;
        case GfxFilter::Linear:
            return MTL::SamplerMinMagFilterLinear;
        default:
            return MTL::SamplerMinMagFilterNearest;
    }
}

inline MTL::SamplerMipFilter ToSamplerMipFilter(GfxFilter filter)
{
    switch (filter)
    {
        case GfxFilter::Point:
            return MTL::SamplerMipFilterNearest;
        case GfxFilter::Linear:
            return MTL::SamplerMipFilterLinear;
        default:
            return MTL::SamplerMipFilterNotMipmapped;
    }
}

inline MTL::SamplerAddressMode ToSamplerAddressMode(GfxSamplerAddressMode mode)
{
    switch (mode)
    {
        case GfxSamplerAddressMode::Repeat:
            return MTL::SamplerAddressModeRepeat;
        case GfxSamplerAddressMode::MirroredRepeat:
            return MTL::SamplerAddressModeMirrorRepeat;
        case GfxSamplerAddressMode::ClampToEdge:
            return MTL::SamplerAddressModeClampToEdge;
        case GfxSamplerAddressMode::ClampToBorder:
            return MTL::SamplerAddressModeClampToBorderColor;
        default:
            return MTL::SamplerAddressModeRepeat;
    }
}

inline MTL::AccelerationStructureUsage ToAccelerationStructureUsage(GfxRayTracingASFlag flags)
{
    MTL::AccelerationStructureUsage usage = MTL::AccelerationStructureUsageNone;
    
    if(flags & GfxRayTracingASFlagAllowUpdate)
    {
        usage |= MTL::AccelerationStructureUsageRefit;
    }
    
    if(flags & GfxRayTracingASFlagPreferFastBuild)
    {
        usage |= MTL::AccelerationStructureUsagePreferFastBuild;
    }
    
    return usage;
}

inline MTL::AttributeFormat ToAttributeFormat(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::RGB32F:
            return MTL::AttributeFormatFloat3;
        case GfxFormat::RGBA32F:
            return MTL::AttributeFormatFloat4;
        case GfxFormat::RGBA16F:
            return MTL::AttributeFormatHalf4;
        default:
            RE_ASSERT(false);
            return MTL::AttributeFormatInvalid;
    }
}

inline MTL::AccelerationStructureInstanceOptions ToAccelerationStructureInstanceOptions(GfxRayTracingInstanceFlag flags)
{
    MTL::AccelerationStructureInstanceOptions options = MTL::AccelerationStructureInstanceOptionNone;
    
    if(flags & GfxRayTracingInstanceFlagDisableCull)
    {
        options |= MTL::AccelerationStructureInstanceOptionDisableTriangleCulling;
    }
    
    if(flags & GfxRayTracingInstanceFlagFrontFaceCCW)
    {
        options |= MTL::AccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise;
    }
    
    if(flags & GfxRayTracingInstanceFlagForceOpaque)
    {
        options |= MTL::AccelerationStructureInstanceOptionOpaque;
    }
    
    if(flags & GfxRayTracingInstanceFlagForceNoOpaque)
    {
        options |= MTL::AccelerationStructureInstanceOptionNonOpaque;
    }
    
    return options;
}

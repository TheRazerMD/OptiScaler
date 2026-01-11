#pragma once

#include <pch.h>

#include "IFeature_Vk.h"

#include <inputs/NVNGX_DLSS.h>

class FeatureProvider_Vk
{
  public:
    static bool GetFeature(std::string upscalerName, UINT handleId, NVSDK_NGX_Parameter* parameters,
                           std::unique_ptr<IFeature_Vk>* feature);

    static bool ChangeFeature(std::string upscalerName, VkInstance instance, VkPhysicalDevice pd, VkDevice device,
                              VkCommandBuffer cmdBuffer, PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa,
                              UINT handleId, NVSDK_NGX_Parameter* parameters, ContextData<IFeature_Vk>* contextData);
};

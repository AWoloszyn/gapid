/*
 * Copyright (C) 2018 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cmd/read_pack_cpp/vulkan_replay_types.h"
#include "cmd/read_pack_cpp/vulkan_replay_imports.h"
#include "cmd/read_pack_cpp/helpers2.h"
#include "cmd/read_pack_cpp/vulkan_replay_subroutines.h"
#include "cmd/read_pack_cpp/helpers3.h"
#include "gapis/api/vulkan/vulkan_pb/api.pb.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "cmd/read_pack_cpp/stb_image.h"
#include "cmd/read_pack_cpp/stb_image_write.h"

std::map<uintptr_t, std::pair<void*, uintptr_t>> _mapped_ranges;
extern gapii::VulkanImports::PFNVKGETINSTANCEPROCADDR get_instance_proc_addr;

namespace gapii {

  PFN_vkVoidFunction SpyOverride_vkGetInstanceProcAddr(VkInstance instance, char* pName) {
    return nullptr;
  }

PFN_vkVoidFunction SpyOverride_vkGetDeviceProcAddr(VkDevice device, char* pName) {
    return nullptr;
}

uint32_t SpyOverride_vkDebugMarkerSetObjectTagEXT(uint64_t, VkDebugMarkerObjectTagInfoEXT*) {
  return 0;
}
uint32_t SpyOverride_vkDebugMarkerSetObjectInfoEXT(uint64_t, VkDebugMarkerMarkerInfoEXT*) { return 0; }
uint32_t SpyOverride_vkDebugMarkerSetObjectNameEXT(uint64_t, VkDebugMarkerObjectNameInfoEXT*) { return 0; }
void SpyOverride_vkCmdDebugMarkerBeginEXT(uint64_t, VkDebugMarkerMarkerInfoEXT *) {  }
void SpyOverride_vkCmdDebugMarkerEndEXT(uint64_t) { }
void SpyOverride_vkCmdDebugMarkerInsertEXT(uint64_t, VkDebugMarkerMarkerInfoEXT *) {}

  // Override API functions
  // SpyOverride_vkGetInstanceProcAddr(), SpyOverride_vkGetDeviceProcAddr(),
  // SpyOverride_vkCreateInstancef() and SpyOverride_vkCreateDevice() require
  // the their function table to be created through the template system, so they
  // won't be defined here, but vk_spy_helpers.cpp.tmpl
  uint32_t SpyOverride_vkEnumerateInstanceLayerProperties(
    uint32_t* pCount, VkLayerProperties* pProperties) {
    if (pProperties == NULL) {
      *pCount = 1;
      return VkResult::VK_SUCCESS;
    }
    if (pCount == 0) {
      return VkResult::VK_INCOMPLETE;
    }
    *pCount = 1;
    memset(pProperties, 0x00, sizeof(*pProperties));
    strcpy((char*)&pProperties->mlayerName[0], "VkGraphicsSpy");
    //pProperties->mspecVersion = VK_VERSION_MAJOR(1) | VK_VERSION_MINOR(0) | 5;
    //pProperties->mimplementationVersion = 1;
    strcpy((char*)&pProperties->mdescription[0], "vulkan_trace");
    return VkResult::VK_SUCCESS;
  }

  uint32_t SpyOverride_vkEnumerateDeviceLayerProperties(
    VkPhysicalDevice dev, uint32_t* pCount, VkLayerProperties* pProperties) {
    if (pProperties == NULL) {
      *pCount = 1;
      return VkResult::VK_SUCCESS;
    }
    if (pCount == 0) {
      return VkResult::VK_INCOMPLETE;
    }
    *pCount = 1;
    memset(pProperties, 0x00, sizeof(*pProperties));
    strcpy((char*)&pProperties->mlayerName[0], "VkGraphicsSpy");
    //pProperties->mspecVersion = VK_VERSION_MAJOR(1) | VK_VERSION_MINOR(0) | 5;
    pProperties->mimplementationVersion = 1;
    strcpy((char*)&pProperties->mdescription[0], "vulkan_trace");
    return VkResult::VK_SUCCESS;
  }
  uint32_t SpyOverride_vkEnumerateInstanceExtensionProperties(
    char* pLayerName, uint32_t* pCount,
    VkExtensionProperties* pProperties) {
    *pCount = 0;
    return VkResult::VK_SUCCESS;
  }

  uint32_t SpyOverride_vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, char* pLayerName, uint32_t* pCount,
    VkExtensionProperties* pProperties) {
    gapii::VulkanImports::PFNVKENUMERATEDEVICEEXTENSIONPROPERTIES
      next_layer_enumerate_extensions = NULL;
    auto phy_dev_iter = mState.PhysicalDevices.find(physicalDevice);
    if (phy_dev_iter != mState.PhysicalDevices.end()) {
      auto inst_func_iter =
        mImports.mVkInstanceFunctions.find(phy_dev_iter->second->mInstance);
      if (inst_func_iter != mImports.mVkInstanceFunctions.end()) {
        next_layer_enumerate_extensions = reinterpret_cast<
          gapii::VulkanImports::PFNVKENUMERATEDEVICEEXTENSIONPROPERTIES>(
            inst_func_iter->second.vkEnumerateDeviceExtensionProperties);
      }
    }

    uint32_t next_layer_count = 0;
    uint32_t next_layer_result;
    if (next_layer_enumerate_extensions) {
      next_layer_result = next_layer_enumerate_extensions(
        physicalDevice, pLayerName, &next_layer_count, NULL);
      if (next_layer_result != VkResult::VK_SUCCESS) {
        return next_layer_result;
      }
    }
    std::vector<VkExtensionProperties> properties(next_layer_count,
      VkExtensionProperties{});
    if (next_layer_enumerate_extensions) {
      next_layer_result = next_layer_enumerate_extensions(
        physicalDevice, pLayerName, &next_layer_count, properties.data());
      if (next_layer_result != VkResult::VK_SUCCESS) {
        return next_layer_result;
      }
    }

    bool has_debug_marker_ext = false;
    for (VkExtensionProperties& ext : properties) {
      // TODO: Check the spec version and emit warning if not match.
      // TODO: refer to VK_EXT_DEBUG_MARKER_EXTENSION_NAME
      if (!strcmp(&ext.mextensionName[0], "VK_EXT_debug_marker")) {
        has_debug_marker_ext = true;
        break;
      }
    }
    if (!has_debug_marker_ext) {
      // TODO: refer to VK_EXT_DEBUG_MARKER_EXTENSION_NAME and
      // VK_EXT_DEBUG_MARKER_SPEC_VERSION
      char debug_marker_extension_name[] = "VK_EXT_debug_marker";
      uint32_t debug_marker_spec_version = 4;
      VkExtensionProperties props;
      memcpy(&props.mextensionName[0], debug_marker_extension_name, strlen(debug_marker_extension_name));
      props.mspecVersion = debug_marker_spec_version;
      properties.emplace_back(props);
    }

    auto extension = subSupportedDeviceExtensions(nullptr);
    std::vector<VkExtensionProperties> all_properties;
    for (VkExtensionProperties& ext : properties) {
      //gapil::String name(, &ext.mextensionName[0]);
      //if (!mHideUnknownExtensions || (extension->mExtensionNames.find(name) !=
      //                                extension->mExtensionNames.end())) {
      all_properties.push_back(ext);
      //}
    }

    if (pProperties == NULL) {
      *pCount = all_properties.size();
      return VkResult::VK_SUCCESS;
    }
    uint32_t copy_count =
      all_properties.size() < *pCount ? all_properties.size() : *pCount;
    memcpy(pProperties, all_properties.data(),
      copy_count * sizeof(VkExtensionProperties));
    if (*pCount < all_properties.size()) {
      return VkResult::VK_INCOMPLETE;
    }
    *pCount = all_properties.size();
    return VkResult::VK_SUCCESS;
  }

  void SpyOverride_vkDestroyInstance(
    VkInstance instance, VkAllocationCallbacks* pAllocator) {
    // First we have to find the function to chain to, then we have to
    // remove this instance from our list, then we forward the call.
    auto it = mImports.mVkInstanceFunctions.find(instance);
    gapii::VulkanImports::PFNVKDESTROYINSTANCE destroy_instance =
      it == mImports.mVkInstanceFunctions.end() ? nullptr
      : it->second.vkDestroyInstance;
    if (destroy_instance) {
      destroy_instance(instance, pAllocator);
    }
    mImports.mVkInstanceFunctions.erase(
      mImports.mVkInstanceFunctions.find(instance));
  }

  uint32_t SpyOverride_vkCreateBuffer(
    VkDevice device, VkBufferCreateInfo* pCreateInfo,
    VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    //if (is_suspended()) {
    //  VkBufferCreateInfo override_create_info = *pCreateInfo;
    //  override_create_info.musage |=
    //      VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    //  return mImports.mVkDeviceFunctions[device].vkCreateBuffer(
    //      device, &override_create_info, pAllocator, pBuffer);
    //} else {
    return mImports.mVkDeviceFunctions[device].vkCreateBuffer(
      device, pCreateInfo, pAllocator, pBuffer);
    //}
  }

  uint32_t SpyOverride_vkCreateImage(
    VkDevice device, VkImageCreateInfo* pCreateInfo,
    VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    VkImageCreateInfo override_create_info = *pCreateInfo;
    override_create_info.musage |=
      VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return mImports.mVkDeviceFunctions[device].vkCreateImage(
      device, &override_create_info, pAllocator, pImage);
  }


  VKAPI_ATTR void VKAPI_CALL vkSetSwapchainCallback(
      VkSwapchainKHR swapchain, void callback(void *, uint8_t *, size_t),
      void *user_data);
  
  void swp_callback(void* userdata, uint8_t* _d, size_t sz) {
    auto swapchain = (uint64_t)(userdata);
    auto& r = mState.Swapchains[swapchain];
    auto width = r->mInfo.mExtent.mWidth;
    auto height = r->mInfo.mExtent.mHeight;
    static size_t framenum = 0;
    if (framenum++ % 100 == 0) {
      std::string nm = "frame" + std::to_string(framenum) + ".png";
      for (size_t i = 0; i < sz / 4; ++i) {
        _d[4 * i] ^= _d[4 * i + 2];
        _d[4 * i + 2] ^= _d[4 * i];
        _d[4 * i] ^= _d[4 * i + 2];
      }
      stbi_write_png(nm.c_str(), width, height, 4, _d, width * 4);
      std::cout << "Got image data " << std::to_string(framenum) << std::endl;
    }
  }


  uint32_t SpyOverride_vkCreateSwapchainKHR(
    VkDevice device, VkSwapchainCreateInfoKHR* pCreateInfo,
    VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pImage) {
    //if (is_observing() || is_suspended()) {
    //  VkSwapchainCreateInfoKHR override_create_info = *pCreateInfo;
    //  override_create_info.mimageUsage |=
    //      VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    //  return mImports.mVkDeviceFunctions[device].vkCreateSwapchainKHR(
    //      device, &override_create_info, pAllocator, pImage);
    //} else {
    uint32_t swp = mImports.mVkDeviceFunctions[device].vkCreateSwapchainKHR(
      device, pCreateInfo, pAllocator, pImage);
    auto physicalDevice = mState.Devices[device]->mPhysicalDevice;

    gapii::VulkanImports::PFNVKGETDEVICEPROCADDR get_device_proc_addr =
      reinterpret_cast<gapii::VulkanImports::PFNVKGETDEVICEPROCADDR>(
        get_instance_proc_addr(mState.PhysicalDevices[physicalDevice]->mInstance, "vkGetDeviceProcAddr"));

    decltype(&vkSetSwapchainCallback) cb = (decltype(&vkSetSwapchainCallback))get_device_proc_addr(device, "vkSetSwapchainCallback");
    
    cb(*pImage, &swp_callback, (void*)*pImage);
    return swp;
  }

  void SpyOverride_vkDestroyDevice(
    VkDevice device, VkAllocationCallbacks* pAllocator) {
    // First we have to find the function to chain to, then we have to
    // remove this instance from our list, then we forward the call.
    auto it = mImports.mVkDeviceFunctions.find(device);
    gapii::VulkanImports::PFNVKDESTROYDEVICE destroy_device =
      it == mImports.mVkDeviceFunctions.end() ? nullptr
      : it->second.vkDestroyDevice;
    if (destroy_device) {
      destroy_device(device, pAllocator);
    }
    mImports.mVkDeviceFunctions.erase(mImports.mVkDeviceFunctions.find(device));
  }

  uint32_t SpyOverride_vkAllocateMemory(
    VkDevice device, VkMemoryAllocateInfo* pAllocateInfo,
    VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    uint32_t r = mImports.mVkDeviceFunctions[device].vkAllocateMemory(
      device, pAllocateInfo, pAllocator, pMemory);
    auto l_physical_device =
      mState.PhysicalDevices[mState.Devices[device]->mPhysicalDevice];
    if (0 !=
      (l_physical_device->mMemoryProperties
        .mmemoryTypes[pAllocateInfo->mmemoryTypeIndex]
        .mpropertyFlags &
        ((uint32_t)(
          VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)))) {
      // This is host-coherent memory. Some drivers actually allocate these pages
      // on-demand. This forces all of the pages to be created. This is needed as
      // our coherent memory tracker relies on page-faults which interferes with
      // the on-demand allocation.
      char* memory;
      mImports.mVkDeviceFunctions[device].vkMapMemory(
        device, *pMemory, 0, pAllocateInfo->mallocationSize, 0,
        reinterpret_cast<void**>(&memory));
      memset(memory, 0x00, pAllocateInfo->mallocationSize);
      mImports.mVkDeviceFunctions[device].vkUnmapMemory(device, *pMemory);
    }
    return r;
  }

uint32_t ReplayOverride_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
  auto it = mImports.mVkInstanceFunctions.find(instance);
  if (pPhysicalDevices == nullptr) {
    return it->second.vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
  }

  auto no = get_next_object("vulkan.PhysicalDevicesAndProperties");
  vulkan::PhysicalDevicesAndProperties props;
  props.ParseFromString(no);
  
  
  auto key_size = props.phydevtoproperties().keys_size();
  std::vector<uint64_t> device_ids;
  std::vector<uint64_t> vendor_ids;
  for (size_t i = 0; i < key_size; ++i) {
    auto& pdp = props.phydevtoproperties().values(i);
    device_ids.push_back(pdp.deviceid());
    vendor_ids.push_back(pdp.vendorid());
  }
  
  uint32_t real_phys_dev_count = 0;
  it->second.vkEnumeratePhysicalDevices(instance, &real_phys_dev_count, nullptr);
  std::vector<VkPhysicalDevice> physicalDevices;
  physicalDevices.resize(real_phys_dev_count);
  it->second.vkEnumeratePhysicalDevices(instance, &real_phys_dev_count, physicalDevices.data());

  std::vector<uint64_t> new_device_ids;
  std::vector<uint64_t> new_vendor_ids;
  std::vector<uint64_t> use_count;
  for (size_t i = 0; i < real_phys_dev_count; ++i) {
    VkPhysicalDeviceProperties new_props;
    it->second.vkGetPhysicalDeviceProperties(physicalDevices[i], &new_props);
    new_device_ids.push_back(new_props.mdeviceID);
    new_vendor_ids.push_back(new_props.mvendorID);
    use_count.push_back(0);
  }

  // Last thing to do: Remap devices.
  for (size_t i = 0; i < *pPhysicalDeviceCount; ++i) {
    bool any_suitable = false;
    for (uint32_t use_ct = 0;; use_ct++) {
      for (size_t j = 0; j < new_device_ids.size(); ++j) {
        if (new_device_ids[j] == device_ids[i] &&
          new_vendor_ids[j] == vendor_ids[i]) {
          if (use_count[j] <= use_ct) {
            std::cout << "Remapping physical device " << i << "->" << j << std::endl;
            pPhysicalDevices[i] = physicalDevices[j];
            use_count[j]++;
            goto next_device;
          }
          else {
            any_suitable = true;
          }
        }
      }
      if (!any_suitable) { break; }
    }
    if (!any_suitable) {
      std::cout << "Remapping physical device " << i << "->0" << std::endl;
      pPhysicalDevices[i] = physicalDevices[0];
    }
// GASP PLEASE TAKE THIS OUT
  next_device:
    ;
  }
  return 0;
}

uint32_t
ReplayOverride_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                     uint64_t timeout, VkSemaphore semaphore,
                                     VkFence fence, uint32_t *pImageIndex) {
  std::vector<uint8_t> nImages;
  nImages.resize(sizeof(uint32_t));

  get_next_write_to(pImageIndex, nImages);
  *pImageIndex = *((uint32_t*)(nImages.data()));

  return mImports.mVkDeviceFunctions[device].vkAcquireNextImageKHR(
      device, swapchain, timeout, semaphore, fence, pImageIndex);
}

static const uint32_t VIRTUAL_SWAPCHAIN_CREATE_PNEXT = 0xFFFFFFAA;
struct struct_header{
  uint32_t sType;
  void* pNext;
  void *surfaceCreateInfo;
};

void add_surface_pnext(void* createInfo) {
  static struct_header surfHeader{
    VIRTUAL_SWAPCHAIN_CREATE_PNEXT,
    nullptr,
    nullptr,
  };
  struct_header* sh = (struct_header*)createInfo;
  sh->pNext = &surfHeader;
}

uint32_t ReplayOverride_vkCreateAndroidSurfaceKHR(VkInstance instance, VkAndroidSurfaceCreateInfoKHR* pCreateInfo, AllocationCallbacks pAllocator, VkSurfaceKHR* pSurface) {
  add_surface_pnext(pCreateInfo);
  return mImports.mVkInstanceFunctions[instance].vkCreateAndroidSurfaceKHR(
      instance, pCreateInfo, pAllocator, pSurface);
}

uint32_t ReplayOverride_vkCreateXlibSurfaceKHR(VkInstance instance, VkXlibSurfaceCreateInfoKHR* pCreateInfo, AllocationCallbacks pAllocator, VkSurfaceKHR* pSurface) {
  add_surface_pnext(pCreateInfo);
  return mImports.mVkInstanceFunctions[instance].vkCreateXlibSurfaceKHR(
    instance, pCreateInfo, pAllocator, pSurface);
}


uint32_t ReplayOverride_vkCreateXcbSurfaceKHR(VkInstance instance, VkXcbSurfaceCreateInfoKHR* pCreateInfo, AllocationCallbacks pAllocator, VkSurfaceKHR* pSurface) {
  add_surface_pnext(pCreateInfo);
  return mImports.mVkInstanceFunctions[instance].vkCreateXcbSurfaceKHR(
    instance, pCreateInfo, pAllocator, pSurface);
}


uint32_t ReplayOverride_vkCreateWaylandSurfaceKHR(VkInstance instance, VkWaylandSurfaceCreateInfoKHR* pCreateInfo, AllocationCallbacks pAllocator, VkSurfaceKHR* pSurface) {
  add_surface_pnext(pCreateInfo);
  return mImports.mVkInstanceFunctions[instance].vkCreateWaylandSurfaceKHR(
    instance, pCreateInfo, pAllocator, pSurface);
}


uint32_t ReplayOverride_vkCreateMirSurfaceKHR(VkInstance instance, VkMirSurfaceCreateInfoKHR* pCreateInfo, AllocationCallbacks pAllocator, VkSurfaceKHR* pSurface) {
  add_surface_pnext(pCreateInfo);
  return mImports.mVkInstanceFunctions[instance].vkCreateMirSurfaceKHR(
    instance, pCreateInfo, pAllocator, pSurface);
}


uint32_t ReplayOverride_vkCreateWin32SurfaceKHR(VkInstance instance, VkWin32SurfaceCreateInfoKHR* pCreateInfo, AllocationCallbacks pAllocator, VkSurfaceKHR* pSurface) {
  add_surface_pnext(pCreateInfo);
  return mImports.mVkInstanceFunctions[instance].vkCreateWin32SurfaceKHR(
    instance, pCreateInfo, pAllocator, pSurface);
}


} // namespace gapii
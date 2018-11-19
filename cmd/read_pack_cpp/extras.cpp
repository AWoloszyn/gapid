/*
 * Copyright (C) 2017 Google Inc.
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

#include "gapil/runtime/cc/runtime.h"
#include "gapil/runtime/cc/slice.inc"
#include "gapil/runtime/cc/string.h"
#include "cmd/read_pack_cpp/vulkan_replay_types.h"
#include "cmd/read_pack_cpp/vulkan_replay_subroutines.h"
#include "helpers2.h"

#include <functional>

namespace gapii {

struct destroyer {
  destroyer(const std::function<void(void)>& f) { destroy = f; }
  ~destroyer() { destroy(); }
  std::function<void(void)> destroy;
};


void walkImageSubRng(
    std::shared_ptr<ImageObject> img, VkImageSubresourceRange rng,
    std::function<void(uint32_t aspect_bit, uint32_t layer, uint32_t level)>
        f) {
  uint32_t layer_count =
      subImageSubresourceLayerCount(nullptr, img, rng);
  uint32_t level_count =
      subImageSubresourceLevelCount(nullptr, img, rng);
  auto aspect_map =
      subUnpackImageAspectFlags(nullptr, rng.maspectMask);
  for (auto b : aspect_map) {
    auto ai = img->mAspects.find(b.second);
    if (ai == img->mAspects.end()) {
      continue;
    }
    for (uint32_t layer = rng.mbaseArrayLayer;
         layer < rng.mbaseArrayLayer + layer_count; layer++) {
      auto layi = ai->second->mLayers.find(layer);
      if (layi == ai->second->mLayers.end()) {
        continue;
      }
      for (uint32_t level = rng.mbaseMipLevel;
           level < rng.mbaseMipLevel + level_count; level++) {
        auto levi = layi->second->mLevels.find(level);
        if (levi == layi->second->mLevels.end()) {
          continue;
        }
        f(b.second, layer, level);
      }
    }
  }
}

// Extern functions
void trackMappedCoherentMemory(uint64_t start,
                                          size_t size) {}

void readMappedCoherentMemory(VkDeviceMemory memory,
                                         uint64_t offset_in_mapped,
                                         size_t readSize) {
}

void untrackMappedCoherentMemory(uint64_t start,
                                            size_t size) {
}

void mapMemory(void** mappedLocation, gapil::Slice<uint8_t> _s) {
  void* mappedLoc = *mappedLocation;
  uintptr_t sz = _s.size();
  _pending_writes.push_back(
      pending_write{(uintptr_t)mappedLocation, (uintptr_t)sizeof(void *),
                    [mappedLoc, mappedLocation, sz]() {
                      std::cout << "Mapping " << mappedLocation[0] << " -> "
                                << mappedLoc << std::endl;
                      _mapped_ranges[(uintptr_t)mappedLocation[0]] = std::make_pair(mappedLoc, sz);
                    }});
}

void unmapMemory(gapil::Slice<uint8_t> _x) {
  for (auto it = _mapped_ranges.begin(); it != _mapped_ranges.end(); ++it) {
    if (it->second.first == &_x[0]) {
      std::cout << "Unmapping: " << (void*)it->first << " -> " << (void*)it->second.first << std::endl;
    }
  }
}
void recordFenceSignal(uint64_t) {}
void recordFenceWait(uint64_t) {}
void recordFenceReset(uint64_t) {}
void recordAcquireNextImage(uint64_t, uint32_t) {}
void recordPresentSwapchainImage(uint64_t, uint32_t) {
}

bool hasDynamicProperty(VkPipelineDynamicStateCreateInfo* info,
                                   uint32_t state) {
  if (!info) {
    return false;
  }
  fixup_pointer(&info->mpDynamicStates);
  for (size_t i = 0; i < info->mdynamicStateCount; ++i) {
    if (info->mpDynamicStates[i] == state) {
      return true;
    }
  }
  return false;
}

void resetCmd(VkCommandBuffer cmdBuf) {}
void enterSubcontext() {}
void leaveSubcontext() {}
void nextSubcontext() {}
void resetSubcontext() {}
void onPreSubcommand(std::shared_ptr<CommandReference>) {}
void onPreProcessCommand(std::shared_ptr<CommandReference>) {}
void onPostSubcommand(std::shared_ptr<CommandReference>) {}
void onDeferSubcommand(std::shared_ptr<CommandReference>) {}
void onCommandAdded(VkCommandBuffer) {}
void postBindSparse(std::shared_ptr<QueuedSparseBinds>) {}
void pushDebugMarker(std::string) {}
void popDebugMarker() {}
void pushRenderPassMarker(VkRenderPass) {}
void popRenderPassMarker() {}
void popAndPushMarkerForNextSubpass(uint32_t) {}

std::shared_ptr<PhysicalDevicesAndProperties>
fetchPhysicalDeviceProperties(VkInstance instance,
                                         gapil::Slice<VkPhysicalDevice> devs) {
  auto props = std::make_shared<PhysicalDevicesAndProperties>();
  for (VkPhysicalDevice dev : devs) {
    props->mPhyDevToProperties[dev] = VkPhysicalDeviceProperties();
    mImports.mVkInstanceFunctions[instance].vkGetPhysicalDeviceProperties(
        dev, &props->mPhyDevToProperties[dev]);
  }
  return props;
}

std::shared_ptr<PhysicalDevicesMemoryProperties>
fetchPhysicalDeviceMemoryProperties(
    VkInstance instance,
    gapil::Slice<VkPhysicalDevice> devs) {
  auto props = std::make_shared<PhysicalDevicesMemoryProperties>();
  for (VkPhysicalDevice dev : devs) {
    props->mPhyDevToMemoryProperties[dev] =
        VkPhysicalDeviceMemoryProperties();
    mImports.mVkInstanceFunctions[instance].vkGetPhysicalDeviceMemoryProperties(
        dev, &props->mPhyDevToMemoryProperties[dev]);
  }
  return props;
}

std::shared_ptr<PhysicalDevicesAndQueueFamilyProperties>
fetchPhysicalDeviceQueueFamilyProperties(
    VkInstance instance,
    gapil::Slice<VkPhysicalDevice> devs) {
  auto all_props =
      std::make_shared<PhysicalDevicesAndQueueFamilyProperties>();
  for (VkPhysicalDevice dev : devs) {
    uint32_t propCount = 0;
    mImports.mVkInstanceFunctions[instance]
        .vkGetPhysicalDeviceQueueFamilyProperties(dev, &propCount, nullptr);
    std::vector<VkQueueFamilyProperties> props(
        propCount, VkQueueFamilyProperties());
    mImports.mVkInstanceFunctions[instance]
        .vkGetPhysicalDeviceQueueFamilyProperties(dev, &propCount,
                                                  props.data());
    for (uint32_t i = 0; i < props.size(); i++) {
      all_props->mPhyDevToQueueFamilyProperties[dev][i] = props[i];
    }
  }
  return all_props;
}

std::shared_ptr<PhysicalDevicesFormatProperties>
fetchPhysicalDeviceFormatProperties(
    VkInstance instance,
    gapil::Slice<VkPhysicalDevice> devs) {
  auto props = std::make_shared<PhysicalDevicesFormatProperties>();
  return props;
}

std::shared_ptr<ImageMemoryRequirements> fetchImageMemoryRequirements(
    VkDevice device, VkImage image, bool hasSparseBit) {
  auto reqs = std::make_shared<ImageMemoryRequirements>();
  mImports.mVkDeviceFunctions[device].vkGetImageMemoryRequirements(
      device, image, &reqs->mMemoryRequirements);
  if (hasSparseBit) {
    uint32_t sparse_mem_req_count = 0;
    mImports.mVkDeviceFunctions[device].vkGetImageSparseMemoryRequirements(
        device, image, &sparse_mem_req_count, nullptr);
    std::vector<VkSparseImageMemoryRequirements> sparse_mem_reqs(
        sparse_mem_req_count, VkSparseImageMemoryRequirements());
    mImports.mVkDeviceFunctions[device].vkGetImageSparseMemoryRequirements(
        device, image, &sparse_mem_req_count, sparse_mem_reqs.data());
    for (VkSparseImageMemoryRequirements& req : sparse_mem_reqs) {
      auto aspect_map = subUnpackImageAspectFlags(
          nullptr, req.mformatProperties.maspectMask);
      for (auto aspect : aspect_map) {
        reqs->mAspectBitsToSparseMemoryRequirements[aspect.second] = req;
      }
    }
  }
  return reqs;
}

VkMemoryRequirements fetchBufferMemoryRequirements(
    VkDevice device, VkBuffer buffer) {
  auto reqs = VkMemoryRequirements();
  mImports.mVkDeviceFunctions[device].vkGetBufferMemoryRequirements(
      device, buffer, &reqs);
  return reqs;
}

std::shared_ptr<LinearImageLayouts> fetchLinearImageSubresourceLayouts(
    VkDevice device, std::shared_ptr<ImageObject> image,
    VkImageSubresourceRange rng) {
  auto layouts = std::make_shared<LinearImageLayouts>();
  walkImageSubRng(
      image, rng,
      [&layouts, device, &image](uint32_t aspect_bit, uint32_t layer,
                                       uint32_t level) {
        VkImageSubresource subres(VkImageAspectFlags(aspect_bit),  // aspectMask
                                  level,                           // mipLevel
                                  layer                            // arrayLayer
        );
        auto aspect_i = layouts->mAspectLayouts.find(aspect_bit);
        if (aspect_i == layouts->mAspectLayouts.end()) {
          layouts->mAspectLayouts[aspect_bit] =
              std::make_shared<LinearImageAspectLayouts>();
          aspect_i = layouts->mAspectLayouts.find(aspect_bit);
        }
        auto layer_i = aspect_i->second->mLayerLayouts.find(layer);
        if (layer_i == aspect_i->second->mLayerLayouts.end()) {
          aspect_i->second->mLayerLayouts[layer] =
              std::make_shared<LinearImageLayerLayouts>();
          layer_i = aspect_i->second->mLayerLayouts.find(layer);
        }
        layer_i->second->mLevelLayouts[level] =
            std::make_shared<VkSubresourceLayout>();
        mImports.mVkDeviceFunctions[device].vkGetImageSubresourceLayout(
            device, image->mVulkanHandle, &subres,
            &*layer_i->second->mLevelLayouts[level]);
      });
  return layouts;
}

// Utility functions
uint32_t numberOfPNext(void* pNext) {
  uint32_t counter = 0;
  while (pNext) {
    counter++;
    pNext = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t*>(pNext)[1]);
  }
  return counter;
}

void notifyPendingCommandAdded(VkQueue) {}

void vkErrInvalidHandle(std::string handleType, uint64_t handle){
    GAPID_WARNING("Error: Invalid %s: %" PRIu64, handleType.c_str(), handle)
}

void vkErrNullPointer(std::string pointerType) {
    GAPID_WARNING("Error: Null pointer of %s", pointerType.c_str())
}

void vkErrNotNullPointer(std::string pointerType) {
    GAPID_WARNING("Error: Not Null Pointer: %s", pointerType.c_str());
}

void vkErrUnrecognizedExtension(std::string name) {
    GAPID_WARNING("Error: Unrecognized extension: %s", name.c_str())
}

void vkErrExpectNVDedicatedlyAllocatedHandle(std::string handleType, uint64_t handle) {
    GAPID_WARNING("Error: Expected handle that was allocated with a dedicated allocation: %s: %" PRIu64, handleType.c_str(), handle)
}

void vkErrInvalidDescriptorArrayElement(uint64_t set, uint32_t binding, uint32_t array_index) {
  GAPID_WARNING("Error: Invalid descriptor array element specified by descriptor set: %" PRIu64 ", binding: %" PRIu32 ", array index: %" PRIu32, set, binding, array_index);
}

void vkErrCommandBufferIncomplete(VkCommandBuffer cmdbuf) {
    GAPID_WARNING("Error: Executing command buffer %zu was not in the COMPLETED state", cmdbuf)
}

void vkErrInvalidImageLayout(VkImage img, uint32_t aspect, uint32_t layer, uint32_t level, uint32_t layout, uint32_t expectedLayout) {
    GAPID_WARNING("Error: Image subresource at Image: %" PRIu64 ", AspectBit: %" PRIu32 ", Layer: %" PRIu32 ", Level: %" PRIu32 " was in layout %" PRIu32 ", but was expected to be in layout %" PRIu32,
        img, aspect, layer, level, layout, expectedLayout);
}

void vkErrInvalidImageSubresource(VkImage img, std::string subresourceType, uint32_t value) {
    GAPID_WARNING("Error: Accessing invalid image subresource at Image: %" PRIu64 ", %s: %" PRIu32, img, subresourceType.c_str(), value);
}

}  // namespace gapii

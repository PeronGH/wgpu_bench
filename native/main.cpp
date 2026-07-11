#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr VkDeviceSize kMiB = 1024 * 1024;
constexpr uint32_t kWorkgroupSize = 256;

void check(VkResult result, const char *operation) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string(operation) + " failed (VkResult " +
                             std::to_string(result) + ")");
  }
}

uint32_t positive_integer(const char *value, uint32_t fallback,
                          const char *name) {
  if (value == nullptr)
    return fallback;
  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value, &end, 10);
  if (*value == '\0' || *end != '\0' || parsed == 0 ||
      parsed > std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error(std::string(name) + " must be a positive integer");
  }
  return static_cast<uint32_t>(parsed);
}

std::vector<uint32_t> read_spirv() {
  std::ifstream input(SHADER_PATH, std::ios::binary | std::ios::ate);
  if (!input)
    throw std::runtime_error("Could not open " SHADER_PATH);
  const auto length = input.tellg();
  if (length <= 0 || length % 4 != 0)
    throw std::runtime_error("Invalid SPIR-V");
  std::vector<uint32_t> code(static_cast<size_t>(length) / sizeof(uint32_t));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(code.data()), length);
  if (!input)
    throw std::runtime_error("Could not read SPIR-V");
  return code;
}

uint32_t memory_type(const VkPhysicalDeviceMemoryProperties &properties,
                     uint32_t allowed, VkMemoryPropertyFlags required) {
  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if ((allowed & (1U << i)) != 0 &&
        (properties.memoryTypes[i].propertyFlags & required) == required) {
      return i;
    }
  }
  throw std::runtime_error("No suitable device-local memory type");
}

struct Buffer {
  VkBuffer handle = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
};

Buffer create_buffer(VkDevice device,
                     const VkPhysicalDeviceMemoryProperties &memory,
                     VkDeviceSize size) {
  Buffer buffer;
  const VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                nullptr,
                                0,
                                size,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                VK_SHARING_MODE_EXCLUSIVE,
                                0,
                                nullptr};
  check(vkCreateBuffer(device, &info, nullptr, &buffer.handle),
        "vkCreateBuffer");
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(device, buffer.handle, &requirements);
  const VkMemoryAllocateInfo allocation{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, requirements.size,
      memory_type(memory, requirements.memoryTypeBits,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
  check(vkAllocateMemory(device, &allocation, nullptr, &buffer.memory),
        "vkAllocateMemory");
  check(vkBindBufferMemory(device, buffer.handle, buffer.memory, 0),
        "vkBindBufferMemory");
  return buffer;
}
} // namespace

int main(int argc, char **argv) {
  try {
    const uint32_t size_mib =
        positive_integer(argc > 1 ? argv[1] : nullptr, 128, "sizeMiB");
    const uint32_t iterations =
        positive_integer(argc > 2 ? argv[2] : nullptr, 20, "iterations");
    const uint32_t bursts =
        positive_integer(argc > 3 ? argv[3] : nullptr, 10, "bursts");

    const VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                nullptr,
                                "Vulkan bandwidth",
                                1,
                                nullptr,
                                0,
                                VK_API_VERSION_1_1};
    const VkInstanceCreateInfo instance_info{
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        nullptr,
        0,
        &app,
        0,
        nullptr,
        0,
        nullptr};
    VkInstance instance;
    check(vkCreateInstance(&instance_info, nullptr, &instance),
          "vkCreateInstance");

    uint32_t device_count = 0;
    check(vkEnumeratePhysicalDevices(instance, &device_count, nullptr),
          "vkEnumeratePhysicalDevices");
    if (device_count == 0)
      throw std::runtime_error("No Vulkan device found");
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    check(vkEnumeratePhysicalDevices(instance, &device_count,
                                     physical_devices.data()),
          "vkEnumeratePhysicalDevices");
    const VkPhysicalDevice physical = physical_devices.front();

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceProperties(physical, &properties);
    vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count,
                                             families.data());
    uint32_t family = family_count;
    for (uint32_t i = 0; i < family_count; ++i) {
      if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
          families[i].timestampValidBits != 0) {
        family = i;
        break;
      }
    }
    if (family == family_count)
      throw std::runtime_error("No timestamp-capable compute queue");

    constexpr float priority = 1.0F;
    const VkDeviceQueueCreateInfo queue_info{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        0,
        family,
        1,
        &priority};
    const VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                         nullptr,
                                         0,
                                         1,
                                         &queue_info,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr};
    VkDevice device;
    check(vkCreateDevice(physical, &device_info, nullptr, &device),
          "vkCreateDevice");
    VkQueue queue;
    vkGetDeviceQueue(device, family, 0, &queue);

    const VkDeviceSize requested = static_cast<VkDeviceSize>(size_mib) * kMiB;
    const VkDeviceSize invocation_bytes = 16;
    const VkDeviceSize workgroup_bytes = invocation_bytes * kWorkgroupSize;
    const VkDeviceSize max_dispatch_bytes =
        static_cast<VkDeviceSize>(
            properties.limits.maxComputeWorkGroupCount[0]) *
        workgroup_bytes;
    const VkDeviceSize size = std::min(
        {requested,
         static_cast<VkDeviceSize>(properties.limits.maxStorageBufferRange),
         max_dispatch_bytes});
    const VkDeviceSize aligned_size = size / workgroup_bytes * workgroup_bytes;
    if (aligned_size == 0)
      throw std::runtime_error("Requested buffer is too small");
    const uint32_t workgroups =
        static_cast<uint32_t>(aligned_size / workgroup_bytes);

    Buffer source = create_buffer(device, memory_properties, aligned_size);
    Buffer destination = create_buffer(device, memory_properties, aligned_size);

    const std::array<VkDescriptorSetLayoutBinding, 2> bindings{{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
         nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
         nullptr},
    }};
    const VkDescriptorSetLayoutCreateInfo layout_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
        static_cast<uint32_t>(bindings.size()), bindings.data()};
    VkDescriptorSetLayout descriptor_layout;
    check(vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
                                      &descriptor_layout),
          "vkCreateDescriptorSetLayout");
    const VkPipelineLayoutCreateInfo pipeline_layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr,
        0,
        1,
        &descriptor_layout,
        0,
        nullptr};
    VkPipelineLayout pipeline_layout;
    check(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                                 &pipeline_layout),
          "vkCreatePipelineLayout");

    const auto code = read_spirv();
    const VkShaderModuleCreateInfo shader_info{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
        code.size() * 4, code.data()};
    VkShaderModule shader;
    check(vkCreateShaderModule(device, &shader_info, nullptr, &shader),
          "vkCreateShaderModule");
    const VkPipelineShaderStageCreateInfo stage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_COMPUTE_BIT,
        shader,
        "main",
        nullptr};
    const VkComputePipelineCreateInfo pipeline_info{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr,
        0,
        stage,
        pipeline_layout,
        VK_NULL_HANDLE,
        0};
    VkPipeline pipeline;
    check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                   nullptr, &pipeline),
          "vkCreateComputePipelines");

    const VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
    const VkDescriptorPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        nullptr,
        0,
        1,
        1,
        &pool_size};
    VkDescriptorPool descriptor_pool;
    check(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool),
          "vkCreateDescriptorPool");
    const VkDescriptorSetAllocateInfo set_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
        descriptor_pool, 1, &descriptor_layout};
    VkDescriptorSet descriptor_set;
    check(vkAllocateDescriptorSets(device, &set_info, &descriptor_set),
          "vkAllocateDescriptorSets");
    const std::array<VkDescriptorBufferInfo, 2> buffer_infos{
        {{source.handle, 0, aligned_size},
         {destination.handle, 0, aligned_size}}};
    const std::array<VkWriteDescriptorSet, 2> writes{{
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0, 0,
         1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer_infos[0],
         nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 1, 0,
         1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &buffer_infos[1],
         nullptr},
    }};
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);

    const VkQueryPoolCreateInfo query_info{
        VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        nullptr,
        0,
        VK_QUERY_TYPE_TIMESTAMP,
        bursts * 2,
        0};
    VkQueryPool query_pool;
    check(vkCreateQueryPool(device, &query_info, nullptr, &query_pool),
          "vkCreateQueryPool");
    const VkCommandPoolCreateInfo command_pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, family};
    VkCommandPool command_pool;
    check(
        vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool),
        "vkCreateCommandPool");
    std::vector<VkCommandBuffer> commands(bursts);
    const VkCommandBufferAllocateInfo command_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, command_pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, bursts};
    check(vkAllocateCommandBuffers(device, &command_info, commands.data()),
          "vkAllocateCommandBuffers");

    const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                  VK_ACCESS_SHADER_WRITE_BIT,
                                  VK_ACCESS_SHADER_WRITE_BIT};
    for (uint32_t burst = 0; burst < bursts; ++burst) {
      const VkCommandBufferBeginInfo begin{
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
      check(vkBeginCommandBuffer(commands[burst], &begin),
            "vkBeginCommandBuffer");
      vkCmdResetQueryPool(commands[burst], query_pool, burst * 2, 2);
      vkCmdBindPipeline(commands[burst], VK_PIPELINE_BIND_POINT_COMPUTE,
                        pipeline);
      vkCmdBindDescriptorSets(commands[burst], VK_PIPELINE_BIND_POINT_COMPUTE,
                              pipeline_layout, 0, 1, &descriptor_set, 0,
                              nullptr);
      vkCmdWriteTimestamp(commands[burst], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          query_pool, burst * 2);
      for (uint32_t i = 0; i < iterations; ++i) {
        vkCmdDispatch(commands[burst], workgroups, 1, 1);
        if (i + 1 < iterations) {
          vkCmdPipelineBarrier(commands[burst],
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                               &barrier, 0, nullptr, 0, nullptr);
        }
      }
      vkCmdWriteTimestamp(commands[burst], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          query_pool, burst * 2 + 1);
      check(vkEndCommandBuffer(commands[burst]), "vkEndCommandBuffer");
    }

    std::vector<double> bandwidths;
    std::vector<double> elapsed_ms;
    bandwidths.reserve(bursts);
    elapsed_ms.reserve(bursts);
    for (uint32_t burst = 0; burst < bursts; ++burst) {
      const VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                nullptr,
                                0,
                                nullptr,
                                nullptr,
                                1,
                                &commands[burst],
                                0,
                                nullptr};
      check(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
      uint64_t timestamps[2];
      check(vkGetQueryPoolResults(
                device, query_pool, burst * 2, 2, sizeof(timestamps),
                timestamps, sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
            "vkGetQueryPoolResults");
      const uint64_t mask =
          families[family].timestampValidBits == 64
              ? std::numeric_limits<uint64_t>::max()
              : (uint64_t{1} << families[family].timestampValidBits) - 1;
      const uint64_t ticks = (timestamps[1] - timestamps[0]) & mask;
      const double ms =
          static_cast<double>(ticks) * properties.limits.timestampPeriod / 1e6;
      const double bytes = static_cast<double>(aligned_size) * iterations * 2.0;
      elapsed_ms.push_back(ms);
      bandwidths.push_back(bytes / (ms * 1e6));
    }

    const auto peak_it = std::max_element(bandwidths.begin(), bandwidths.end());
    const size_t peak_index = static_cast<size_t>(peak_it - bandwidths.begin());
    auto sorted = bandwidths;
    std::sort(sorted.begin(), sorted.end());
    const double median =
        bursts % 2 == 0 ? (sorted[bursts / 2 - 1] + sorted[bursts / 2]) / 2.0
                        : sorted[bursts / 2];
    std::cout << std::fixed << std::setprecision(2)
              << "Adapter:      " << properties.deviceName << '\n'
              << "Buffer size:  " << static_cast<double>(aligned_size) / kMiB
              << " MiB x 2\n"
              << "Iterations:   " << iterations << " per burst\n"
              << "Bursts:       " << bursts << '\n'
              << "Best elapsed: " << elapsed_ms[peak_index] << " ms\n"
              << "Median:       " << median << " GB/s\n"
              << "Peak:         " << *peak_it << " GB/s (read + write)\n";

    check(vkDeviceWaitIdle(device), "vkDeviceWaitIdle");
    vkDestroyCommandPool(device, command_pool, nullptr);
    vkDestroyQueryPool(device, query_pool, nullptr);
    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
    vkDestroyBuffer(device, destination.handle, nullptr);
    vkFreeMemory(device, destination.memory, nullptr);
    vkDestroyBuffer(device, source.handle, nullptr);
    vkFreeMemory(device, source.memory, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
  } catch (const std::exception &error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}

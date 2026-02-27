#include "gfx/Context.h"

#include "gfx/GlfwVulkan.h"
#include "gfx/Upload.h"

#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <new>

namespace gfx {

namespace {

constexpr char const* kValidationLayer = "VK_LAYER_KHRONOS_validation";

[[noreturn]] void fail(char const* msg) { throw std::runtime_error(msg); }
[[noreturn]] void fail(std::string const& msg) { throw std::runtime_error(msg); }

void vk_check(VkResult r, char const* what) {
  if (r == VK_SUCCESS) {
    return;
  }
  fail(std::string("Vulkan error: ") + what + " (" + std::to_string(static_cast<int>(r)) + ")");
}

bool has_extension(std::vector<char const*> const& exts, char const* name) {
  for (auto const* e : exts) {
    if (e != nullptr && std::strcmp(e, name) == 0) {
      return true;
    }
  }
  return false;
}

VkResult create_debug_utils_messenger_ext(
  VkInstance instance,
  VkDebugUtilsMessengerCreateInfoEXT const* create_info,
  VkAllocationCallbacks const* allocator,
  VkDebugUtilsMessengerEXT* out_messenger
) {
  auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
  );
  return (fn != nullptr)
    ? fn(instance, create_info, allocator, out_messenger)
    : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void destroy_debug_utils_messenger_ext(
  VkInstance instance,
  VkDebugUtilsMessengerEXT messenger,
  VkAllocationCallbacks const* allocator
) {
  auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
  );
  if (fn != nullptr) {
    fn(instance, messenger, allocator);
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT type,
  VkDebugUtilsMessengerCallbackDataEXT const* callback_data,
  void* user_data
) {
  (void)severity;
  (void)type;
  (void)user_data;

  if (callback_data != nullptr && callback_data->pMessage != nullptr) {
    std::fprintf(stderr, "VULKAN: %s\n", callback_data->pMessage);
  }
  return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT make_debug_messenger_create_info() {
  VkDebugUtilsMessengerCreateInfoEXT ci{};
  ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  ci.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  ci.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  ci.pfnUserCallback = debug_callback;
  ci.pUserData = nullptr;
  return ci;
}

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics;
  std::optional<uint32_t> present;

  bool complete() const {
    return graphics.has_value() && present.has_value();
  }
};

bool has_device_extension(VkPhysicalDevice pd, char const* name) {
  uint32_t count = 0;
  vk_check(vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties(count)");

  std::vector<VkExtensionProperties> props(count);
  vk_check(vkEnumerateDeviceExtensionProperties(pd, nullptr, &count, props.data()), "vkEnumerateDeviceExtensionProperties(list)");

  for (auto const& p : props) {
    if (std::strcmp(p.extensionName, name) == 0) {
      return true;
    }
  }
  return false;
}

QueueFamilyIndices find_queue_families(VkPhysicalDevice pd, VkSurfaceKHR surface) {
  QueueFamilyIndices indices{};

  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, nullptr);

  std::vector<VkQueueFamilyProperties> props(count);
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &count, props.data());

  for (uint32_t i = 0; i < count; ++i) {
    auto const& q = props[i];

    if ((q.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
      indices.graphics = i;
    }

    VkBool32 present_support = VK_FALSE;
    vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present_support), "vkGetPhysicalDeviceSurfaceSupportKHR");

    if (present_support == VK_TRUE) {
      indices.present = i;
    }

    if (indices.complete()) {
      break;
    }
  }

  return indices;
}

} // namespace

Context::~Context() {
  shutdown();
}

Context::Context(Context&& other) noexcept {
  *this = std::move(other);
}

Context& Context::operator=(Context&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  shutdown();

  instance_ = other.instance_;
  debug_messenger_ = other.debug_messenger_;
  surface_ = other.surface_;
  physical_device_ = other.physical_device_;
  device_ = other.device_;
  graphics_queue_ = other.graphics_queue_;
  present_queue_ = other.present_queue_;
  graphics_queue_family_ = other.graphics_queue_family_;
  present_queue_family_ = other.present_queue_family_;
  upload_ = other.upload_;

  other.instance_ = VK_NULL_HANDLE;
  other.debug_messenger_ = VK_NULL_HANDLE;
  other.surface_ = VK_NULL_HANDLE;
  other.physical_device_ = VK_NULL_HANDLE;
  other.device_ = VK_NULL_HANDLE;
  other.graphics_queue_ = VK_NULL_HANDLE;
  other.present_queue_ = VK_NULL_HANDLE;
  other.graphics_queue_family_ = UINT32_MAX;
  other.present_queue_family_ = UINT32_MAX;
  other.upload_ = nullptr;

  return *this;
}

void Context::init(GLFWwindow* window, ContextCreateInfo const& info) {
  if (window == nullptr) {
    fail("Context::init: window == nullptr");
  }

  create_instance(window, info);
  setup_debug(info);
  create_surface(window);
  pick_physical_device();
  create_device(info);

  // must be after device creation
  create_uploader();
}

void Context::shutdown() {
  // uploader uses device/queue -> destroy before vkDestroyDevice
  destroy_uploader();

  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }

  physical_device_ = VK_NULL_HANDLE;
  graphics_queue_ = VK_NULL_HANDLE;
  present_queue_ = VK_NULL_HANDLE;
  graphics_queue_family_ = UINT32_MAX;
  present_queue_family_ = UINT32_MAX;

  if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }

  if (debug_messenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
    destroy_debug_utils_messenger_ext(instance_, debug_messenger_, nullptr);
    debug_messenger_ = VK_NULL_HANDLE;
  }

  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
}

void Context::create_uploader() {
  if (upload_ != nullptr) {
    fail("Context::create_uploader called twice");
  }
  upload_ = new (std::nothrow) Upload();
  if (upload_ == nullptr) {
    fail("Context: Upload allocation failed");
  }
  upload_->init(*this);
}

void Context::destroy_uploader() {
  if (upload_ != nullptr) {
    if (device_ != VK_NULL_HANDLE) {
      upload_->shutdown(*this);
    }
    delete upload_;
    upload_ = nullptr;
  }
}

void Context::create_instance(GLFWwindow* window, ContextCreateInfo const& info) {
  (void)window;

  if (glfwVulkanSupported() == GLFW_FALSE) {
    fail("glfwVulkanSupported() == false");
  }

  uint32_t glfw_ext_count = 0;
  char const** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  if (glfw_exts == nullptr || glfw_ext_count == 0U) {
    fail("glfwGetRequiredInstanceExtensions() failed");
  }

  std::vector<char const*> instance_exts(glfw_exts, glfw_exts + glfw_ext_count);

  // portability enumeration (MoltenVK/macOS)
  if (!has_extension(instance_exts, "VK_KHR_portability_enumeration")) {
    instance_exts.push_back("VK_KHR_portability_enumeration");
  }

  if (info.enable_validation && info.enable_debug_utils && !has_extension(instance_exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    instance_exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  std::vector<char const*> layers;
  if (info.enable_validation) {
    layers.push_back(kValidationLayer);
  }

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "Renderer";
  app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app.pEngineName = "Renderer";
  app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &app;
  ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  ci.enabledExtensionCount = static_cast<uint32_t>(instance_exts.size());
  ci.ppEnabledExtensionNames = instance_exts.data();
  ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
  ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

  VkDebugUtilsMessengerCreateInfoEXT debug_ci{};
  if (info.enable_validation && info.enable_debug_utils) {
    debug_ci = make_debug_messenger_create_info();
    ci.pNext = &debug_ci;
  }

  vk_check(vkCreateInstance(&ci, nullptr, &instance_), "vkCreateInstance");
}

void Context::setup_debug(ContextCreateInfo const& info) {
  if (!(info.enable_validation && info.enable_debug_utils)) {
    return;
  }

  auto ci = make_debug_messenger_create_info();
  vk_check(
    create_debug_utils_messenger_ext(instance_, &ci, nullptr, &debug_messenger_),
    "vkCreateDebugUtilsMessengerEXT"
  );
}

void Context::create_surface(GLFWwindow* window) {
  vk_check(glfwCreateWindowSurface(instance_, window, nullptr, &surface_), "glfwCreateWindowSurface");
}

void Context::pick_physical_device() {
  uint32_t count = 0;
  vk_check(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices(count)");
  if (count == 0U) {
    fail("No Vulkan physical devices found");
  }

  std::vector<VkPhysicalDevice> devices(count);
  vk_check(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "vkEnumeratePhysicalDevices(list)");

  for (auto pd : devices) {
    auto const qf = find_queue_families(pd, surface_);
    if (!qf.complete()) {
      continue;
    }
    if (!has_device_extension(pd, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
      continue;
    }
    physical_device_ = pd;
    graphics_queue_family_ = qf.graphics.value();
    present_queue_family_ = qf.present.value();
    return;
  }

  fail("No suitable physical device found (need graphics+present and VK_KHR_swapchain)");
}

void Context::create_device(ContextCreateInfo const& info) {
  std::vector<char const*> device_exts;
  device_exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  {
    constexpr char const* kPortabilitySubsetExt = "VK_KHR_portability_subset";
    if (has_device_extension(physical_device_, kPortabilitySubsetExt)) {
      device_exts.push_back(kPortabilitySubsetExt);
    }
  }

  float const prio = 1.0f;

  std::unordered_set<uint32_t> unique_qf{graphics_queue_family_, present_queue_family_};

  std::vector<VkDeviceQueueCreateInfo> qcis;
  qcis.reserve(unique_qf.size());
  for (auto family : unique_qf) {
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    qcis.push_back(qci);
  }

  VkPhysicalDeviceFeatures features{};

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
  dci.pQueueCreateInfos = qcis.data();
  dci.pEnabledFeatures = &features;
  dci.enabledExtensionCount = static_cast<uint32_t>(device_exts.size());
  dci.ppEnabledExtensionNames = device_exts.data();

  if (info.enable_validation) {
    char const* layers_local[] = {kValidationLayer};
    dci.enabledLayerCount = 1;
    dci.ppEnabledLayerNames = layers_local;
  }

  vk_check(vkCreateDevice(physical_device_, &dci, nullptr, &device_), "vkCreateDevice");

  vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
  vkGetDeviceQueue(device_, present_queue_family_, 0, &present_queue_);

  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(physical_device_, &props);
  std::fprintf(stderr, "Selected GPU: %s\n", props.deviceName);
}

} // namespace gfx

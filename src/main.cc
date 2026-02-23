#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

[[noreturn]] void die(char const* msg) {
  std::fprintf(stderr, "%s\n", msg);
  std::fflush(stderr);
  std::exit(EXIT_FAILURE);
}

void vk_check(VkResult r, char const* what) {
  if (r == VK_SUCCESS) {
    return;
  }
  std::fprintf(stderr, "Vulkan error: %s (%d)\n", what, static_cast<int>(r));
  std::fflush(stderr);
  std::exit(EXIT_FAILURE);
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

} // namespace

int main() {
  glfwSetErrorCallback([](int code, char const* desc) {
    std::fprintf(stderr, "GLFW error (%d): %s\n", code, (desc != nullptr) ? desc : "(null)");
  });

  if (glfwInit() == GLFW_FALSE) {
    die("glfwInit() failed.");
  }

  if (glfwVulkanSupported() == GLFW_FALSE) {
    glfwTerminate();
    die("glfwVulkanSupported() == false (Vulkan not available).");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(1280, 720, "Renderer", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    die("glfwCreateWindow() failed.");
  }

  uint32_t glfw_ext_count = 0;
  char const** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  if (glfw_exts == nullptr || glfw_ext_count == 0U) {
    glfwDestroyWindow(window);
    glfwTerminate();
    die("glfwGetRequiredInstanceExtensions() failed.");
  }

  std::vector<char const*> instance_exts(glfw_exts, glfw_exts + glfw_ext_count);

  // MoltenVK / portability enumeration (macOS)
  if (!has_extension(instance_exts, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
    instance_exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  }

  bool const enable_validation = true;
  bool const enable_debug_utils = true;

  if (enable_validation && enable_debug_utils && !has_extension(instance_exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    instance_exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  std::vector<char const*> layers;
  if (enable_validation) {
    layers.push_back("VK_LAYER_KHRONOS_validation");
  }

  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pApplicationName = "GameEngine";
  app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app.pEngineName = "GameEngine";
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
  if (enable_validation && enable_debug_utils) {
    debug_ci = make_debug_messenger_create_info();
    ci.pNext = &debug_ci;
  }

  VkInstance instance = VK_NULL_HANDLE;
  vk_check(vkCreateInstance(&ci, nullptr, &instance), "vkCreateInstance");

  VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
  if (enable_validation && enable_debug_utils) {
    auto dci = make_debug_messenger_create_info();
    vk_check(
      create_debug_utils_messenger_ext(instance, &dci, nullptr, &debug_messenger),
      "vkCreateDebugUtilsMessengerEXT"
    );
  }

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  vk_check(glfwCreateWindowSurface(instance, window, nullptr, &surface), "glfwCreateWindowSurface");

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();
  }

  if (surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance, surface, nullptr);
  }

  if (debug_messenger != VK_NULL_HANDLE) {
    destroy_debug_utils_messenger_ext(instance, debug_messenger, nullptr);
  }

  vkDestroyInstance(instance, nullptr);
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}

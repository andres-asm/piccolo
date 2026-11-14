// piccolo for Android, step 1: open the window and report what Android gives us.
//
// SDLActivity (Java) starts a thread and calls SDL_main() here; SDL_main.h
// renames main() to SDL_main on Android. Nothing is drawn yet: the point is
// that the activity opens, the window exists, and the log (adb logcat -s piccolo)
// shows the window's pixel size next to the display modes. On Android TV that
// tells us whether we get a 1080p window on a 4K display.
// It also logs each Vulkan device's version and the extensions piccolo's
// renderer depends on (it's written for Vulkan 1.3 on desktop).

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <android/log.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <vector>

static const char* LOG_TAG = "piccolo";

static void log_display_modes(SDL_DisplayID display)
{
    const SDL_DisplayMode* current = SDL_GetCurrentDisplayMode(display);
    if (current != nullptr) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "current display mode: %dx%d @ %.3f Hz, density %.2f",
                            current->w, current->h, static_cast<double>(current->refresh_rate),
                            static_cast<double>(current->pixel_density));
    }

    int32_t mode_count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &mode_count);
    if (modes == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "SDL_GetFullscreenDisplayModes failed: %s", SDL_GetError());
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%d fullscreen display modes:", mode_count);
    for (int32_t i = 0; i < mode_count; i++) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "  %dx%d @ %.3f Hz", modes[i]->w, modes[i]->h,
                            static_cast<double>(modes[i]->refresh_rate));
    }
    SDL_free(modes);
}

static void log_vulkan_version(const char* what, uint32_t version)
{
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s: Vulkan %u.%u.%u", what, VK_API_VERSION_MAJOR(version),
                        VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));
}

static bool has_extension(const std::vector<VkExtensionProperties>& extensions, const char* name)
{
    for (size_t i = 0; i < extensions.size(); i++) {
        if (std::strcmp(extensions[i].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

// What the device offers, compared with what the desktop renderer uses.
// Diagnostic only: the instance is destroyed again right away.
static void log_vulkan_devices()
{
    // Vulkan 1.1 function: libvulkan.so for API 26 only exports 1.0, so ask the loader.
    uint32_t instance_version = VK_API_VERSION_1_0;
    PFN_vkEnumerateInstanceVersion enumerate_instance_version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerate_instance_version != nullptr) {
        enumerate_instance_version(&instance_version);
    }
    log_vulkan_version("loader", instance_version);

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "piccolo",
        .apiVersion = instance_version,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "vkCreateInstance failed: %d", static_cast<int32_t>(result));
        return;
    }

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%u Vulkan devices", device_count);

    const char* wanted[] = {
        "VK_KHR_dynamic_rendering",
        "VK_KHR_synchronization2",
        "VK_KHR_timeline_semaphore",
        "VK_KHR_maintenance4",
        "VK_KHR_present_id",
        "VK_KHR_present_wait",
        "VK_EXT_swapchain_maintenance1",
        "VK_GOOGLE_display_timing",
    };

    for (uint32_t d = 0; d < device_count; d++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[d], &properties);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "device %u: %s, driver 0x%08x", d, properties.deviceName,
                            properties.driverVersion);
        log_vulkan_version("  device", properties.apiVersion);

        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(devices[d], nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(devices[d], nullptr, &extension_count, extensions.data());
        for (size_t w = 0; w < sizeof(wanted) / sizeof(wanted[0]); w++) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "  %-32s %s", wanted[w],
                                has_extension(extensions, wanted[w]) ? "yes" : "NO");
        }
    }

    vkDestroyInstance(instance, nullptr);
}

static void log_window_size(SDL_Window* window, const char* reason)
{
    int32_t width = 0;
    int32_t height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "window size in pixels (%s): %dx%d", reason, width, height);
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "piccolo starting");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Vulkan window from the start: that's what piccolo renders with.
    SDL_Window* window = SDL_CreateWindow("piccolo", 0, 0, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
    if (window == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    log_display_modes(SDL_GetDisplayForWindow(window));
    log_vulkan_devices();
    log_window_size(window, "created");

    bool running = true;
    while (running) {
        SDL_Event event;
        // Block: nothing to draw yet, so don't spin while waiting for events.
        if (!SDL_WaitEvent(&event)) {
            continue;
        }
        switch (event.type) {
        case SDL_EVENT_QUIT:
            running = false;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            log_window_size(window, "changed");
            break;
        case SDL_EVENT_DID_ENTER_BACKGROUND:
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "entered background");
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "entered foreground");
            break;
        default:
            break;
        }
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "piccolo quitting");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

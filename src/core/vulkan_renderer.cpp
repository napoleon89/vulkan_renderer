#define MAX_FRAMES_IN_FLIGHT 2


internal_func VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
	printf("validation layer: %s\n", data->pMessage);
    return VK_FALSE;
}

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR surface_capabilities;
	VkSurfaceFormatKHR *formats;
	u32 format_count;
	VkPresentModeKHR *present_modes;
	u32 present_mode_count;	
	
	VkSurfaceFormatKHR chooseFormat() {
		if(format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
			return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
		}
		
		for(u32 i = 0; i < format_count; i++) {
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return formats[i];
			}
		}
		
		return formats[0];
	}
	
	VkPresentModeKHR choosePresentMode() {
		VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR; // NOTE(nathan): guarenteed on all platforms
		
		for(u32 i = 0; i < present_mode_count; i++) {
			if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) 
				return present_modes[i];
			else if(present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
				best_mode = present_modes[i];
		}
		return best_mode; 
	}
	
	VkExtent2D chooseExtent(SDL_Window *window) {
		if(surface_capabilities.currentExtent.width != UINT32_MAX) {
			return surface_capabilities.currentExtent;
		} else {
			int w, h;
			SDL_Vulkan_GetDrawableSize(window, &w, &h);
			VkExtent2D result = {};
			result.width = w;
			result.height = h;
			return result;
		}
	}
};

internal_func SwapChainSupportDetails querySwapChainSupport(Platform *platform, const VkPhysicalDevice &device, const VkSurfaceKHR &surface) {
	SwapChainSupportDetails result = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result.surface_capabilities);
	
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &result.format_count, 0);
	result.formats = (VkSurfaceFormatKHR *)platform->alloc(sizeof(VkSurfaceFormatKHR) * result.format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &result.format_count, result.formats);
	
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &result.present_mode_count, 0);
	result.present_modes = (VkPresentModeKHR *)platform->alloc(sizeof(VkPresentModeKHR) * result.present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &result.present_mode_count, result.present_modes);
	
	return result;
}

internal_func VkShaderModule createShaderModule(Platform *platform, const VkDevice &device, const char *filename) {
	FileData frag_file = platform->readEntireFile(filename);
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = frag_file.size;
	create_info.pCode = (u32 *)frag_file.contents;
	VkShaderModule result;
	if(vkCreateShaderModule(device, &create_info, 0, &result) != VK_SUCCESS) {
		platform->error(formatString("Couldn't create shader %s\n", filename));
	}
	platform->free(frag_file.contents);
	return result;
}

struct Vertex {
	Vec3 pos;
	Vec3 color;	
	Vec2 uv;
};

struct UniformBufferObject {
	Mat4 model;
	Mat4 view;
	Mat4 projection;	
};

struct VulkanRenderer {
	VkDevice device;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkSwapchainKHR swap_chain;
	VkRenderPass render_pass;
	VkPipeline graphics_pipeline;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	SwapChainSupportDetails swap_chain_details;
	VkImageView *swap_image_views;
	VkFramebuffer *swap_chain_frame_buffers;
	VkCommandBuffer *command_buffers;
	VkSurfaceFormatKHR surface_format;
	VkCommandPool command_pool;
	VkExtent2D extent;
	
	VkImage depth_image;
	VkDeviceMemory depth_image_memory;
	VkImageView depth_image_view;
	
	VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

	VkQueue graphics_queue;
	VkQueue present_queue;
	
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_pool;
	VkPipelineLayout pipeline_layout;
	
	VkDebugUtilsMessengerEXT debug_callback;
	
	s32 graphics_queue_index;
	s32 present_queue_index;
	u32 swap_image_count;
	
	u32 current_frame = 0;
	
	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;
	
	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;
	
	VkImage texture_image;
	VkDeviceMemory texture_image_memory;
	VkImageView texture_image_view;
	VkSampler texture_sampler;
	
	VkBuffer *uniform_buffers;
	VkDeviceMemory *uniform_buffers_memory;
	
	VkDescriptorSet *descriptor_sets;
	
	Vertex *vertices;
	u32 vertex_count;
	
	u32 *indices;
	u32 index_count;
	
	char *wanted_layers[1] = {
		"VK_LAYER_LUNARG_standard_validation",	
	};
	
	char *device_extensions[1] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	
	void createInstance(Platform *platform, PlatformWindow *window) {
		SDL_Window *sdl_window = (SDL_Window *)window->handle;
		u32 extension_count = 0;
		if(!SDL_Vulkan_GetInstanceExtensions(sdl_window, &extension_count, 0)) 
			platform->error("Couldn't get instance extensions");
		const char **extensions = (const char **)platform->alloc(sizeof(const char *) * extension_count+1);
		extensions[0] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		
		if(!SDL_Vulkan_GetInstanceExtensions(sdl_window, &extension_count, extensions+1)) 
			platform->error("Couldn't get instance extensions");
		
		extension_count += 1;
		
		printf("=========== Extensions ===========\n");
		for(u32 i = 0; i < extension_count; i++) {
			printf("%s\n", extensions[i]);
		}
		
		VkApplicationInfo vk_app_info = {};
		vk_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		vk_app_info.pApplicationName = "Engine22";
		vk_app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		vk_app_info.pEngineName = "Engine22";
		vk_app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		vk_app_info.apiVersion = VK_API_VERSION_1_0;
		
		u32 vk_wanted_layer_count = ArrayCount(wanted_layers);
		
		u32 vk_valid_layer_count = 0;
		vkEnumerateInstanceLayerProperties(&vk_valid_layer_count, 0);
		VkLayerProperties *vk_layers = (VkLayerProperties *)platform->alloc(sizeof(VkLayerProperties) * vk_valid_layer_count);
		vkEnumerateInstanceLayerProperties(&vk_valid_layer_count, vk_layers);
		
		
		printf("=========== Valid Layers ===========\n");
		for(u32 l = 0; l < vk_wanted_layer_count; l++) {
			const char *wanted_layer = wanted_layers[l];
			bool layer_found = false;
			for(u32 i = 0; i < vk_valid_layer_count; i++) {
				const char *vk_layer = vk_layers[i].layerName;
				if(strcmp(wanted_layer, vk_layer) == 0) {
					layer_found = true;
					break;
				}
			}	
			
			if(!layer_found) {
				platform->error(formatString("Couldn't find %s", wanted_layer));
			} else {
				printf("Found %s\n", wanted_layer);
			}
		}
		
		VkInstanceCreateInfo vk_create_info = {};
		vk_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		vk_create_info.enabledExtensionCount = extension_count;
		vk_create_info.ppEnabledExtensionNames = &extensions[0];
		vk_create_info.pApplicationInfo = &vk_app_info;
		vk_create_info.enabledLayerCount = ArrayCount(wanted_layers);
		vk_create_info.ppEnabledLayerNames = &wanted_layers[0];
		
		if(vkCreateInstance(&vk_create_info, 0, &instance) != VK_SUCCESS) 
			platform->error("Couldn't create vulkan instance");
	}
	
	void setupDebugUtils(Platform *platform) {
		VkDebugUtilsMessengerCreateInfoEXT vk_debug_utils_create_info = {};
		vk_debug_utils_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		vk_debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		vk_debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		vk_debug_utils_create_info.pfnUserCallback = debugCallback;
		vk_debug_utils_create_info.pUserData = 0; 
		
		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		
		if(vkCreateDebugUtilsMessengerEXT(instance, &vk_debug_utils_create_info, 0, &debug_callback) != VK_SUCCESS) 
			platform->error("Coulnd't create debug utils messenger");		
	}
	
	void createSurface(Platform *platform, PlatformWindow *window) {
		SDL_Window *sdl_window = (SDL_Window *)window->handle;
		if(!SDL_Vulkan_CreateSurface(sdl_window, instance, &surface)) 
			platform->error("Couldn't create vulkan surface");
	}
	
	void pickPhysicalDevice(Platform *platform) {
		u32 physical_device_count = 0;
		vkEnumeratePhysicalDevices(instance, &physical_device_count, 0);
		if(physical_device_count == 0) {
			platform->error("Failed to find gpus with vulkan support");
		}
		
		VkPhysicalDevice *physical_devices = (VkPhysicalDevice *)platform->alloc(sizeof(VkPhysicalDevice) * physical_device_count);
		vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);
		
		u32 wanted_device_extensions = ArrayCount(device_extensions);
		
		printf("=========== GPUS ===========\n");
		for(u32 i = 0; i < physical_device_count; i++) {
			VkPhysicalDevice &phys_device = physical_devices[i];
			bool suitable = true;
			VkPhysicalDeviceProperties device_props;
			vkGetPhysicalDeviceProperties(phys_device, &device_props);
			printf("%s\n", device_props.deviceName);
			
			VkPhysicalDeviceFeatures device_features;
			vkGetPhysicalDeviceFeatures(phys_device, &device_features);
			
			u32 device_extension_count = 0;
			vkEnumerateDeviceExtensionProperties(phys_device, 0, &device_extension_count, 0);
			
			VkExtensionProperties *available_extensions = (VkExtensionProperties *)platform->alloc(sizeof(VkExtensionProperties) * device_extension_count);
			vkEnumerateDeviceExtensionProperties(phys_device, 0, &device_extension_count, available_extensions);
			
			bool *extension_available = (bool *)platform->alloc(sizeof(bool) * wanted_device_extensions);
			
			for(u32 e = 0; e < device_extension_count; e++) {
				for(u32 de = 0; de < wanted_device_extensions; de++)
					if(strcmp(device_extensions[de], available_extensions[e].extensionName) == 0)
						extension_available[de] = true;
			}
			
			bool found_all_exts = true;
			for(u32 de = 0; de < wanted_device_extensions; de++) {
				if(!extension_available[de]) {
					found_all_exts = false;	
					printf("Couldn't find %s\n", device_extensions[de]);
					break;
				} 
				printf("Found %s\n", device_extensions[de]);
			}
			
			suitable = found_all_exts && device_features.samplerAnisotropy;
			
			if(suitable) {
				SwapChainSupportDetails details = querySwapChainSupport(platform, phys_device, surface);
				suitable = details.format_count > 0 && details.present_mode_count > 0;	
				if(suitable) swap_chain_details = details;
			}
			
			if(suitable) {
				
				physical_device = phys_device;
				break;
			}
		}
		
		if(physical_device == VK_NULL_HANDLE) {
			platform->error("Failed to find suitable GPU!");
		} else {
			VkPhysicalDeviceProperties device_props;
			vkGetPhysicalDeviceProperties(physical_device, &device_props);
			printf("Chosen %s\n", device_props.deviceName);
		}
	}
	
	void pickQueues(Platform *platform) {
		u32 vk_queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &vk_queue_family_count, 0);
		
		VkQueueFamilyProperties *vk_queue_families = (VkQueueFamilyProperties *)platform->alloc(sizeof(VkQueueFamilyProperties) * vk_queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &vk_queue_family_count, vk_queue_families);
		
		graphics_queue_index = -1;
		present_queue_index = -1;
		
		for(u32 i = 0; i < vk_queue_family_count; i++) {
			VkQueueFamilyProperties &queue_family = vk_queue_families[i];
			if(graphics_queue_index == -1 && queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphics_queue_index = (s32)i;
				
			}
			
			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
			if(present_queue_index == -1 && queue_family.queueCount > 0 && present_support) {
				present_queue_index = (s32)i;
				
			}
		}
		
		printf("Using graphics queue %d\n", graphics_queue_index);
		printf("Using present queue %d\n", present_queue_index);
	}
	
	void createDevice(Platform *platform) {
		f32 queue_priority = 1.0f;
	
		u32 queue_indices[] = {
			(u32)graphics_queue_index,
			(u32)present_queue_index
		};
		
		u32 queue_indices_count = ArrayCount(queue_indices);
		
		VkDeviceQueueCreateInfo *vk_queue_create_infos = (VkDeviceQueueCreateInfo *)platform->alloc(sizeof(VkDeviceQueueCreateInfo) * queue_indices_count);
		u32 queue_unique_count = 0;
		for(u32 i = 0; i < queue_indices_count; i++) {
			bool been_processed = false;
			for(u32 u = 0; u < i; u++) {
				if(queue_indices[i] == queue_indices[u]) {
					been_processed = true; 
					break;
				}
			}
			
			if(!been_processed) {
				
				VkDeviceQueueCreateInfo vk_queue_create_info = {};
				vk_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				vk_queue_create_info.queueFamilyIndex = queue_indices[i];
				vk_queue_create_info.queueCount = 1;
				vk_queue_create_info.pQueuePriorities = &queue_priority;
				vk_queue_create_infos[queue_unique_count] = vk_queue_create_info;				
				queue_unique_count++;
			}
		}
		
		VkPhysicalDeviceFeatures device_features = {};
		device_features.samplerAnisotropy = VK_TRUE;
		
		VkDeviceCreateInfo device_create_info = {};
		device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_create_info.pQueueCreateInfos = vk_queue_create_infos;
		device_create_info.queueCreateInfoCount = queue_unique_count;
		device_create_info.pEnabledFeatures = &device_features;
		device_create_info.enabledLayerCount = ArrayCount(wanted_layers);
		device_create_info.ppEnabledLayerNames = &wanted_layers[0];
		device_create_info.ppEnabledExtensionNames = device_extensions;
		device_create_info.enabledExtensionCount = ArrayCount(device_extensions);
		
		if(vkCreateDevice(physical_device, &device_create_info, 0, &device) != VK_SUCCESS) {
			platform->error("Couldn't create logical device");
		}
	}
	
	void createQueues() {
		vkGetDeviceQueue(device, graphics_queue_index, 0, &graphics_queue);
		vkGetDeviceQueue(device, present_queue_index, 0, &present_queue);
	}
	
	void createSwapChain(Platform *platform, PlatformWindow *window) {
		SDL_Window *sdl_window = (SDL_Window *)window->handle;
		swap_chain_details = querySwapChainSupport(platform, physical_device, surface);
		
		surface_format = swap_chain_details.chooseFormat();
		VkPresentModeKHR present_mode = swap_chain_details.choosePresentMode();
		extent = swap_chain_details.chooseExtent(sdl_window);
		u32 image_count = swap_chain_details.surface_capabilities.minImageCount+1;
		if(swap_chain_details.surface_capabilities.maxImageCount > 0 && image_count > swap_chain_details.surface_capabilities.maxImageCount) 
			image_count = swap_chain_details.surface_capabilities.maxImageCount;
		
		VkSwapchainCreateInfoKHR vk_swap_chain_create_info = {};
		vk_swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		vk_swap_chain_create_info.surface = surface;
		vk_swap_chain_create_info.minImageCount = image_count;
		vk_swap_chain_create_info.imageFormat = surface_format.format;
		vk_swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
		vk_swap_chain_create_info.imageExtent = extent;
		vk_swap_chain_create_info.imageArrayLayers = 1;
		vk_swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		
		if(graphics_queue_index != present_queue_index) {
			u32 queue_indices[] = {
				(u32)graphics_queue_index,
				(u32)present_queue_index
			};
			
			vk_swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			vk_swap_chain_create_info.queueFamilyIndexCount = 2;
			vk_swap_chain_create_info.pQueueFamilyIndices = queue_indices;
		} else {
			vk_swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		
		vk_swap_chain_create_info.preTransform = swap_chain_details.surface_capabilities.currentTransform;
		vk_swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		vk_swap_chain_create_info.presentMode = present_mode;
		vk_swap_chain_create_info.clipped = VK_TRUE;
		vk_swap_chain_create_info.oldSwapchain = VK_NULL_HANDLE;
		
		
		if(vkCreateSwapchainKHR(device, &vk_swap_chain_create_info, 0, &swap_chain) != VK_SUCCESS) 
			platform->error("Couldn't create swap chain");
		
	}
	
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, Platform *platform) {
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = image;
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = format;
		create_info.subresourceRange.aspectMask = aspect_flags;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;
		
		VkImageView result;
		if(vkCreateImageView(device, &create_info, 0, &result) != VK_SUCCESS) {
			platform->error("Couldn't create texture image view");
		}
		
		return result;
	}
	
	void createImageViews(Platform *platform) {	
		swap_image_count = 0;
		vkGetSwapchainImagesKHR(device, swap_chain, &swap_image_count, 0);
		VkImage *vk_swap_images = (VkImage *)platform->alloc(sizeof(VkImage) * swap_image_count);
		vkGetSwapchainImagesKHR(device, swap_chain, &swap_image_count, vk_swap_images);
		
		swap_image_views = (VkImageView *)platform->alloc(sizeof(VkImageView) * swap_image_count);
		for(u32 i = 0; i < swap_image_count; i++) {
			swap_image_views[i] = createImageView(vk_swap_images[i], surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT, platform);
		}
	}
	
	void createRenderPass(Platform *platform) {
		VkAttachmentDescription vk_color_attach_desc = {};
		vk_color_attach_desc.format = surface_format.format;
		vk_color_attach_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		vk_color_attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		vk_color_attach_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		vk_color_attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		vk_color_attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		vk_color_attach_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vk_color_attach_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		
		VkAttachmentDescription depth_attach_desc = {};
		depth_attach_desc.format = findDepthFormat(platform);
		depth_attach_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attach_desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attach_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attach_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		VkAttachmentReference vk_color_attach_ref = {};
		vk_color_attach_ref.attachment = 0;
		vk_color_attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		VkAttachmentReference depth_attach_ref = {};
		depth_attach_ref.attachment = 1;
		depth_attach_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		VkSubpassDescription vk_subpass_desc = {};
		vk_subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		vk_subpass_desc.colorAttachmentCount = 1;
		vk_subpass_desc.pColorAttachments = &vk_color_attach_ref;
		vk_subpass_desc.pDepthStencilAttachment = &depth_attach_ref;
		
		VkSubpassDependency vk_dependency = {};
		vk_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		vk_dependency.dstSubpass = 0;
		vk_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		vk_dependency.srcAccessMask = 0;
		vk_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		vk_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		
		VkAttachmentDescription attachments[] = {
			vk_color_attach_desc,
			depth_attach_desc
		};
		
		VkRenderPassCreateInfo render_pass_create_info = {};
		render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_create_info.attachmentCount = ArrayCount(attachments);
		render_pass_create_info.pAttachments = &attachments[0];
		render_pass_create_info.subpassCount = 1;
		render_pass_create_info.pSubpasses = &vk_subpass_desc;
		render_pass_create_info.dependencyCount = 1;
		render_pass_create_info.pDependencies = &vk_dependency;
		
		if(vkCreateRenderPass(device, &render_pass_create_info, 0, &render_pass) != VK_SUCCESS) {
			platform->error("Couldn't create render pass");
		}
	}
	
	u32 findMemoryType(u32 type_filter, VkMemoryPropertyFlags properties, Platform *platform) {
		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
		
		for(u32 i = 0; i < memory_properties.memoryTypeCount; i++) {
			if(type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		
		platform->error("Couldn't find memory type");
		
		return UINT32_MAX;
	}
	
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &buffer_memory, Platform *platform) {
		VkBufferCreateInfo buffer_create_info = {};
		buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_create_info.size = size;
		buffer_create_info.usage = usage;
		buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		
		if(vkCreateBuffer(device, &buffer_create_info, 0, &buffer) != VK_SUCCESS) {
			platform->error("Couldn't create buffer");
		}
		
		VkMemoryRequirements memory_requirements;
		vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
		
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = memory_requirements.size;
		alloc_info.memoryTypeIndex = findMemoryType(memory_requirements.memoryTypeBits, properties, platform);
		
		if(vkAllocateMemory(device, &alloc_info, 0, &buffer_memory) != VK_SUCCESS) {
			platform->error("Couldn't allocate buffer memory");
		}
		
		vkBindBufferMemory(device, buffer, buffer_memory, 0);
	}
	
		
	void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
		
		VkCommandBuffer command_buffer = beginSingleTimeCommands();
		
		VkBufferCopy copy_region = {};
		copy_region.srcOffset = 0;
		copy_region.dstOffset = 0;
		copy_region.size = size;
		
		vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
		
		endSingleTimeCommands(command_buffer);
	}
	
	void createVertexBuffer(Platform *platform) {
		VkDeviceSize vb_size = sizeof(vertices[0]) * vertex_count;
		
		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_memory;
		
		createBuffer(
             vb_size, 
             VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
             staging_buffer, 
             staging_buffer_memory,
             platform
         );
		
		void *data;
		vkMapMemory(device, staging_buffer_memory, 0, vb_size, 0, &data);
		memcpy(data, vertices, vb_size);
		vkUnmapMemory(device, staging_buffer_memory);
		
		createBuffer(
			vb_size, 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertex_buffer,
			vertex_buffer_memory,
			platform
		);	
		
		copyBuffer(staging_buffer, vertex_buffer, vb_size);
		
		vkDestroyBuffer(device, staging_buffer, 0);
		vkFreeMemory(device, staging_buffer_memory, 0);
	}
	
	void createIndexBuffer(Platform *platform) {
		VkDeviceSize buffer_size = sizeof(indices[0]) * index_count;
		
		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_memory;
		
		createBuffer(
             buffer_size, 
             VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
             staging_buffer, 
             staging_buffer_memory,
             platform
         );
		
		void *data;
		vkMapMemory(device, staging_buffer_memory, 0, buffer_size, 0, &data);
		memcpy(data, indices, buffer_size);
		vkUnmapMemory(device, staging_buffer_memory);
		
		createBuffer(
			buffer_size, 
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			index_buffer,
			index_buffer_memory,
			platform
		);	
		
		copyBuffer(staging_buffer, index_buffer, buffer_size);
		
		vkDestroyBuffer(device, staging_buffer, 0);
		vkFreeMemory(device, staging_buffer_memory, 0);
	}
	
	void createUniformBuffer(Platform *platform) {
		VkDeviceSize buffer_size = sizeof(UniformBufferObject);
		uniform_buffers = (VkBuffer *)platform->alloc(sizeof(VkBuffer) * swap_image_count);
		uniform_buffers_memory = (VkDeviceMemory *)platform->alloc(sizeof(VkDeviceMemory) * swap_image_count);
		
		for(u32 i = 0; i < swap_image_count; i++) {
			createBuffer(
	             buffer_size, 
	             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
	             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
	             uniform_buffers[i], 
	             uniform_buffers_memory[i], 
	             platform
             );
		}
	}
	
	void createDescriptorPool(Platform *platform) {
		VkDescriptorPoolSize pool_sizes[2] = {};
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_sizes[0].descriptorCount = swap_image_count;
		
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[1].descriptorCount = swap_image_count;
		
		VkDescriptorPoolCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		create_info.poolSizeCount = ArrayCount(pool_sizes);
		create_info.pPoolSizes = &pool_sizes[0];
		create_info.maxSets = swap_image_count;
		
		if(vkCreateDescriptorPool(device, &create_info, 0, &descriptor_pool) != VK_SUCCESS) {
			platform->error("Couldn't create descriptor pool");
		}
	}
	
	void createDescriptorSets(Platform *platform) {
		VkDescriptorSetLayout *layouts = (VkDescriptorSetLayout *)platform->alloc(sizeof(VkDescriptorSetLayout) * swap_image_count);
		for(u32 i = 0; i < swap_image_count; i++) {
			layouts[i] = descriptor_set_layout;
		}
		
		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = swap_image_count;
		alloc_info.pSetLayouts = layouts;
		
		descriptor_sets = (VkDescriptorSet *)platform->alloc(sizeof(VkDescriptorSet) * swap_image_count);
		if(vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets) != VK_SUCCESS) {
			platform->error("Couldn't allocate descriptor sets");
		}
		
		for(u32 i = 0; i < swap_image_count; i++) {
			VkDescriptorBufferInfo buffer_info = {};
			buffer_info.buffer = uniform_buffers[i];
			buffer_info.offset = 0;
			buffer_info.range = sizeof(UniformBufferObject);
			
			VkDescriptorImageInfo image_info = {};
			image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			image_info.imageView = texture_image_view;
			image_info.sampler = texture_sampler;
			
			VkWriteDescriptorSet descriptor_writes[2] = {};
			
			descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor_writes[0].dstSet = descriptor_sets[i];
			descriptor_writes[0].dstBinding = 0;
			descriptor_writes[0].dstArrayElement = 0;
			descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptor_writes[0].descriptorCount = 1;
			descriptor_writes[0].pBufferInfo = &buffer_info;
			descriptor_writes[0].pImageInfo = 0;
			descriptor_writes[0].pTexelBufferView = 0;
			
			descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptor_writes[1].dstSet = descriptor_sets[i];
			descriptor_writes[1].dstBinding = 1;
			descriptor_writes[1].dstArrayElement = 0;
			descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptor_writes[1].descriptorCount = 1;
			descriptor_writes[1].pImageInfo = &image_info;
			descriptor_writes[1].pTexelBufferView = 0;
			
			
			vkUpdateDescriptorSets(device, ArrayCount(descriptor_writes), &descriptor_writes[0], 0, 0);
		}
	}
	
	void createDescriptorSetLayout(Platform *platform) {
		VkDescriptorSetLayoutBinding ubo_layout_binding = {};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		ubo_layout_binding.pImmutableSamplers = 0;
		
		VkDescriptorSetLayoutBinding sampler_layout_binding = {};
		sampler_layout_binding.binding = 1;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sampler_layout_binding.pImmutableSamplers = 0;
		
		VkDescriptorSetLayoutBinding layouts[] = {
			ubo_layout_binding,
			sampler_layout_binding	
		};
		
		VkDescriptorSetLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = ArrayCount(layouts);
		layout_info.pBindings = &layouts[0];
		
		if(vkCreateDescriptorSetLayout(device, &layout_info, 0, &descriptor_set_layout) != VK_SUCCESS) {
			platform->error("Couldn't create descriptor set layout");
		}
	}
	
	void createGraphicsPipeline(Platform *platform) {
		VkShaderModule vert_shader_module = createShaderModule(platform, device, "data/shaders/vert.spv");
		VkShaderModule frag_shader_module = createShaderModule(platform, device, "data/shaders/frag.spv");
		
		VkPipelineShaderStageCreateInfo vk_vert_shader_stage_create_info = {};
		vk_vert_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vk_vert_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vk_vert_shader_stage_create_info.module = vert_shader_module;
		vk_vert_shader_stage_create_info.pName = "main";
		
		VkPipelineShaderStageCreateInfo vk_frag_shader_stage_create_info = {};
		vk_frag_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vk_frag_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		vk_frag_shader_stage_create_info.module = frag_shader_module;
		vk_frag_shader_stage_create_info.pName = "main";
		
		VkPipelineShaderStageCreateInfo shader_stage_infos[] = {
			vk_vert_shader_stage_create_info,
			vk_frag_shader_stage_create_info
		};
		
		
		VkVertexInputBindingDescription vk_binding_description = {};
		vk_binding_description.binding = 0;
		vk_binding_description.stride = sizeof(Vertex);
		vk_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		
		VkVertexInputAttributeDescription vk_attribute_descriptions[3] = {};
		vk_attribute_descriptions[0].binding = 0;
		vk_attribute_descriptions[0].location = 0;
		vk_attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vk_attribute_descriptions[0].offset = offsetof(Vertex, pos);
		
		vk_attribute_descriptions[1].binding = 0;
		vk_attribute_descriptions[1].location = 1;
		vk_attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vk_attribute_descriptions[1].offset = offsetof(Vertex, color);
		
		vk_attribute_descriptions[2].binding = 0;
		vk_attribute_descriptions[2].location = 2;
		vk_attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		vk_attribute_descriptions[2].offset = offsetof(Vertex, uv);
		

		
		VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 1;
		vertex_input_info.pVertexBindingDescriptions  = &vk_binding_description;
		vertex_input_info.vertexAttributeDescriptionCount = ArrayCount(vk_attribute_descriptions);
		vertex_input_info.pVertexAttributeDescriptions = &vk_attribute_descriptions[0];
		
		VkPipelineInputAssemblyStateCreateInfo  input_assembly_create_info = {};
		input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_create_info.primitiveRestartEnable = VK_FALSE;
		
		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {};
		depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_create_info.depthTestEnable = VK_TRUE;
		depth_stencil_create_info.depthWriteEnable = VK_TRUE;
		depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_create_info.minDepthBounds = 0.0f;
		depth_stencil_create_info.maxDepthBounds = 1.0f;
		depth_stencil_create_info.stencilTestEnable = VK_FALSE;
		depth_stencil_create_info.front = {};
		depth_stencil_create_info.back = {};
		
		VkViewport vk_viewport = {};
		vk_viewport.x = 0;
		vk_viewport.y = 0;
		vk_viewport.width = extent.width;
		vk_viewport.height = extent.height;
		vk_viewport.minDepth = 0.0f;
		vk_viewport.maxDepth = 1.0f;
		
		VkRect2D vk_scissor = {};
		vk_scissor.offset = {0, 0};
		vk_scissor.extent = extent;
			
		VkPipelineViewportStateCreateInfo vk_viewport_state_create_info = {};
		vk_viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vk_viewport_state_create_info.viewportCount = 1;
		vk_viewport_state_create_info.pViewports = &vk_viewport;
		vk_viewport_state_create_info.scissorCount = 1;
		vk_viewport_state_create_info.pScissors = &vk_scissor;
		
		VkPipelineRasterizationStateCreateInfo vk_rasterizer_create_info = {};
		vk_rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		vk_rasterizer_create_info.depthClampEnable = VK_FALSE;
		vk_rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
		vk_rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		vk_rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		vk_rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		
		vk_rasterizer_create_info.depthBiasEnable = VK_FALSE;
		vk_rasterizer_create_info.depthBiasConstantFactor = 0.0f;
		vk_rasterizer_create_info.depthBiasClamp = 0.0f;
		vk_rasterizer_create_info.depthBiasSlopeFactor = 0.0f;
		vk_rasterizer_create_info.lineWidth = 1.0f;
		
		VkPipelineMultisampleStateCreateInfo vk_msaa_state_create_info = {};
		vk_msaa_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		vk_msaa_state_create_info.sampleShadingEnable = VK_FALSE;
		vk_msaa_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		vk_msaa_state_create_info.minSampleShading = 1.0f;
		vk_msaa_state_create_info.pSampleMask = 0;
		vk_msaa_state_create_info.alphaToCoverageEnable = VK_FALSE;
		vk_msaa_state_create_info.alphaToOneEnable = VK_FALSE;
		
		VkPipelineColorBlendAttachmentState vk_color_blend_attachment = {};
		vk_color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		vk_color_blend_attachment.blendEnable = VK_FALSE;
		vk_color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		vk_color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		vk_color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		vk_color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		vk_color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		vk_color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		
		VkPipelineColorBlendStateCreateInfo vk_color_blend_state_create_info = {};
		vk_color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		vk_color_blend_state_create_info.logicOpEnable = VK_FALSE;
		vk_color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
		vk_color_blend_state_create_info.attachmentCount = 1;
		vk_color_blend_state_create_info.pAttachments = &vk_color_blend_attachment;
		vk_color_blend_state_create_info.blendConstants[0] = 0.0f;
		vk_color_blend_state_create_info.blendConstants[1] = 0.0f;
		vk_color_blend_state_create_info.blendConstants[2] = 0.0f;
		vk_color_blend_state_create_info.blendConstants[3] = 0.0f;
		
		VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
		pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_create_info.setLayoutCount = 1;
		pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;
		pipeline_layout_create_info.pushConstantRangeCount = 0;
		pipeline_layout_create_info.pPushConstantRanges = 0;
		
		if(vkCreatePipelineLayout(device, &pipeline_layout_create_info, 0, &pipeline_layout) != VK_SUCCESS) {
			platform->error("Couldn't create pipeline layout");
		}
		
		VkGraphicsPipelineCreateInfo vk_graphics_pipeline_create_info = {};
		vk_graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		vk_graphics_pipeline_create_info.stageCount = 2;
		vk_graphics_pipeline_create_info.pStages = shader_stage_infos;
		vk_graphics_pipeline_create_info.pVertexInputState = &vertex_input_info;
		vk_graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
		vk_graphics_pipeline_create_info.pViewportState = &vk_viewport_state_create_info;
		vk_graphics_pipeline_create_info.pRasterizationState = &vk_rasterizer_create_info;
		vk_graphics_pipeline_create_info.pMultisampleState = &vk_msaa_state_create_info;
		vk_graphics_pipeline_create_info.pDepthStencilState = 0;
		vk_graphics_pipeline_create_info.pColorBlendState = &vk_color_blend_state_create_info;
		vk_graphics_pipeline_create_info.pDynamicState = 0;
		vk_graphics_pipeline_create_info.layout = pipeline_layout;
		vk_graphics_pipeline_create_info.renderPass = render_pass;
		vk_graphics_pipeline_create_info.subpass = 0;
		vk_graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
		vk_graphics_pipeline_create_info.basePipelineIndex = -1;
		vk_graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
		
		
		if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &vk_graphics_pipeline_create_info, 0, &graphics_pipeline) != VK_SUCCESS) {
			platform->error("Couldn't create graphics pipeline");
		}
		
		vkDestroyShaderModule(device, vert_shader_module, 0);
		vkDestroyShaderModule(device, frag_shader_module, 0);
	}
	
	void createFramebuffers(Platform *platform) {
		swap_chain_frame_buffers = (VkFramebuffer *)platform->alloc(sizeof(VkFramebuffer) * swap_image_count);
		for(u32 i = 0; i < swap_image_count; i++) {
			VkImageView attachments[] = {
				swap_image_views[i],
				depth_image_view
			};
			
			VkFramebufferCreateInfo vk_frame_buffer_create_info = {};
			vk_frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			vk_frame_buffer_create_info.renderPass = render_pass;
			vk_frame_buffer_create_info.attachmentCount = ArrayCount(attachments);
			vk_frame_buffer_create_info.pAttachments = attachments;
			vk_frame_buffer_create_info.width = extent.width;
			vk_frame_buffer_create_info.height = extent.height;
			vk_frame_buffer_create_info.layers = 1;
			if(vkCreateFramebuffer(device, &vk_frame_buffer_create_info, 0, &swap_chain_frame_buffers[i]) != VK_SUCCESS) {
				platform->error("Couldn't create frame buffer");
			}
		}
	}
	
	void createCommandPool(Platform *platform) {
		
	
		VkCommandPoolCreateInfo vk_pool_create_info = {};
		vk_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		vk_pool_create_info.queueFamilyIndex = graphics_queue_index;
		vk_pool_create_info.flags = 0;
		
		if(vkCreateCommandPool(device, &vk_pool_create_info, 0, &command_pool) != VK_SUCCESS) {
			platform->error("Couldn't create command pool");
		}
	}
	
	VkCommandBuffer beginSingleTimeCommands() {
		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandPool = command_pool;
		alloc_info.commandBufferCount = 1;
		
		VkCommandBuffer command_buffer;
		vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);
		
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		
		vkBeginCommandBuffer(command_buffer, &begin_info);
		
		return command_buffer;
	}
	
	void endSingleTimeCommands(VkCommandBuffer command_buffer) {
		vkEndCommandBuffer(command_buffer);
		
		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
		
		vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphics_queue);
		
		vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
	}
	
	void createAndRecordCommandBuffers(Platform *platform) {
			
		command_buffers = (VkCommandBuffer *)platform->alloc(sizeof(VkCommandBuffer) * swap_image_count);
		
		VkCommandBufferAllocateInfo vk_cmd_buffer_alloc_info = {};
		vk_cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		vk_cmd_buffer_alloc_info.commandPool = command_pool;
		vk_cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		vk_cmd_buffer_alloc_info.commandBufferCount = swap_image_count;
		
		if(vkAllocateCommandBuffers(device, &vk_cmd_buffer_alloc_info, command_buffers) != VK_SUCCESS) {
			platform->error("Couldn't allocate command buffers");
		}
		
		for(u32 i = 0; i < swap_image_count; i++) {
			VkCommandBufferBeginInfo begin_info = {};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin_info.pInheritanceInfo = 0;
			
			if(vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
				platform->error("Couldn't begin recording command buffer");
			}
			
			VkClearValue clear_values[] = {
				{0.0f, 0.0f, 0.0f, 1.0f},
				{1.0f, 0.0f}
			};
			
			VkRenderPassBeginInfo render_pass_begin_info = {};
			render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_begin_info.renderPass = render_pass;
			render_pass_begin_info.framebuffer = swap_chain_frame_buffers[i];
			render_pass_begin_info.renderArea.offset = {0, 0};
			render_pass_begin_info.renderArea.extent = extent;
			render_pass_begin_info.clearValueCount = ArrayCount(clear_values);
			render_pass_begin_info.pClearValues = &clear_values[0];
			
			vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
			
			vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
			
			VkBuffer vertex_buffers[] = {vertex_buffer};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);
			
			vkCmdBindIndexBuffer(command_buffers[i], index_buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_sets[i], 0, 0);
			vkCmdDrawIndexed(command_buffers[i], index_count, 1, 0, 0, 0);
			
			vkCmdEndRenderPass(command_buffers[i]);
			
			if(vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
				platform->error("Couldn't record command buffer");
			}
		}
	}
	
	void createSyncObjects(Platform *platform) {
		
		VkSemaphoreCreateInfo vk_semaphore_create_info = {};
		vk_semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		
		VkFenceCreateInfo vk_fence_create_info = {};
		vk_fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		vk_fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		
		for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {	
			VkResult first_result = vkCreateSemaphore(device, &vk_semaphore_create_info, 0, &image_available_semaphores[i]);
			VkResult second_result = vkCreateSemaphore(device, &vk_semaphore_create_info, 0, &render_finished_semaphores[i]);
			VkResult third_result = vkCreateFence(device, &vk_fence_create_info, 0, &in_flight_fences[i]);
			if(first_result != VK_SUCCESS || second_result != VK_SUCCESS || third_result != VK_SUCCESS) {
				platform->error("Couldn't create sync objects");
			}	
		}
	}
	
	void cleanupSwapChain() {
		
		vkDestroyImageView(device, depth_image_view, 0);
		vkDestroyImage(device, depth_image, 0);
		vkFreeMemory(device, depth_image_memory, 0);
		
		for(u32 i = 0; i < swap_image_count; i++) {
			vkDestroyFramebuffer(device, swap_chain_frame_buffers[i], 0);	
		}
		
		vkFreeCommandBuffers(device, command_pool, swap_image_count, command_buffers);
		
		vkDestroyPipeline(device, graphics_pipeline, 0);
		vkDestroyPipelineLayout(device, pipeline_layout, 0);
		vkDestroyRenderPass(device, render_pass, 0);
		
		
		for(u32 i = 0; i < swap_image_count; i++) {
			vkDestroyImageView(device, swap_image_views[i], 0);	
		}
		
		vkDestroySwapchainKHR(device, swap_chain, 0);
	}
	
	void recreateSwapChain(Platform *platform, PlatformWindow *window) {
		vkDeviceWaitIdle(device);
		
		cleanupSwapChain();
		
		createSwapChain(platform, window);
		createImageViews(platform);
		createRenderPass(platform);
		createGraphicsPipeline(platform);
		createDepthResources(platform);
		createFramebuffers(platform);
		createAndRecordCommandBuffers(platform);
	}
	
	void createImage(u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &image_memory, Platform *platform) {
		VkImageCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent.width = width;
		create_info.extent.height = height;
		create_info.extent.depth = 1;
		create_info.mipLevels = 1;
		create_info.arrayLayers = 1;
		create_info.format = format;
		create_info.tiling = tiling;
		create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		create_info.usage = usage;
		create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		create_info.flags = 0;
		
		if(vkCreateImage(device, &create_info, 0, &image) != VK_SUCCESS) {
			platform->error("Failed to create image");
		}
		
		VkMemoryRequirements memory_requirements;
		vkGetImageMemoryRequirements(device, image, &memory_requirements);
		
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = memory_requirements.size;
		alloc_info.memoryTypeIndex = findMemoryType(memory_requirements.memoryTypeBits, properties, platform);
		
		if(vkAllocateMemory(device, &alloc_info, 0, &image_memory) != VK_SUCCESS) {
			platform->error("Couldn't allocate image memory");
		}
		
		vkBindImageMemory(device, image, image_memory, 0);
	}
	
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, Platform *platform) {
		VkCommandBuffer command_buffer = beginSingleTimeCommands();
		
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = old_layout;
		barrier.newLayout = new_layout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;
		
		VkPipelineStageFlags source_stage;
		VkPipelineStageFlags destination_stage;
		
		if(new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			
			if(hasStencilComponent(format)) {
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		} else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		
		if(old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			
			source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else if(old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			
			source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		} else if(old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			
			source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		} else {
			platform->error("Unsupported layout transition");
		}
		
		vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, 0, 0, 0, 1, &barrier);
		
		endSingleTimeCommands(command_buffer);
	}
	
	void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height) {
		VkCommandBuffer command_buffer = beginSingleTimeCommands();
		
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1};
		
		vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		
		endSingleTimeCommands(command_buffer);
	}
	
	void createTextureImage(Platform *platform) {
		int width, height, channels;
		u8 *pixels = stbi_load("data/textures/chalet.jpg", &width, &height, &channels, STBI_rgb_alpha);
		VkDeviceSize image_size = width * height * 4;
		
		if(pixels == 0) {
			platform->error("Couldn't load image");
		}
		
		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_memory;
		createBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory, platform);
		
		void *data;
		vkMapMemory(device, staging_buffer_memory, 0, image_size, 0, &data);
		memcpy(data, pixels, (size_t)image_size);
		vkUnmapMemory(device, staging_buffer_memory);
		stbi_image_free(pixels);
		
		createImage((u32)width, (u32)height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image, texture_image_memory, platform);
		
		transitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, platform);
		copyBufferToImage(staging_buffer, texture_image, (u32)width, (u32)height);
		
		transitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, platform);
		
		vkDestroyBuffer(device, staging_buffer, 0);
		vkFreeMemory(device, staging_buffer_memory, 0);
	}
	
	void createTextureImageView(Platform *platform) {
		texture_image_view = createImageView(texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, platform);
	}
	
	void createTextureSampler(Platform *platform) {
		VkSamplerCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		
		create_info.anisotropyEnable = VK_TRUE;
		create_info.maxAnisotropy = 16;
		
		create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		create_info.unnormalizedCoordinates = VK_FALSE;
		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_ALWAYS;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.mipLodBias = 0.0f;
		create_info.minLod = 0.0f;
		create_info.maxLod = 0.0f;
		
		if(vkCreateSampler(device, &create_info, 0, &texture_sampler) != VK_SUCCESS) {
			platform->error("Couldn't create texture sampler");
		}
	}
	
	VkFormat findSupportedFormat(const VkFormat *candidates, const u32 candidate_count, VkImageTiling tiling, VkFormatFeatureFlags features, Platform *platform) {
		for(u32 i = 0; i < candidate_count; i++) {
			VkFormat format = candidates[i];
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);
			if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
				return format;
			else if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
				return format;
		}
		
		platform->error("Failed to find supported format");
		return (VkFormat)0;
	}
	
	VkFormat findDepthFormat(Platform *platform) {
		VkFormat formats[] = {
			VK_FORMAT_D32_SFLOAT, 
			VK_FORMAT_D32_SFLOAT_S8_UINT, 
			VK_FORMAT_D24_UNORM_S8_UINT
		};
		return findSupportedFormat(formats, ArrayCount(formats), VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, platform);
	}
	
	bool hasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}
	
	void createDepthResources(Platform *platform) {
		VkFormat depth_format = findDepthFormat(platform);
		createImage(
            extent.width, extent.height, 
            depth_format, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depth_image, depth_image_memory, platform
        );
        depth_image_view = createImageView(depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, platform);
        
        transitionImageLayout(depth_image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, platform);
	}
	
	void init(Platform *platform, PlatformWindow *window) {
		createInstance(platform, window);	
		setupDebugUtils(platform);
		createSurface(platform, window);
		pickPhysicalDevice(platform);
		pickQueues(platform);
		createDevice(platform);
		createQueues();
		createSwapChain(platform, window);
		createImageViews(platform);
		createRenderPass(platform);
		createDescriptorSetLayout(platform);
		createGraphicsPipeline(platform);
		createCommandPool(platform);
		createDepthResources(platform);
		createFramebuffers(platform);
		createTextureImage(platform);
		createTextureImageView(platform);
		createTextureSampler(platform);
		createVertexBuffer(platform);
		createIndexBuffer(platform);
		createUniformBuffer(platform);
		createDescriptorPool(platform);
		createDescriptorSets(platform);
		createAndRecordCommandBuffers(platform);
		createSyncObjects(platform);
	}	
	
	void startFrame() {
		vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);
	}
	
	void recreateIfFailed(VkResult result, Platform *platform, PlatformWindow *window, char *error) {
		
		if(result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain(platform, window);
			return;
		} else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			platform->error(error);
		}
	}
	
	f32 rotation = 0.0f;
	
	void updateUniformBuffers(u32 current_image, float delta) {
		UniformBufferObject ubo = {};
		rotation += delta * 10.0f;
		ubo.model = Mat4::transpose(Mat4::translate(Vec3(0.0f, 0.0f, 0.0f)) * Mat4::rotateY(Math::Pi32) * Mat4::rotateZ(Math::toRadians(rotation)));
		
		Vec3 camera_position = Vec3(2.0f, 0.0f, -2.0f);
		Vec3 forward = Vec3::normalize(-camera_position);
		
		ubo.view = Mat4::transpose(Mat4::lookAt(camera_position, forward, Vec3(0.0f, 0.0f, 1.0f)));
		// ubo.view = Mat4();
		ubo.projection = Mat4::transpose(Mat4::perspective(45.0f, (f32)extent.width / (f32)extent.height, 0.1f, 10.0f));
		// ubo.projection = Mat4();
		
		void *data;
		vkMapMemory(device, uniform_buffers_memory[current_image], 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(device, uniform_buffers_memory[current_image]);
	}
	
	void renderFrame(Platform *platform, PlatformWindow *window, float delta) {
		u32 image_index;
		VkResult result = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
		recreateIfFailed(result, platform, window, "Failed to acquire swap chain image");
				
		VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
		VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		VkSemaphore signal_semaphores[] = {render_finished_semaphores[current_frame]};
		
		updateUniformBuffers(image_index, delta);
		
		VkSubmitInfo vk_submit_info = {};
		vk_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		vk_submit_info.waitSemaphoreCount = 1;
		vk_submit_info.pWaitSemaphores = wait_semaphores;
		vk_submit_info.pWaitDstStageMask = wait_stages;
		vk_submit_info.commandBufferCount = 1;
		vk_submit_info.pCommandBuffers = &command_buffers[image_index];
		vk_submit_info.signalSemaphoreCount = 1;
		vk_submit_info.pSignalSemaphores = signal_semaphores;
		
		vkResetFences(device, 1, &in_flight_fences[current_frame]);
		
		if(vkQueueSubmit(graphics_queue, 1, &vk_submit_info, in_flight_fences[current_frame]) != VK_SUCCESS) {
			platform->error("Couldn't submit draw command buffer");
		}
		
		VkSwapchainKHR swap_chains[] = {swap_chain};
		
		VkPresentInfoKHR vk_present_info = {};
		vk_present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		vk_present_info.waitSemaphoreCount = 1;
		vk_present_info.pWaitSemaphores = signal_semaphores;
		vk_present_info.swapchainCount = 1;
		vk_present_info.pSwapchains = swap_chains;
		vk_present_info.pImageIndices = &image_index;
		vk_present_info.pResults = 0;
		
		result = vkQueuePresentKHR(present_queue, &vk_present_info);
		recreateIfFailed(result, platform, window, "Failed to present swap chain images");
	}
	
	void endFrame() {
		current_frame = (current_frame + 1)  % MAX_FRAMES_IN_FLIGHT;
	}
	
	void cleanup() {
		vkDeviceWaitIdle(device);
		
		cleanupSwapChain();

		vkDestroySampler(device, texture_sampler, 0);
		vkDestroyImageView(device, texture_image_view, 0);
		vkDestroyImage(device, texture_image, 0);
		vkFreeMemory(device, texture_image_memory, 0);

		vkDestroyDescriptorPool(device, descriptor_pool, 0);
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout, 0);

		for(u32 i = 0; i < swap_image_count; i++) {
			vkDestroyBuffer(device, uniform_buffers[i], 0);
			vkFreeMemory(device, uniform_buffers_memory[i], 0);
		}

		vkDestroyBuffer(device, index_buffer, 0);
		vkFreeMemory(device, index_buffer_memory, 0);

		vkDestroyBuffer(device, vertex_buffer, 0);
		vkFreeMemory(device, vertex_buffer_memory, 0);
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

		
		for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {	
			vkDestroySemaphore(device, render_finished_semaphores[i], 0);
			vkDestroySemaphore(device, image_available_semaphores[i], 0);	
			vkDestroyFence(device, in_flight_fences[i], 0);
		}
		
		vkDestroyCommandPool(device, command_pool, 0);
		
		vkDestroyDevice(device, 0);
		vkDestroyDebugUtilsMessengerEXT(instance, debug_callback, 0);
		vkDestroySurfaceKHR(instance, surface, 0);
		vkDestroyInstance(instance, 0);
	}
};
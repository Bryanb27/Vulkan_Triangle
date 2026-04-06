#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <fstream>

struct App {
    // Window
    GLFWwindow* window;

    // Vulkan core
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    VkQueue graphicsQueue;

    uint32_t graphicsQueueFamilyIndex;

    // Swapchain
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> imageViews;
    VkSurfaceCapabilitiesKHR capabilities;

    // Pipeline
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    // Shaders
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;

    // Framebuffers
    std::vector<VkFramebuffer> framebuffers;

    // Commands
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    // Sync
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    // GLFW extensions
    uint32_t glfwExtensionCount;
    const char** glfwExtensions;
};

const int WIDTH = 800;
const int HEIGHT = 600;

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    std::streamsize fileSize = file.tellg();

    if (fileSize <= 0) {
        throw std::runtime_error("file is empty or invalid: " + filename);
    }

    std::vector<char> buffer(static_cast<size_t>(fileSize));

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

void initWindow(App& app){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    app.window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    app.glfwExtensions = glfwGetRequiredInstanceExtensions(&app.glfwExtensionCount);
}

/*
void initVulkan(){
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
}*/

void initVulkan(App& app){
    // 1. Create Vulkan instance
    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.enabledExtensionCount = app.glfwExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames = app.glfwExtensions;

    if(vkCreateInstance(&instanceCreateInfo, nullptr, &app.instance) != VK_SUCCESS){
        throw std::runtime_error("failed to create instance");
    }

    // Create Surface
    if (glfwCreateWindowSurface(app.instance, app.window, nullptr, &app.surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create surface");
    }

    // Verify the devices to operate Vulkan
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(app.instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(app.instance, &deviceCount, devices.data());

    // Pick the first one
    app.physicalDevice = devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(app.physicalDevice, &props);

    std::cout << "Using GPU: " << props.deviceName << std::endl;

    /*
     GPU has different types of workers, called queue families:
     Graphics(drawning)
     Compute(math)
     Transfer(copying memory)
     and we need to pick one
    */
    //Finds graphics queue
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(app.physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(app.physicalDevice, &queueFamilyCount, queueFamilies.data());

    app.graphicsQueueFamilyIndex = -1;

    for (int i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            app.graphicsQueueFamilyIndex = i;
            break;
        }
    }

    if (app.graphicsQueueFamilyIndex == -1) {
        throw std::runtime_error("failed to find a graphics queue family");
    }

    //Create logical device
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = app.graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(app.physicalDevice, &deviceCreateInfo, nullptr, &app.device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device");
    }

    //Get the queue
    vkGetDeviceQueue(app.device, app.graphicsQueueFamilyIndex, 0, &app.graphicsQueue);

    
    // Swapchain
    // Check surface support
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        app.physicalDevice,
        app.graphicsQueueFamilyIndex,
        app.surface,
        &presentSupport
    );

    if (!presentSupport) {
        throw std::runtime_error("queue family does not support presenting");
    }

    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app.physicalDevice, app.surface, &app.capabilities);

    // Choose format
    uint32_t formatCount = 0;

    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        app.physicalDevice,
        app.surface,
        &formatCount,
        nullptr
    );

    if (result != VK_SUCCESS || formatCount == 0) {
        throw std::runtime_error("failed to get surface formats");
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);

    vkGetPhysicalDeviceSurfaceFormatsKHR(
        app.physicalDevice,
        app.surface,
        &formatCount,
        formats.data()
    );

    VkSurfaceFormatKHR surfaceFormat = formats[0];

    // Create Swapchain
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = app.surface;

    uint32_t imageCountDesired = app.capabilities.minImageCount + 1;

    if (app.capabilities.maxImageCount > 0 &&
        imageCountDesired > app.capabilities.maxImageCount) {
        imageCountDesired = app.capabilities.maxImageCount;
    }

    swapchainCreateInfo.minImageCount = imageCountDesired;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = app.capabilities.currentExtent;

    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    swapchainCreateInfo.preTransform = app.capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(app.device, &swapchainCreateInfo, nullptr, &app.swapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swapchain");
    }

    // Get images
    uint32_t imageCount = 0;

    VkResult res = vkGetSwapchainImagesKHR(
        app.device,
        app.swapchain,
        &imageCount,
        nullptr
    );

    if (res != VK_SUCCESS || imageCount == 0) {
        throw std::runtime_error("failed to get swapchain image count");
    }

    app.swapchainImages.resize(imageCount);

    res = vkGetSwapchainImagesKHR(
        app.device,
        app.swapchain,
        &imageCount,
        app.swapchainImages.data()
    );

    if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to get swapchain images");
    }

    // Create Render Pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;


    if (vkCreateRenderPass(app.device, &renderPassInfo, nullptr, &app.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass");
    }

    // Create shader modules
    auto vertCode = readFile("../shaders/vert.spv");
    auto fragCode = readFile("../shaders/frag.spv");

    VkShaderModuleCreateInfo vertInfo{};
    vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertInfo.codeSize = vertCode.size();
    vertInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());

    vkCreateShaderModule(app.device, &vertInfo, nullptr, &app.vertShaderModule);

    // Fragment
    VkShaderModuleCreateInfo fragInfo{};
    fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragInfo.codeSize = fragCode.size();
    fragInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());

    vkCreateShaderModule(app.device, &fragInfo, nullptr, &app.fragShaderModule);

    // Shader stages
    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = app.vertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = app.fragShaderModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    vkCreatePipelineLayout(app.device, &pipelineLayoutInfo, nullptr, &app.pipelineLayout);

    // Viewport + scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) app.capabilities.currentExtent.width;
    viewport.height = (float) app.capabilities.currentExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = app.capabilities.currentExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Create Graphics Pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;

    pipelineInfo.layout = app.pipelineLayout;
    pipelineInfo.renderPass = app.renderPass;
    pipelineInfo.subpass = 0;

    vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &app.graphicsPipeline);

    // Create Framebuffers
    app.imageViews.resize(app.swapchainImages.size());
    app.framebuffers.resize(app.swapchainImages.size());

    for (size_t i = 0; i < app.swapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = app.swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;

        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(app.device, &viewInfo, nullptr, &app.imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view");
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = app.renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &app.imageViews[i];
        framebufferInfo.width = app.capabilities.currentExtent.width;
        framebufferInfo.height = app.capabilities.currentExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(app.device, &framebufferInfo, nullptr, &app.framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer");
        }
    }

    // Command Pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = app.graphicsQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    vkCreateCommandPool(app.device, &poolInfo, nullptr, &app.commandPool);

    // Command Buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = app.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(app.device, &allocInfo, &app.commandBuffer);

    // Submit to GPU
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &app.commandBuffer;

    // Create Semaphores
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    vkCreateSemaphore(app.device, &semaphoreInfo, nullptr, &app.imageAvailableSemaphore);
    vkCreateSemaphore(app.device, &semaphoreInfo, nullptr, &app.renderFinishedSemaphore);
}

void mainLoop(App& app){
    while(!glfwWindowShouldClose(app.window)){
        glfwPollEvents();

        uint32_t imageIndex;

        // 1. Acquire image
        vkAcquireNextImageKHR(
            app.device,
            app.swapchain,
            UINT64_MAX,
            app.imageAvailableSemaphore,
            VK_NULL_HANDLE,
            &imageIndex
        );

        // RESET + RECORD COMMAND BUFFER HERE
        vkResetCommandBuffer(app.commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(app.commandBuffer, &beginInfo);

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = app.renderPass;
        renderPassBeginInfo.framebuffer = app.framebuffers[imageIndex]; 
        renderPassBeginInfo.renderArea.extent = app.capabilities.currentExtent;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(app.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(app.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, app.graphicsPipeline);

        vkCmdDraw(app.commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(app.commandBuffer);

        vkEndCommandBuffer(app.commandBuffer);


        // 2. Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {app.imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &app.commandBuffer;

        VkSemaphore signalSemaphores[] = {app.renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkQueueSubmit(app.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

        // 3. Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &app.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(app.graphicsQueue, &presentInfo);

        vkQueueWaitIdle(app.graphicsQueue);
    }
}

void cleanup(App& app){
    // Semaphores
    vkDestroySemaphore(app.device, app.renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(app.device, app.imageAvailableSemaphore, nullptr);

    // Command pool (frees command buffers too)
    vkDestroyCommandPool(app.device, app.commandPool, nullptr);

    // Framebuffers
    for (auto framebuffer : app.framebuffers) {
        vkDestroyFramebuffer(app.device, framebuffer, nullptr);
    }

    // Image views
    for (auto imageView : app.imageViews) {
        vkDestroyImageView(app.device, imageView, nullptr);
    }

    // Pipeline
    vkDestroyPipeline(app.device, app.graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.pipelineLayout, nullptr);

    // Shaders
    vkDestroyShaderModule(app.device, app.fragShaderModule, nullptr);
    vkDestroyShaderModule(app.device, app.vertShaderModule, nullptr);

    // Render pass
    vkDestroyRenderPass(app.device, app.renderPass, nullptr);

    // Then swapchain
    vkDestroySwapchainKHR(app.device, app.swapchain, nullptr);

    // Then device
    vkDestroyDevice(app.device, nullptr);

    // Then surface + instance
    vkDestroySurfaceKHR(app.instance, app.surface, nullptr);
    vkDestroyInstance(app.instance, nullptr);

    // Finally window
    glfwDestroyWindow(app.window);
    glfwTerminate();
}

int main(){
    App app{};

    initWindow(app);

    initVulkan(app);

    mainLoop(app);

    cleanup(app);

    return 0;
}
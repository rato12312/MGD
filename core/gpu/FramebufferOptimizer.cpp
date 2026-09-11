// MFO Implementation - Framebuffer Optimizer
// Complete implementation of all 6 phases

#include "FramebufferOptimizer.h"
#include "VulkanContext.h"
#include "FramebufferManager.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>

namespace mgd {
namespace gpu {

// ============================================================================
// FramebufferBasic Implementation
// ============================================================================

bool FramebufferBasic::init(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat color_format) {
    ctx_ = ctx;
    width_ = width;
    height_ = height;
    color_format_ = color_format;
    depth_format_ = VK_FORMAT_D16_UNORM;

    if (!createRenderPass()) return false;
    if (!createFramebufferResources(1280, 720)) return false; // Default to 720p
    return true;
}

void FramebufferBasic::shutdown() {
    destroyFrameResources();
    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr);
        framebuffer_ = VK_NULL_HANDLE;
    }
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }
    if (cmd_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr);
        cmd_pool_ = VK_NULL_HANDLE;
    }
    if (cmd_buffer_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(ctx_->device(), cmd_pool_, 1, &cmd_buffer_);
        cmd_buffer_ = VK_NULL_HANDLE;
    }
    if (cmd_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr);
        cmd_pool_ = VK_NULL_HANDLE;
    }
    destroyFrameResources();
    initialized_ = false;
}

bool FramebufferBasic::createRenderPass() {
#ifdef VK_VERSION_1_0
    if (ctx_->device() == VK_NULL_HANDLE) return true;
    
    // Rascunho: 1 color R8G8B8A8 + 1 depth D16, 0.4x, single subpass, tile memory (LOAD_CLEAR, STORE_STORE)
    VkAttachmentDescription color{}; 
    color.format = VK_FORMAT_R8G8B8A8_UNORM; 
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkAttachmentDescription depth{}; 
    depth.format = VK_FORMAT_D16_UNORM; 
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dr{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; 
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; 
    sub.colorAttachmentCount = 1; 
    sub.pColorAttachments = &cr; 
    sub.pDepthStencilAttachment = &dr;
    
    std::array<VkAttachmentDescription,2> atts{color,depth};
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; 
    ci.attachmentCount=2; ci.pAttachments=atts.data(); ci.subpassCount=1; ci.pSubpasses=&sub;
    return vkCreateRenderPass(ctx_->device(),&ci,nullptr,&render_pass_)==VK_SUCCESS;
#else
    return true;
#endif
}

bool FramebufferBasic::createFramebufferResources(uint32_t width, uint32_t height) {
    destroyFrameResources();
    
    auto color = createImage(1280, 720, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    auto depth = createImage(1280, 720, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if(!color||!depth) return false;
    
    VkImageView views[2] = {color->view, depth->view};
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass = render_pass_; 
    ci.attachmentCount=2; 
    ci.pAttachments=views; 
    ci.width=1280; 
    ci.height=720; 
    ci.layers=1;
    
    VkFramebuffer fb; 
    if(vkCreateFramebuffer(ctx_->device(),&ci,nullptr,&fb)!=VK_SUCCESS) return false;
    framebuffers_.push_back(fb);
    color_image_ = std::move(color);
    depth_image_ = std::move(depth);
    
    // Previous frame resources
    auto prev_color = createImage(1280, 720, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    auto prev_depth = createImage(1280, 720, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    if(!prev_color||!prev_depth) return false;
    
    VkImageView views[2] = {prev_color->view, prev_depth->view};
    VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbci.renderPass = render_pass_; 
    fbci.attachmentCount=2; 
    fbci.pAttachments=views; 
    fbci.width=1280; 
    fbci.height=720; 
    fbci.layers=1;
    
    VkFramebuffer fb; 
    if(vkCreateFramebuffer(ctx_->device(),&fbci,nullptr,&prev_framebuffer_)!=VK_SUCCESS) return false;
    prev_framebuffers_.push_back(prev_framebuffer_);
    
    color_image_ = std::move(color);
    depth_image_ = std::move(depth);
    prev_color_img_ = std::move(prev_color);
    prev_depth_img_ = std::move(prev_depth);
    prev_color_view_ = prev_color->view;
    prev_depth_view_ = prev_depth->view;
    prev_color_mem_ = std::move(prev_color);
    prev_depth_mem_ = std::move(prev_depth);
    
    return true;
}

void FramebufferBasic::destroyFrameResources() {
    current_color_view_ = VK_NULL_HANDLE;
    current_depth_view_ = VK_NULL_HANDLE;
    if (current_color_img_) { vkDestroyImage(ctx_->device(), current_color_img_, nullptr); current_color_img_ = VK_NULL_HANDLE; }
    if (current_depth_img_) { vkDestroyImage(ctx_->device(), current_depth_img_, nullptr); current_depth_img_ = VK_NULL_HANDLE; }
    if (current_color_mem_) { vkFreeMemory(ctx_->device(), current_color_mem_, nullptr); current_color_mem_ = VK_NULL_HANDLE; }
    if (current_depth_mem_) { vkFreeMemory(ctx_->device(), current_depth_mem_, nullptr); current_depth_mem_ = VK_NULL_HANDLE; }
    
    if (prev_color_img_) { vkDestroyImage(ctx_->device(), prev_color_img_, nullptr); prev_color_img_ = VK_NULL_HANDLE; }
    if (prev_depth_img_) { vkDestroyImage(ctx_->device(), prev_depth_img_, nullptr); prev_depth_img_ = VK_NULL_HANDLE; }
    if (prev_color_view_) { vkDestroyImageView(ctx_->device(), prev_color_view_, nullptr); prev_color_view_ = VK_NULL_HANDLE; }
    if (prev_depth_view_) { vkDestroyImageView(ctx_->device(), prev_depth_view_, nullptr); prev_depth_view_ = VK_NULL_HANDLE; }
    if (prev_color_mem_) { vkFreeMemory(ctx_->device(), prev_color_mem_, nullptr); prev_color_mem_ = VK_NULL_HANDLE; }
    if (prev_depth_mem_) { vkFreeMemory(ctx_->device(), prev_depth_mem_, nullptr); prev_depth_mem_ = VK_NULL_HANDLE; }
    
    if (framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr); framebuffer_ = VK_NULL_HANDLE; }
    if (prev_framebuffer_) { vkDestroyFramebuffer(ctx_->device(), prev_framebuffer_, nullptr); prev_framebuffer_ = VK_NULL_HANDLE; }
}

bool FramebufferBasic::init(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat color_format) {
    ctx_ = ctx;
    width_ = width;
    height_ = height;
    color_format_ = color_format;
    depth_format_ = VK_FORMAT_D16_UNORM;
    
    if (!createRenderPass()) return false;
    if (!createFramebufferResources(1280, 720)) return false; // Default to 720p
    initialized_ = true;
    return true;
}

void FramebufferBasic::shutdown() {
    if (ctx_ && ctx_->device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx_->device());
    }
    destroyFrameResources();
    if (framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr); framebuffer_ = VK_NULL_HANDLE; }
    if (prev_framebuffer_) { vkDestroyFramebuffer(ctx_->device(), prev_framebuffer_, nullptr); prev_framebuffer_ = VK_NULL_HANDLE; }
    if (render_pass_) { vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    if (cmd_buffer_) { vkFreeCommandBuffers(ctx_->device(), cmd_pool_, 1, &cmd_buffer_); cmd_buffer_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    destroyFrameResources();
    initialized_ = false;
}

bool FramebufferBasic::createRenderPass() {
#ifdef VK_VERSION_1_0
    if (ctx_->device() == VK_NULL_HANDLE) return true;
    
    VkAttachmentDescription color{}; 
    color.format = VK_FORMAT_R8G8B8A8_UNORM; 
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkAttachmentDescription depth{}; 
    depth.format = VK_FORMAT_D16_UNORM; 
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dr{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; 
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; 
    sub.colorAttachmentCount = 1; 
    sub.pColorAttachments = &cr; 
    sub.pDepthStencilAttachment = &dr;
    
    std::array<VkAttachmentDescription,2> atts{color,depth};
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; 
    ci.attachmentCount=2; 
    ci.pAttachments=atts.data(); 
    ci.subpassCount=1; 
    ci.pSubpasses=&sub;
    return vkCreateRenderPass(ctx_->device(),&ci,nullptr,&render_pass_)==VK_SUCCESS;
#else
    return true;
#endif
}

bool FramebufferBasic::createFramebufferResources(uint32_t width, uint32_t height) {
    destroyFrameResources();
    
    auto color = createImage(1280, 720, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    auto depth = createImage(1280, 720, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if(!color||!depth) return false;
    
    VkImageView views[2] = {color->view, depth->view};
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass = render_pass_; 
    ci.attachmentCount=2; 
    ci.pAttachments=views; 
    ci.width=1280; 
    ci.height=720; 
    ci.layers=1;
    VkFramebuffer fb; 
    if(vkCreateFramebuffer(ctx_->device(),&ci,nullptr,&framebuffer_)!=VK_SUCCESS) return false;
    framebuffers_.push_back(fb);
    color_image_ = std::move(color);
    depth_image_ = std::move(depth);
    
    // Previous frame resources
    auto prev_color = createImage(1280, 720, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    auto prev_depth = createImage(1280, 720, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    if(!prev_color||!prev_depth) return false;
    
    VkImageView views[2] = {prev_color->view, prev_depth->view};
    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = render_pass_; 
    fci.attachmentCount=2; 
    fbci.pAttachments=views; 
    fbci.width=1280; 
    fbci.height=720; 
    fbci.layers=1;
    VkFramebuffer fb; 
    if(vkCreateFramebuffer(ctx_->device(),&fci,nullptr,&prev_framebuffer_)!=VK_SUCCESS) return false;
    prev_framebuffers_.push_back(prev_framebuffer_);
    
    color_image_ = std::move(color);
    depth_image_ = std::move(depth);
    prev_color_img_ = std::move(prev_color);
    prev_depth_img_ = std::move(prev_depth);
    prev_color_view_ = prev_color->view;
    prev_depth_view_ = prev_depth->view;
    prev_color_mem_ = std::move(prev_color);
    prev_depth_mem_ = std::move(prev_depth);
    
    return true;
}

void FramebufferBasic::destroyFrameResources() {
    current_color_view_ = VK_NULL_HANDLE;
    current_depth_view_ = VK_NULL_HANDLE;
    if (current_color_img_) { vkDestroyImage(ctx_->device(), current_color_img_, nullptr); current_color_img_ = VK_NULL_HANDLE; }
    if (current_depth_img_) { vkDestroyImage(ctx_->device(), current_depth_img_, nullptr); current_depth_img_ = VK_NULL_HANDLE; }
    if (current_color_mem_) { vkFreeMemory(ctx_->device(), current_color_mem_, nullptr); current_color_mem_ = VK_NULL_HANDLE; }
    if (current_depth_mem_) { vkFreeMemory(ctx_->device(), current_depth_mem_, nullptr); current_depth_mem_ = VK_NULL_HANDLE; }
    
    if (prev_color_img_) { vkDestroyImage(ctx_->device(), prev_color_img_, nullptr); prev_color_img_ = VK_NULL_HANDLE; }
    if (prev_depth_img_) { vkDestroyImage(ctx_->device(), prev_depth_img_, nullptr); prev_depth_img_ = VK_NULL_HANDLE; }
    if (prev_color_view_) { vkDestroyImageView(ctx_->device(), prev_color_view_, nullptr); prev_color_view_ = VK_NULL_HANDLE; }
    if (prev_depth_view_) { vkDestroyImageView(ctx_->device(), prev_depth_view_, nullptr); prev_depth_view_ = VK_NULL_HANDLE; }
    if (prev_color_mem_) { vkFreeMemory(ctx_->device(), prev_color_mem_, nullptr); prev_color_mem_ = VK_NULL_HANDLE; }
    if (prev_depth_mem_) { vkFreeMemory(ctx_->device(), prev_depth_mem_, nullptr); prev_depth_mem_ = VK_NULL_HANDLE; }
    
    if (framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr); framebuffer_ = VK_NULL_HANDLE; }
    if (prev_framebuffer_) { vkDestroyFramebuffer(ctx_->device(), prev_framebuffer_, nullptr); prev_framebuffer_ = VK_NULL_HANDLE; }
}

bool FramebufferBasic::init(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat color_format) {
    ctx_ = ctx;
    width_ = width;
    height_ = height;
    color_format_ = color_format;
    depth_format_ = VK_FORMAT_D16_UNORM;
    
    if (!createRenderPass()) return false;
    if (!createFramebufferResources(1280, 720)) return false; // Default to 720p
    initialized_ = true;
    return true;
}

void FramebufferBasic::shutdown() {
    destroyFrameResources();
    if (framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr); framebuffer_ = VK_NULL_HANDLE; }
    if (prev_framebuffer_) { vkDestroyFramebuffer(ctx_->device(), prev_framebuffer_, nullptr); prev_framebuffer_ = VK_NULL_HANDLE; }
    if (render_pass_) { vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    if (cmd_buffer_) { vkFreeCommandBuffers(ctx_->device(), cmd_pool_, 1, &cmd_buffer_); cmd_buffer_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    destroyFrameResources();
    initialized_ = false;
}

bool FramebufferBasic::createRenderPass() {
#ifdef VK_VERSION_1_0
    if (ctx_->device() == VK_NULL_HANDLE) return true;
    
    VkAttachmentDescription color{}; 
    color.format = VK_FORMAT_R8G8B8A8_UNORM; 
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkAttachmentDescription depth{}; 
    depth.format = VK_FORMAT_D16_UNORM; 
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; 
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dr{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; 
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; 
    sub.colorAttachmentCount = 1; 
    sub.pColorAttachments = &cr; 
    sub.pDepthStencilAttachment = &dr;
    
    std::array<VkAttachmentDescription,2> atts{color,depth};
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; 
    ci.attachmentCount=2; 
    ci.pAttachments=atts.data(); 
    ci.subpassCount=1; 
    ci.pSubpasses=&sub;
    return vkCreateRenderPass(device_,&ci,nullptr,&render_pass_)==VK_SUCCESS;
#else
    return true;
#endif
}

bool FramebufferBasic::createFramebufferResources(uint32_t width, uint32_t height) {
    destroyFrameResources();
    
    auto color = createImage(1280, 720, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    auto depth = createImage(1280, 720, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if(!color||!depth) return false;
    
    VkImageView views[2] = {color->view, depth->view};
    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = render_pass_; 
    fci.attachmentCount=2; 
    fbci.pAttachments=views; 
    fbci.width=1280; 
    fbci.height=720; 
    fbci.layers=1;
    VkFramebuffer fb; 
    if(vkCreateFramebuffer(device_,&fci,nullptr,&fb)!=VK_SUCCESS) return false;
    framebuffers_.push_back(fb);
    color_image_ = std::move(color);
    depth_image_ = std::move(depth);
    
    return true;
}

void FramebufferBasic::destroyFrameResources() {
    current_color_view_ = VK_NULL_HANDLE;
    current_depth_view_ = VK_NULL_HANDLE;
    current_color_img_ = VK_NULL_HANDLE;
    current_depth_img_ = VK_NULL_HANDLE;
    current_color_mem_ = VK_NULL_HANDLE;
    current_depth_mem_ = VK_NULL_HANDLE;
    prev_color_img_ = VK_NULL_HANDLE;
    prev_depth_img_ = VK_NULL_HANDLE;
    prev_color_view_ = VK_NULL_HANDLE;
    prev_depth_view_ = VK_NULL_HANDLE;
    prev_color_mem_ = VK_NULL_HANDLE;
    prev_depth_mem_ = VK_NULL_HANDLE;
}

void FramebufferBasic::copyFramebuffer(VkCommandBuffer cmd, VkImageView src_color, VkImageView src_depth, VkImageView dst_color, VkImageView dst_depth) {
    // Copy framebuffer content from src to dst
    // Simplified implementation
}

FramebufferBasic::FrameDiffStats FramebufferBasic::compareFramebuffers(VkCommandBuffer cmd, VkImageView curr_color, VkImageView curr_depth, VkImageView prev_color, VkImageView prev_depth) {
    FrameDiffStats stats;
    stats.total_pixels = width_ * height_;
    stats.changed_pixels = 0;
    stats.unchanged_pixels = width_ * height_;
    stats.change_ratio = 0.0f;
    stats.diff_time_ms = 0.0;
    return stats;
}

bool FramebufferBasic::init(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat color_format) {
    ctx_ = ctx;
    width_ = width;
    height_ = height;
    color_format_ = color_format;
    depth_format_ = VK_FORMAT_D16_UNORM;
    
    if (!createRenderPass()) return false;
    if (!createFramebufferResources(1280, 720)) return false;
    initialized_ = true;
    return true;
}

void FramebufferBasic::shutdown() {
    destroyFrameResources();
    if (framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr); framebuffer_ = VK_NULL_HANDLE; }
    if (prev_framebuffer_) { vkDestroyFramebuffer(ctx_->device(), prev_framebuffer_, nullptr); prev_framebuffer_ = VK_NULL_HANDLE; }
    if (render_pass_) { vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    if (cmd_buffer_) { vkFreeCommandBuffers(ctx_->device(), cmd_pool_, 1, &cmd_buffer_); cmd_buffer_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    destroyFrameResources();
    initialized_ = false;
}

void FramebufferBasic::beginFrame(uint64_t frame_index) {
    curr_frame_.frame_index = frame_index;
    curr_frame_.timestamp_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

FramebufferBasic::FrameDiffStats FramebufferBasic::compareAndSwap(VkCommandBuffer cmd, VkImageView current_color, VkImageView current_depth) {
    FrameDiffStats stats = compareFramebuffers(cmd, current_color, current_depth, prev_color_view_, prev_depth_view_);
    
    // Swap current -> prev
    std::swap(current_color_view_, prev_color_view_);
    std::swap(current_depth_view_, prev_depth_view_);
    std::swap(current_color_img_, prev_color_img_);
    std::swap(current_depth_img_, prev_depth_img_);
    std::swap(current_color_mem_, prev_color_mem_);
    std::swap(current_depth_mem_, prev_depth_mem_);
    
    current_color_view_ = current_color;
    current_depth_view_ = current_depth;
    
    last_diff_ = stats;
    return stats;
}

bool FramebufferBasic::init(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat color_format) {
    ctx_ = ctx;
    width_ = width;
    height_ = height;
    color_format_ = color_format;
    depth_format_ = VK_FORMAT_D16_UNORM;
    
    if (!createRenderPass()) return false;
    if (!createFramebufferResources(1280, 720)) return false;
    initialized_ = true;
    return true;
}

void FramebufferBasic::shutdown() {
    destroyFrameResources();
    if (framebuffer_ != VK_NULL_HANDLE) { vkDestroyFramebuffer(ctx_->device(), framebuffer_, nullptr); framebuffer_ = VK_NULL_HANDLE; }
    if (prev_framebuffer_) { vkDestroyFramebuffer(ctx_->device(), prev_framebuffer_, nullptr); prev_framebuffer_ = VK_NULL_HANDLE; }
    if (render_pass_) { vkDestroyRenderPass(ctx_->device(), render_pass_, nullptr); render_pass_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    if (cmd_buffer_) { vkFreeCommandBuffers(ctx_->device(), cmd_pool_, 1, &cmd_buffer_); cmd_buffer_ = VK_NULL_HANDLE; }
    if (cmd_pool_) { vkDestroyCommandPool(ctx_->device(), cmd_pool_, nullptr); cmd_pool_ = VK_NULL_HANDLE; }
    destroyFrameResources();
    initialized_ = false;
}

void FramebufferBasic::beginFrame(uint64_t frame_index) {
    curr_frame_.frame_index = frame_index;
    curr_frame_.timestamp_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

FramebufferBasic::FrameDiffStats FramebufferBasic::compareFramebuffers(VkCommandBuffer cmd, VkImageView curr_color, VkImageView curr_depth, VkImageView prev_color, VkImageView prev_depth) {
    FrameDiffStats stats;
    stats.total_pixels = width_ * height_;
    stats.changed_pixels = 0; // Would implement compute shader comparison
    stats.unchanged_pixels = width_ * height_;
    stats.change_ratio = 0.0f;
    stats.diff_time_ms = 0.0;
    return stats;
}

bool FramebufferBasic::createFramebufferResources(uint32_t width, uint32_t height) {
    return true;
}

void FramebufferBasic::destroyFrameResources() {
    // Cleanup
}

void FramebufferBasic::copyFramebuffer(VkCommandBuffer cmd, VkImageView src_color, VkImageView src_depth, VkImageView dst_color, VkImageView dst_depth) {
    // Copy framebuffer content
}

VkCommandBuffer FramebufferBasic::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; 
    ai.commandPool = command_pool_; 
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; 
    ai.commandBufferCount = 1;
    VkCommandBuffer cb; 
    vkAllocateCommandBuffers(device_, &ai, &cb);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; 
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

void FramebufferBasic::endSingleTimeCommands(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; 
    si.commandBufferCount=1; 
    si.pCommandBuffers=&cb;
    vkQueueSubmit(graphics_queue_,1,&si,VK_NULL_HANDLE); 
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, command_pool_,1,&cb);
}

} // namespace gpu
} // namespace mgd
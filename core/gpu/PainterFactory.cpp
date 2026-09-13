// PainterFactory.cpp - Factory for creating painter instances
#include "Painter.h"
#include "SimplePainter.cpp"
#include "IntelligentPainter.h"
#include <memory>

namespace mgd {
namespace gpu {

class PainterFactory {
public:
    static std::unique_ptr<Painter> createPainter(Painter::Type type, 
                                                   VulkanContext* ctx, 
                                                   FramebufferManager* fb_mgr) {
        std::unique_ptr<Painter> painter;
        
        switch (type) {
            case Painter::Type::SIMPLE: {
                auto painter = std::make_unique<SimplePainter>();
                if (!painter->init(ctx, fb_mgr)) return nullptr;
                return painter;
            }
            
            case Painter::Type::INTELLIGENT: {
                auto painter = std::make_unique<IntelligentPainter>();
                if (!painter->init(ctx, fb_mgr)) return nullptr;
                return painter;
            }
            
            case Painter::Type::LEGACY:
            default: {
                auto painter = std::make_unique<SimplePainter>();
                if (!painter->init(ctx, fb_mgr)) return nullptr;
                return painter;
            }
        }
    }
    
    static std::unique_ptr<Painter> createFromConfig(const PainterConfig& config,
                                                      VulkanContext* ctx, 
                                                      FramebufferManager* fb_mgr) {
        return createPainter(config.type, ctx, fb_mgr);
    }
};

} // namespace gpu
} // namespace mgd
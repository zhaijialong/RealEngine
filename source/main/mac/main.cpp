#include "core/engine.h"
#include "Foundation/Foundation.hpp"
#include "AppKit/AppKit.hpp"
#include "MetalKit/MetalKit.hpp"

class ViewDelegate : public MTK::ViewDelegate
{
public:
    virtual void drawInMTKView(MTK::View* pView) override
    {
        Engine::GetInstance()->Tick();
    }
    
    virtual void drawableSizeWillChange(MTK::View* pView, CGSize size) override
    {
        Engine::GetInstance()->WindowResizeSignal(pView, size.width, size.height);
    }
};

inline eastl::string GetWorkPath()
{
    NS::Bundle* bundle = NS::Bundle::mainBundle();
    eastl::string path = bundle->bundlePath()->utf8String();
    
    size_t last_slash = path.find_last_of('/');
    return path.substr(0, last_slash + 1);
}

class AppDelegate : public NS::ApplicationDelegate
{
public:
    ~AppDelegate()
    {
        m_pView->release();
        m_pWindow->release();
        delete m_pViewDelegate;
    }

    virtual void applicationWillFinishLaunching(NS::Notification* pNotification) override
    {
    }
    
    virtual void applicationDidFinishLaunching(NS::Notification* pNotification) override
    {
        CGRect frame = { {200.0, 200.0}, {1280, 800.0} };

        m_pView = MTK::View::alloc()->init(frame, nullptr);
        m_pViewDelegate = new ViewDelegate();
        m_pView->setDelegate(m_pViewDelegate);
        
        m_pWindow = NS::Window::alloc()->init(frame,
                NS::WindowStyleMaskClosable | NS::WindowStyleMaskMiniaturizable | NS::WindowStyleMaskResizable |NS::WindowStyleMaskTitled,
                NS::BackingStoreBuffered, false);
        m_pWindow->setContentView(m_pView);
        m_pWindow->setTitle(NS::String::string("RealEngine", NS::StringEncoding::UTF8StringEncoding));
        m_pWindow->makeKeyAndOrderFront(nullptr);

        Engine::GetInstance()->Init(GetWorkPath(), m_pView, m_pView->drawableSize().width, m_pView->drawableSize().height);
    }
    
    virtual bool applicationShouldTerminateAfterLastWindowClosed(NS::Application* pSender) override
    {
        Engine::GetInstance()->Shut();
        return true;
    }

private:
    NS::Window* m_pWindow = nullptr;
    MTK::View* m_pView = nullptr;
    ViewDelegate* m_pViewDelegate = nullptr;
};

int main(int argc, char* argv[])
{
    rpmalloc_initialize();
    
    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    AppDelegate delegate;
    
    NS::Application* pSharedApplication = NS::Application::sharedApplication();
    pSharedApplication->setDelegate(&delegate);
    pSharedApplication->run();
    
    pAutoreleasePool->release();
    
    return 0;
}

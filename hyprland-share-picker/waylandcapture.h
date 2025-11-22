#pragma once

#include <QObject>
#include <QImage>
#include <QMap>
#include <QSocketNotifier>
#include <wayland-client.h>
#include <hyprutils/memory/WeakPtr.hpp>

using namespace Hyprutils::Memory;
#define SP CSharedPointer
#define WP CWeakPointer

#include "protocols/hyprland-toplevel-export-v1.hpp"
#include "protocols/wayland.hpp"

class WaylandCapture : public QObject {
    Q_OBJECT
public:
    WaylandCapture(QObject* parent = nullptr);
    ~WaylandCapture();

    void capture(unsigned long long handle);

signals:
    void frameCaptured(unsigned long long handle, QImage image);

private:
    wl_display* m_display = nullptr;
    
    SP<CCWlRegistry> m_registry;
    SP<CCWlShm> m_shm;
    SP<CCHyprlandToplevelExportManagerV1> m_manager;

    struct CaptureSession {
        SP<CCHyprlandToplevelExportFrameV1> frame;
        SP<CCWlBuffer> buffer;
        void* data = nullptr;
        int width = 0, height = 0, stride = 0, size = 0;
        uint32_t format = 0;
        
        ~CaptureSession();
    };

    QMap<unsigned long long, std::shared_ptr<CaptureSession>> m_sessions;

    void init();
    void processEvents();
    
    QSocketNotifier* m_socketNotifier = nullptr;
};

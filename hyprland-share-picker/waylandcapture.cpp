#include "waylandcapture.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <random>
#include <QTimer>

static std::string getRandName() {
    static const char charset[] = "0123456789"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    std::string str = "/xdph-picker-";
    for (int i = 0; i < 8; ++i)
        str += charset[rand() % max_index];
    return str;
}

static int create_shm_file(size_t size) {
    int retries = 100;
    do {
        std::string name = getRandName();
        int fd = shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name.c_str());
            if (ftruncate(fd, size) < 0) {
                close(fd);
                return -1;
            }
            return fd;
        }
    } while (--retries > 0 && errno == EEXIST);
    return -1;
}

WaylandCapture::CaptureSession::~CaptureSession() {
    if (data && data != MAP_FAILED) {
        munmap(data, size);
    }
}

WaylandCapture::WaylandCapture(QObject* parent) : QObject(parent) {
    init();
}

WaylandCapture::~WaylandCapture() {
    if (m_socketNotifier) {
        m_socketNotifier->setEnabled(false);
        delete m_socketNotifier;
        m_socketNotifier = nullptr;
    }
    
    m_sessions.clear(); 
    
    // Reset protocol wrappers before disconnecting display
    // This ensures we don't leave dangling pointers to proxies that are about to be freed
    m_manager.reset();
    m_shm.reset();
    m_registry.reset();

    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

void WaylandCapture::init() {
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        std::cerr << "[picker] failed to connect to wayland display" << std::endl;
        return;
    }

    auto registry = wl_display_get_registry(m_display);
    m_registry = makeShared<CCWlRegistry>((wl_proxy*)registry);

    m_registry->setGlobal([this](CCWlRegistry* r, uint32_t name, const char* interface, uint32_t version) {
        std::string iface = interface;
        if (iface == "wl_shm") {
            m_shm = makeShared<CCWlShm>((wl_proxy*)wl_registry_bind((wl_registry*)r->resource(), name, &wl_shm_interface, 1));
        } else if (iface == "hyprland_toplevel_export_manager_v1") {
            m_manager = makeShared<CCHyprlandToplevelExportManagerV1>((wl_proxy*)wl_registry_bind((wl_registry*)r->resource(), name, &hyprland_toplevel_export_manager_v1_interface, 2));
        }
    });

    wl_display_roundtrip(m_display);

    // Setup socket notifier
    m_socketNotifier = new QSocketNotifier(wl_display_get_fd(m_display), QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &WaylandCapture::processEvents);
}

void WaylandCapture::processEvents() {
    if (m_display) {
        if (wl_display_dispatch(m_display) < 0) {
             // error or disconnected
        }
    }
}

void WaylandCapture::capture(unsigned long long handle) {
    if (!m_manager || !m_shm) return;

    if (m_sessions.contains(handle)) {
        m_sessions.remove(handle);
    }

    auto frame = makeShared<CCHyprlandToplevelExportFrameV1>(m_manager->sendCaptureToplevel(0, handle));
    auto session = std::make_shared<CaptureSession>();
    session->frame = frame;
    m_sessions[handle] = session;

    frame->setBuffer([this, handle](CCHyprlandToplevelExportFrameV1* r, uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
        if (!m_sessions.contains(handle)) return;
        auto& s = m_sessions[handle];
        s->width = width;
        s->height = height;
        s->stride = stride;
        s->format = format;
        s->size = stride * height;
    });
    
    frame->setBufferDone([this, handle](CCHyprlandToplevelExportFrameV1* r) {
        if (!m_sessions.contains(handle)) return;
        auto& s = m_sessions[handle];

        if (s->size <= 0) return;

        // Create buffer
        int fd = create_shm_file(s->size);
        if (fd < 0) return;

        s->data = mmap(NULL, s->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (s->data == MAP_FAILED) {
            close(fd);
            return;
        }

        auto pool = makeShared<CCWlShmPool>(m_shm->sendCreatePool(fd, s->size));
        s->buffer = makeShared<CCWlBuffer>(pool->sendCreateBuffer(0, s->width, s->height, s->stride, s->format));
        
        pool->sendDestroy(); // Destroy pool on server side
        pool.reset();        // Destroy wrapper

        r->sendCopy(s->buffer->proxy(), 0);
        close(fd);
        wl_display_flush(m_display);
    });

    frame->setReady([this, handle](CCHyprlandToplevelExportFrameV1* r, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
        if (!m_sessions.contains(handle)) return;
        auto s = m_sessions[handle]; // Keep session alive during this scope

        QImage::Format fmt = QImage::Format_ARGB32; 
        if (s->format == WL_SHM_FORMAT_XRGB8888)
            fmt = QImage::Format_RGB32;
        else if (s->format == WL_SHM_FORMAT_ARGB8888)
            fmt = QImage::Format_ARGB32_Premultiplied; 
            
        QImage img((uchar*)s->data, s->width, s->height, s->stride, fmt);
        
        // we need to copy bits because we will destroy session (and unmap)
        QImage copy = img.copy();

        r->sendDestroy();
        
        emit frameCaptured(handle, copy);

        // Defer removal
        QTimer::singleShot(0, this, [this, handle]() {
            m_sessions.remove(handle);
        });
    });

    frame->setFailed([this, handle](CCHyprlandToplevelExportFrameV1* r) {
        if (m_sessions.contains(handle)) {
            m_sessions[handle]->frame->sendDestroy();
            QTimer::singleShot(0, this, [this, handle]() {
                m_sessions.remove(handle);
            });
        }
    });
    
    wl_display_flush(m_display);
}

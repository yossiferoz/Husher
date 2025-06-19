#include <clap/clap.h>
#include <cstring>
#include <atomic>

namespace KhDetector {

// Plugin descriptor
static const clap_plugin_descriptor_t s_descriptor = {
    CLAP_VERSION_INIT,
    "com.khdetector.clap",
    "KhDetector CLAP",
    "KhDetector",
    "https://github.com/khdetector/khdetector",
    "",
    "",
    "1.0.0",
    "Audio analysis and hit detection plugin",
    0
};

class KhDetectorClapPlugin {
public:
    KhDetectorClapPlugin(const clap_host_t* host) : mHost(host), mBypass(false), mSensitivity(0.6f), mHadHit(false) {}
    ~KhDetectorClapPlugin() = default;

    bool init() { return true; }
    void destroy() {}
    bool activate(double /*sample_rate*/, uint32_t /*min_frames*/, uint32_t /*max_frames*/) { return true; }
    void deactivate() {}
    bool start_processing() { return true; }
    void stop_processing() {}
    void reset() { mHadHit.store(false); }

    clap_process_status process(const clap_process_t* process) {
        // Simple pass-through with basic hit detection
        if (process->audio_inputs_count > 0 && process->audio_outputs_count > 0) {
            const float* input = process->audio_inputs[0].data32[0];
            float* output = process->audio_outputs[0].data32[0];
            
            // Copy input to output
            memcpy(output, input, process->frames_count * sizeof(float));
            
            // Simple energy-based hit detection
            float energy = 0.0f;
            for (uint32_t i = 0; i < process->frames_count; ++i) {
                energy += input[i] * input[i];
            }
            energy /= process->frames_count;
            
            if (energy > mSensitivity) {
                mHadHit.store(true);
            }
        }
        return CLAP_PROCESS_CONTINUE;
    }

    const void* get_extension(const char* /*id*/) { return nullptr; }

private:
    const clap_host_t* mHost;
    bool mBypass;
    float mSensitivity;
    std::atomic<bool> mHadHit;
};

} // namespace KhDetector

// Plugin interface
static bool plugin_init(const clap_plugin_t* plugin) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->init();
}

static void plugin_destroy(const clap_plugin_t* plugin) {
    auto* p = static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data);
    p->destroy();
    delete p;
}

static bool plugin_activate(const clap_plugin_t* plugin, double sample_rate, uint32_t min_frames, uint32_t max_frames) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->activate(sample_rate, min_frames, max_frames);
}

static void plugin_deactivate(const clap_plugin_t* plugin) {
    static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->deactivate();
}

static bool plugin_start_processing(const clap_plugin_t* plugin) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->start_processing();
}

static void plugin_stop_processing(const clap_plugin_t* plugin) {
    static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->stop_processing();
}

static void plugin_reset(const clap_plugin_t* plugin) {
    static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->reset();
}

static clap_process_status plugin_process(const clap_plugin_t* plugin, const clap_process_t* process) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->process(process);
}

static const void* plugin_get_extension(const clap_plugin_t* plugin, const char* id) {
    return static_cast<KhDetector::KhDetectorClapPlugin*>(plugin->plugin_data)->get_extension(id);
}

static void plugin_on_main_thread(const clap_plugin_t* /*plugin*/) {}

static const clap_plugin_t s_plugin_class = {
    &KhDetector::s_descriptor,
    nullptr,
    plugin_init,
    plugin_destroy,
    plugin_activate,
    plugin_deactivate,
    plugin_start_processing,
    plugin_stop_processing,
    plugin_reset,
    plugin_process,
    plugin_get_extension,
    plugin_on_main_thread
};

// Factory
static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* /*factory*/) { return 1; }

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(const clap_plugin_factory_t* /*factory*/, uint32_t index) {
    return index == 0 ? &KhDetector::s_descriptor : nullptr;
}

static const clap_plugin_t* factory_create_plugin(const clap_plugin_factory_t* /*factory*/, const clap_host_t* host, const char* plugin_id) {
    if (strcmp(plugin_id, KhDetector::s_descriptor.id) == 0) {
        auto* plugin_instance = new KhDetector::KhDetectorClapPlugin(host);
        clap_plugin_t* plugin = new clap_plugin_t;
        *plugin = s_plugin_class;
        plugin->plugin_data = plugin_instance;
        return plugin;
    }
    return nullptr;
}

static const clap_plugin_factory_t s_plugin_factory = {
    factory_get_plugin_count,
    factory_get_plugin_descriptor,
    factory_create_plugin
};

// Entry point
extern "C" {
CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    [](const char* /*plugin_path*/) -> bool { return true; },
    []() {},
    [](const char* factory_id) -> const void* {
        return strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0 ? &s_plugin_factory : nullptr;
    }
};
}

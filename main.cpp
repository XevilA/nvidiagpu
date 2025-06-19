#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include <map>

// Cross-platform headers
#ifdef _WIN32
    #include <windows.h>
    #include <d3d11.h>
    #include <dxgi.h>
    #pragma comment(lib, "d3d11.lib")
    #pragma comment(lib, "dxgi.lib")
#elif __APPLE__
    #include <Metal/Metal.h>
    #include <IOKit/IOKitLib.h>
#endif

// Dear ImGui for modern GUI
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <GL/gl3w.h>

// NVIDIA Management Library (NVML) for GPU control
#ifdef _WIN32
    #include <nvml.h>
    #pragma comment(lib, "nvml.lib")
#endif

class GPUInfo {
public:
    std::string name;
    std::string driver_version;
    int temperature = 0;
    int memory_used = 0;
    int memory_total = 0;
    int gpu_utilization = 0;
    int memory_utilization = 0;
    int power_usage = 0;
    int power_limit = 0;
    int core_clock = 0;
    int memory_clock = 0;
    int fan_speed = 0;
    bool is_nvidia = false;
    
    // Tuning parameters
    int target_core_clock = 0;
    int target_memory_clock = 0;
    int target_fan_curve[5] = {30, 40, 50, 70, 85}; // Fan speeds at different temps
    int target_power_limit = 100; // Percentage
};

class GPUMonitor {
private:
    std::vector<GPUInfo> gpus;
    bool nvml_initialized = false;
    
public:
    GPUMonitor() {
        initializeNVML();
        detectGPUs();
    }
    
    ~GPUMonitor() {
        if (nvml_initialized) {
#ifdef _WIN32
            nvmlShutdown();
#endif
        }
    }
    
    void initializeNVML() {
#ifdef _WIN32
        nvmlReturn_t result = nvmlInit();
        if (result == NVML_SUCCESS) {
            nvml_initialized = true;
            std::cout << "NVML initialized successfully" << std::endl;
        } else {
            std::cout << "Failed to initialize NVML: " << nvmlErrorString(result) << std::endl;
        }
#endif
    }
    
    void detectGPUs() {
        gpus.clear();
        
#ifdef _WIN32
        if (nvml_initialized) {
            unsigned int device_count;
            nvmlReturn_t result = nvmlDeviceGetCount(&device_count);
            
            if (result == NVML_SUCCESS) {
                for (unsigned int i = 0; i < device_count; i++) {
                    nvmlDevice_t device;
                    result = nvmlDeviceGetHandleByIndex(i, &device);
                    
                    if (result == NVML_SUCCESS) {
                        GPUInfo gpu;
                        gpu.is_nvidia = true;
                        
                        char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                        nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
                        gpu.name = std::string(name);
                        
                        // Get driver version
                        char version[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
                        nvmlSystemGetDriverVersion(version, NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE);
                        gpu.driver_version = std::string(version);
                        
                        updateGPUInfo(gpu, device, i);
                        gpus.push_back(gpu);
                    }
                }
            }
        }
#elif __APPLE__
        // macOS Metal GPU detection
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device) {
            GPUInfo gpu;
            gpu.name = std::string([device.name UTF8String]);
            gpu.is_nvidia = false; // Apple Silicon or AMD
            gpus.push_back(gpu);
        }
#endif
    }
    
    void updateGPUInfo(GPUInfo& gpu, void* device_handle, int index) {
#ifdef _WIN32
        if (!nvml_initialized) return;
        
        nvmlDevice_t device = static_cast<nvmlDevice_t>(device_handle);
        
        // Temperature
        unsigned int temp;
        if (nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
            gpu.temperature = temp;
        }
        
        // Memory info
        nvmlMemory_t memory;
        if (nvmlDeviceGetMemoryInfo(device, &memory) == NVML_SUCCESS) {
            gpu.memory_used = memory.used / (1024 * 1024); // Convert to MB
            gpu.memory_total = memory.total / (1024 * 1024);
        }
        
        // Utilization
        nvmlUtilization_t util;
        if (nvmlDeviceGetUtilizationRates(device, &util) == NVML_SUCCESS) {
            gpu.gpu_utilization = util.gpu;
            gpu.memory_utilization = util.memory;
        }
        
        // Power usage
        unsigned int power;
        if (nvmlDeviceGetPowerUsage(device, &power) == NVML_SUCCESS) {
            gpu.power_usage = power / 1000; // Convert to watts
        }
        
        // Power limit
        unsigned int power_limit;
        if (nvmlDeviceGetPowerManagementLimitConstraints(device, &power_limit, &power_limit) == NVML_SUCCESS) {
            gpu.power_limit = power_limit / 1000;
        }
        
        // Clock speeds
        unsigned int clock;
        if (nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &clock) == NVML_SUCCESS) {
            gpu.core_clock = clock;
        }
        if (nvmlDeviceGetClockInfo(device, NVML_CLOCK_MEM, &clock) == NVML_SUCCESS) {
            gpu.memory_clock = clock;
        }
        
        // Fan speed
        unsigned int fan;
        if (nvmlDeviceGetFanSpeed(device, &fan) == NVML_SUCCESS) {
            gpu.fan_speed = fan;
        }
#endif
    }
    
    void updateAllGPUs() {
#ifdef _WIN32
        if (!nvml_initialized) return;
        
        for (size_t i = 0; i < gpus.size(); i++) {
            nvmlDevice_t device;
            if (nvmlDeviceGetHandleByIndex(i, &device) == NVML_SUCCESS) {
                updateGPUInfo(gpus[i], device, i);
            }
        }
#endif
    }
    
    bool applyGPUSettings(int gpu_index, const GPUInfo& settings) {
#ifdef _WIN32
        if (!nvml_initialized || gpu_index >= gpus.size()) return false;
        
        nvmlDevice_t device;
        if (nvmlDeviceGetHandleByIndex(gpu_index, &device) != NVML_SUCCESS) {
            return false;
        }
        
        // Apply power limit
        if (settings.target_power_limit > 0) {
            unsigned int power_limit = settings.target_power_limit * gpus[gpu_index].power_limit / 100;
            nvmlDeviceSetPowerManagementLimitConstraints(device, power_limit * 1000, power_limit * 1000);
        }
        
        // Apply clock speeds (requires admin privileges)
        if (settings.target_core_clock > 0) {
            nvmlDeviceSetApplicationsClocks(device, settings.target_memory_clock, settings.target_core_clock);
        }
        
        return true;
#endif
        return false;
    }
    
    std::vector<GPUInfo>& getGPUs() { return gpus; }
};

class GPUTuneApp {
private:
    GLFWwindow* window;
    GPUMonitor monitor;
    bool show_about = false;
    int selected_gpu = 0;
    std::chrono::steady_clock::time_point last_update;
    
    // Theme colors
    ImVec4 primary_color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
    ImVec4 secondary_color = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    ImVec4 accent_color = ImVec4(0.0f, 0.8f, 0.4f, 1.0f);
    ImVec4 warning_color = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
    ImVec4 danger_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    
public:
    GPUTuneApp() {
        initializeGLFW();
        setupImGui();
        last_update = std::chrono::steady_clock::now();
    }
    
    ~GPUTuneApp() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    void initializeGLFW() {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        
        window = glfwCreateWindow(1400, 900, "GPUTune - GPU Monitor & Tuning Tool", NULL, NULL);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            exit(-1);
        }
        
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync
        
        if (gl3wInit() != 0) {
            std::cerr << "Failed to initialize OpenGL loader" << std::endl;
            exit(-1);
        }
    }
    
    void setupImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        
        // Setup modern dark style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Modern styling
        style.WindowRounding = 8.0f;
        style.FrameRounding = 6.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 6.0f;
        style.WindowBorderSize = 0.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 0.0f;
        style.WindowPadding = ImVec2(12, 12);
        style.FramePadding = ImVec2(12, 6);
        style.ItemSpacing = ImVec2(12, 6);
        style.ItemInnerSpacing = ImVec2(6, 6);
        style.IndentSpacing = 25.0f;
        style.ScrollbarSize = 16.0f;
        style.GrabMinSize = 12.0f;
        
        // Custom color scheme
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.5f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
        colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 0.5f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.8f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 0.8f);
        colors[ImGuiCol_TitleBg] = secondary_color;
        colors[ImGuiCol_TitleBgActive] = primary_color;
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = primary_color;
        colors[ImGuiCol_ScrollbarGrabActive] = primary_color;
        colors[ImGuiCol_CheckMark] = primary_color;
        colors[ImGuiCol_SliderGrab] = primary_color;
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = primary_color;
        colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = primary_color;
        colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
        colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        colors[ImGuiCol_TabHovered] = primary_color;
        colors[ImGuiCol_TabActive] = primary_color;
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }
    
    void drawProgressBar(const char* label, float value, float max_value, ImVec4 color, const char* overlay = nullptr) {
        ImGui::Text("%s", label);
        ImGui::SameLine(200);
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
        char buf[32];
        if (overlay) {
            sprintf(buf, "%s", overlay);
        } else {
            sprintf(buf, "%.1f/%.1f", value, max_value);
        }
        ImGui::ProgressBar(value / max_value, ImVec2(-1, 0), buf);
        ImGui::PopStyleColor();
    }
    
    void drawMetricCard(const char* title, const char* value, const char* unit, ImVec4 color) {
        ImGui::BeginChild(title, ImVec2(180, 80), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", title);
        ImGui::PopStyleColor();
        
        ImGui::PushFont(nullptr); // Use default font for now, could load larger font
        ImGui::Text("%s %s", value, unit);
        ImGui::PopFont();
        
        ImGui::EndChild();
    }
    
    void drawGPUMonitoring() {
        auto& gpus = monitor.getGPUs();
        if (gpus.empty()) {
            ImGui::Text("No NVIDIA GPUs detected or NVML not available");
            return;
        }
        
        // GPU Selection
        if (gpus.size() > 1) {
            std::vector<std::string> gpu_names;
            for (const auto& gpu : gpus) {
                gpu_names.push_back(gpu.name);
            }
            
            ImGui::Text("Select GPU:");
            ImGui::SameLine();
            
            std::vector<const char*> gpu_names_cstr;
            for (const auto& name : gpu_names) {
                gpu_names_cstr.push_back(name.c_str());
            }
            
            ImGui::Combo("##gpu_select", &selected_gpu, gpu_names_cstr.data(), gpu_names_cstr.size());
            ImGui::Separator();
        }
        
        if (selected_gpu >= gpus.size()) selected_gpu = 0;
        const auto& gpu = gpus[selected_gpu];
        
        // GPU Information Header
        ImGui::Text("GPU: %s", gpu.name.c_str());
        ImGui::Text("Driver: %s", gpu.driver_version.c_str());
        ImGui::Separator();
        
        // Metrics Cards Row 1
        ImGui::BeginGroup();
        {
            char temp_str[16], util_str[16], power_str[16], mem_str[16];
            sprintf(temp_str, "%d", gpu.temperature);
            sprintf(util_str, "%d", gpu.gpu_utilization);
            sprintf(power_str, "%d", gpu.power_usage);
            sprintf(mem_str, "%d", gpu.memory_used);
            
            ImVec4 temp_color = gpu.temperature > 80 ? danger_color : (gpu.temperature > 70 ? warning_color : accent_color);
            ImVec4 util_color = gpu.gpu_utilization > 90 ? warning_color : primary_color;
            ImVec4 power_color = gpu.power_usage > (gpu.power_limit * 0.9f) ? warning_color : accent_color;
            
            drawMetricCard("Temperature", temp_str, "°C", temp_color);
            ImGui::SameLine();
            drawMetricCard("GPU Usage", util_str, "%", util_color);
            ImGui::SameLine();
            drawMetricCard("Power", power_str, "W", power_color);
            ImGui::SameLine();
            drawMetricCard("Memory", mem_str, "MB", primary_color);
        }
        ImGui::EndGroup();
        
        ImGui::Spacing();
        
        // Metrics Cards Row 2
        ImGui::BeginGroup();
        {
            char core_clock_str[16], mem_clock_str[16], fan_str[16], mem_util_str[16];
            sprintf(core_clock_str, "%d", gpu.core_clock);
            sprintf(mem_clock_str, "%d", gpu.memory_clock);
            sprintf(fan_str, "%d", gpu.fan_speed);
            sprintf(mem_util_str, "%d", gpu.memory_utilization);
            
            drawMetricCard("Core Clock", core_clock_str, "MHz", accent_color);
            ImGui::SameLine();
            drawMetricCard("Mem Clock", mem_clock_str, "MHz", accent_color);
            ImGui::SameLine();
            drawMetricCard("Fan Speed", fan_str, "%", primary_color);
            ImGui::SameLine();
            drawMetricCard("Mem Usage", mem_util_str, "%", primary_color);
        }
        ImGui::EndGroup();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Detailed Progress Bars
        ImGui::Text("Detailed Status");
        ImGui::Spacing();
        
        char temp_overlay[32], mem_overlay[32], power_overlay[32];
        sprintf(temp_overlay, "%d°C", gpu.temperature);
        sprintf(mem_overlay, "%d/%d MB", gpu.memory_used, gpu.memory_total);
        sprintf(power_overlay, "%d/%d W", gpu.power_usage, gpu.power_limit);
        
        ImVec4 temp_bar_color = gpu.temperature > 80 ? danger_color : (gpu.temperature > 70 ? warning_color : accent_color);
        
        drawProgressBar("Temperature:", gpu.temperature, 100, temp_bar_color, temp_overlay);
        drawProgressBar("GPU Utilization:", gpu.gpu_utilization, 100, primary_color);
        drawProgressBar("Memory Utilization:", gpu.memory_utilization, 100, primary_color);
        drawProgressBar("Memory Usage:", gpu.memory_used, gpu.memory_total, primary_color, mem_overlay);
        drawProgressBar("Power Usage:", gpu.power_usage, gpu.power_limit, accent_color, power_overlay);
        drawProgressBar("Fan Speed:", gpu.fan_speed, 100, primary_color);
    }
    
    void drawGPUTuning() {
        auto& gpus = monitor.getGPUs();
        if (gpus.empty()) {
            ImGui::Text("No NVIDIA GPUs detected for tuning");
            return;
        }
        
        if (selected_gpu >= gpus.size()) selected_gpu = 0;
        auto& gpu = gpus[selected_gpu];
        
        ImGui::Text("GPU Tuning for: %s", gpu.name.c_str());
        ImGui::Separator();
        
        // Warning notice
        ImGui::PushStyleColor(ImGuiCol_Text, warning_color);
        ImGui::Text("⚠️  WARNING: GPU tuning can damage your hardware if done incorrectly!");
        ImGui::Text("   Always monitor temperatures and start with small adjustments.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        
        // Core Clock Tuning
        ImGui::Text("Core Clock Adjustment");
        ImGui::Text("Current: %d MHz", gpu.core_clock);
        ImGui::SliderInt("Target Core Clock (MHz)", &gpu.target_core_clock, gpu.core_clock - 200, gpu.core_clock + 200);
        ImGui::Spacing();
        
        // Memory Clock Tuning
        ImGui::Text("Memory Clock Adjustment");
        ImGui::Text("Current: %d MHz", gpu.memory_clock);
        ImGui::SliderInt("Target Memory Clock (MHz)", &gpu.target_memory_clock, gpu.memory_clock - 500, gpu.memory_clock + 500);
        ImGui::Spacing();
        
        // Power Limit
        ImGui::Text("Power Limit");
        ImGui::Text("Current: %d W", gpu.power_limit);
        ImGui::SliderInt("Power Limit (%)", &gpu.target_power_limit, 50, 120);
        ImGui::Text("Target: %d W", (gpu.power_limit * gpu.target_power_limit) / 100);
        ImGui::Spacing();
        
        // Fan Curve
        ImGui::Text("Fan Curve (Temperature vs Fan Speed %)");
        ImGui::Text("30°C    50°C    65°C    75°C    85°C");
        for (int i = 0; i < 5; i++) {
            ImGui::PushID(i);
            ImGui::SliderInt("", &gpu.target_fan_curve[i], 0, 100);
            if (i < 4) ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::Spacing();
        
        // Apply Settings Button
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Apply Settings", ImVec2(150, 40))) {
            if (monitor.applyGPUSettings(selected_gpu, gpu)) {
                ImGui::OpenPopup("Success");
            } else {
                ImGui::OpenPopup("Error");
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Reset to Default", ImVec2(150, 40))) {
            gpu.target_core_clock = gpu.core_clock;
            gpu.target_memory_clock = gpu.memory_clock;
            gpu.target_power_limit = 100;
            for (int i = 0; i < 5; i++) {
                gpu.target_fan_curve[i] = 30 + (i * 15);
            }
        }
        
        // Popups
        if (ImGui::BeginPopupModal("Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Settings applied successfully!");
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Failed to apply settings.");
            ImGui::Text("Make sure you're running as administrator.");
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    
    void drawAbout() {
        if (!show_about) return;
        
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("About GPUTune", &show_about)) {
            ImGui::Text("GPUTune");
            ImGui::Text("Advanced GPU Monitoring & Tuning Tool");
            ImGui::Separator();
            
            ImGui::Text("Version: 1.0.0");
            ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);
            ImGui::Spacing();
            
            ImGui::Text("Developer:");
            ImGui::PushStyleColor(ImGuiCol_Text, primary_color);
            ImGui::Text("Tirawat Nantamas");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::Text("Features:");
            ImGui::BulletText("Real-time GPU monitoring");
            ImGui::BulletText("Temperature, utilization, and power tracking");
            ImGui::BulletText("Core and memory clock adjustment");
            ImGui::BulletText("Custom fan curves");
            ImGui::BulletText("Power limit management");
            ImGui::BulletText("Cross-platform support (Windows/macOS)");
            ImGui::Spacing();
            
            ImGui::Text("Supported GPUs:");
            ImGui::BulletText("NVIDIA GPUs (via NVML)");
            ImGui::BulletText("AMD GPUs (via ADL - planned)");
            ImGui::BulletText("Intel GPUs (via Intel GPU Control - planned)");
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Text, warning_color);
            ImGui::Text("⚠️  Use at your own risk!");
            ImGui::Text("GPU tuning can void warranty.");
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }
    
    void render() {
        // Update GPU data every second
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count() > 1000) {
            monitor.updateAllGPUs();
            last_update = now;
        }
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Main window with docking
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("GPUTune Main", nullptr, window_flags);
        ImGui::PopStyleVar(3);
        
        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Refresh GPUs", "F5")) {
                    monitor.detectGPUs();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("Reset All Settings")) {
                    // Reset all GPU settings to default
                }
                if (ImGui::MenuItem("Export Profile")) {
                    // Export current tuning profile
                }
                if (ImGui::MenuItem("Import Profile")) {
                    // Import tuning profile
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) {
                    show_about = true;
                }
                if (ImGui::MenuItem("Documentation")) {
                    // Open documentation
                }
                ImGui::EndMenu();
            }
            
            // Status indicators in menu bar
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 300);
            auto& gpus = monitor.getGPUs();
            if (!gpus.empty() && selected_gpu < gpus.size()) {
                const auto& gpu = gpus[selected_gpu];
                
                // Temperature indicator
                ImVec4 temp_color = gpu.temperature > 80 ? danger_color : 
                                   (gpu.temperature > 70 ? warning_color : accent_color);
                ImGui::PushStyleColor(ImGuiCol_Text, temp_color);
                ImGui::Text("🌡️ %d°C", gpu.temperature);
                ImGui::PopStyleColor();
                
                ImGui::SameLine();
                
                // Power indicator
                ImGui::PushStyleColor(ImGuiCol_Text, accent_color);
                ImGui::Text("⚡ %dW", gpu.power_usage);
                ImGui::PopStyleColor();
                
                ImGui::SameLine();
                
                // GPU usage indicator
                ImVec4 usage_color = gpu.gpu_utilization > 90 ? warning_color : primary_color;
                ImGui::PushStyleColor(ImGuiCol_Text, usage_color);
                ImGui::Text("🎮 %d%%", gpu.gpu_utilization);
                ImGui::PopStyleColor();
            }
            
            ImGui::EndMenuBar();
        }
        
        // Create docking space
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        
        // Monitor Window
        ImGui::Begin("GPU Monitor", nullptr, ImGuiWindowFlags_None);
        drawGPUMonitoring();
        ImGui::End();
        
        // Tuning Window
        ImGui::Begin("GPU Tuning", nullptr, ImGuiWindowFlags_None);
        drawGPUTuning();
        ImGui::End();
        
        // Performance Graph Window
        ImGui::Begin("Performance Graphs", nullptr, ImGuiWindowFlags_None);
        drawPerformanceGraphs();
        ImGui::End();
        
        // System Info Window
        ImGui::Begin("System Information", nullptr, ImGuiWindowFlags_None);
        drawSystemInfo();
        ImGui::End();
        
        ImGui::End(); // Main window
        
        drawAbout();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.08f, 0.08f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    void drawPerformanceGraphs() {
        static std::vector<float> temp_history(100, 0.0f);
        static std::vector<float> gpu_util_history(100, 0.0f);
        static std::vector<float> power_history(100, 0.0f);
        static std::vector<float> memory_history(100, 0.0f);
        static int history_offset = 0;
        
        auto& gpus = monitor.getGPUs();
        if (!gpus.empty() && selected_gpu < gpus.size()) {
            const auto& gpu = gpus[selected_gpu];
            
            // Update history data
            temp_history[history_offset] = gpu.temperature;
            gpu_util_history[history_offset] = gpu.gpu_utilization;
            power_history[history_offset] = gpu.power_usage;
            memory_history[history_offset] = (float)gpu.memory_used / gpu.memory_total * 100.0f;
            
            history_offset = (history_offset + 1) % 100;
            
            ImGui::Text("Real-time Performance Graphs");
            ImGui::Separator();
            
            // Temperature Graph
            ImGui::Text("Temperature (°C)");
            ImGui::PlotLines("##temp", temp_history.data(), temp_history.size(), 
                           history_offset, nullptr, 0.0f, 100.0f, ImVec2(0, 80));
            
            // GPU Utilization Graph
            ImGui::Text("GPU Utilization (%%)");
            ImGui::PlotLines("##gpu_util", gpu_util_history.data(), gpu_util_history.size(), 
                           history_offset, nullptr, 0.0f, 100.0f, ImVec2(0, 80));
            
            // Power Usage Graph
            ImGui::Text("Power Usage (W)");
            ImGui::PlotLines("##power", power_history.data(), power_history.size(), 
                           history_offset, nullptr, 0.0f, (float)gpu.power_limit, ImVec2(0, 80));
            
            // Memory Usage Graph
            ImGui::Text("Memory Usage (%%)");
            ImGui::PlotLines("##memory", memory_history.data(), memory_history.size(), 
                           history_offset, nullptr, 0.0f, 100.0f, ImVec2(0, 80));
        } else {
            ImGui::Text("No GPU data available for graphing");
        }
    }
    
    void drawSystemInfo() {
        ImGui::Text("System Information");
        ImGui::Separator();
        
        // Operating System
        ImGui::Text("Operating System:");
        ImGui::SameLine(200);
#ifdef _WIN32
        ImGui::Text("Windows");
#elif __APPLE__
        ImGui::Text("macOS");
#elif __linux__
        ImGui::Text("Linux");
#else
        ImGui::Text("Unknown");
#endif
        
        // GPU Count
        auto& gpus = monitor.getGPUs();
        ImGui::Text("Detected GPUs:");
        ImGui::SameLine(200);
        ImGui::Text("%zu", gpus.size());
        
        // NVML Status
        ImGui::Text("NVML Status:");
        ImGui::SameLine(200);
        if (monitor.nvml_initialized) {
            ImGui::PushStyleColor(ImGuiCol_Text, accent_color);
            ImGui::Text("✓ Available");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, danger_color);
            ImGui::Text("✗ Not Available");
            ImGui::PopStyleColor();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // GPU Details List
        ImGui::Text("GPU Details");
        if (ImGui::BeginTable("gpu_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Memory");
            ImGui::TableSetupColumn("Driver");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();
            
            for (size_t i = 0; i < gpus.size(); i++) {
                const auto& gpu = gpus[i];
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                if (i == selected_gpu) {
                    ImGui::PushStyleColor(ImGuiCol_Text, primary_color);
                    ImGui::Text("► %s", gpu.name.c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::Text("%s", gpu.name.c_str());
                }
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d MB", gpu.memory_total);
                
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", gpu.driver_version.c_str());
                
                ImGui::TableSetColumnIndex(3);
                if (gpu.is_nvidia) {
                    ImGui::PushStyleColor(ImGuiCol_Text, accent_color);
                    ImGui::Text("✓ Active");
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, warning_color);
                    ImGui::Text("⚠ Limited");
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndTable();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // System Requirements
        ImGui::Text("Requirements & Recommendations");
        ImGui::BulletText("NVIDIA GPU with driver version 450+ for full functionality");
        ImGui::BulletText("Administrator privileges for GPU tuning");
        ImGui::BulletText("Adequate cooling for overclocking");
        ImGui::BulletText("Power supply headroom for increased power limits");
        
        ImGui::Spacing();
        ImGui::Text("Safety Reminders");
        ImGui::PushStyleColor(ImGuiCol_Text, warning_color);
        ImGui::BulletText("Monitor temperatures continuously during tuning");
        ImGui::BulletText("Start with conservative adjustments");
        ImGui::BulletText("Stress test after any changes");
        ImGui::BulletText("Keep original BIOS backup if flashing");
        ImGui::PopStyleColor();
    }
    
    void run() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            // Handle keyboard shortcuts
            if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
                monitor.detectGPUs();
            }
            
            render();
        }
    }
};

// Entry point
int main() {
    try {
        GPUTuneApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}

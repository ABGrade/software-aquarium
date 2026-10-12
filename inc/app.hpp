#pragma once

#include <windows.h>
#include <vector>
#include <array>
#include <string>
#include "renderer.hpp"
#include "simulation.hpp"

class App
{
public:
    App();
    int run(HINSTANCE hInstance, int nCmdShow);
    bool exportPreview(const char* outputPath);

private:
    struct Control
    {
        enum Type { Button, Slider, Combo, ListItem } type;
        RECT rect{};
        bool enabled{true};
        int index{}; // for buttons/list
        std::string id;
        float* value{nullptr};
        float minV{}, maxV{};
        bool integer{false};
        std::vector<std::wstring> options;
    };

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void renderFrame();
    void update(float dt);
    void handleKey(WPARAM key);
    void resize(int w, int h);
    void drawFish(const FishState& fish, const Vec3& camPos, const Vec3& camDir, const Vec3& camUp, const Vec3& camRight);
    void drawWalls(const Vec3& camPos, const Vec3& camDir, const Vec3& camUp, const Vec3& camRight);
    void drawWater(const Vec3& camPos, const Vec3& camDir, const Vec3& camUp, const Vec3& camRight);
    void drawTransparentFaces(const std::vector<std::array<VSOut,3>>& faces);
    void drawSidebar(HDC dc);
    void rebuildPreview();
    void startSimulation();
    void stopSimulation();
    int viewportWidth() const { return std::max(1, renderer_.width() - sidebarWidth_); }
    void updateVelocityFromHeading(FishConfig& cfg);
    void processSidebarClick(int mx, int my);
    void processSidebarDrag(int mx);
    void applySlider(Control& c, float value);
    void startBenchmark();
    void abortBenchmark();
    void applyTestCase(size_t idx);
    void finishBenchmark();

    HWND hwnd_{};
    Renderer renderer_;
    Simulation sim_;
    BITMAPINFO bmi_{};

    // camera
    float yaw_{0.6f};
    float pitch_{0.2f};
    float camDist_{420.0f};
    float camMinDist_{0.0f};
    bool rotating_{false};
    POINT lastMouse_{};

    // UI state
    int selectedFish_{0};
    Vec3 waterColor_{0.05f, 0.18f, 0.25f};
    Vec3 glassColor_{0.3f, 0.5f, 0.7f};
    float glassAlpha_{0.35f};
    Vec3 bgColor_{1.0f, 1.0f, 1.0f};
    float waterLevel_{0.85f}; // fraction of aquarium height (0..1)
    bool running_{false};
    bool previewDirty_{true};
    std::vector<FishConfig> editConfigs_;
    std::vector<FishState> previewFishes_;
    int sidebarWidth_{340};
    float aquariumHalf_{140.0f};
    float segProxy_{0.0f};
    std::vector<Control> controls_;
    int activeSlider_{-1};
    float sliderMin_{}, sliderMax_{};
    int sidebarScroll_{0};
    int sidebarContent_{0};
    // benchmarking
    bool testing_{false};
    bool forceAbort_{false};
    struct TestCase { int fish; int segments; float yaw; float pitch; std::string group; };
    struct TestResult { int fish; int segments; float yaw; float pitch; double avgMs; std::string group; };
    std::vector<TestCase> testCases_;
    std::vector<TestResult> testResults_;
    size_t currentTest_{0};
    int warmupFrames_{30};
    int sampleFrames_{120};
    int frameCounter_{0};
    double timeAccum_{0.0};
    std::vector<FishConfig> savedConfigs_;
    float savedYaw_{0}, savedPitch_{0}, savedCamDist_{0};
    bool savedRunning_{false};

    // timing
    double frameMs_{0.0};
    long long perfFreq_{};
    long long lastTick_{};
};

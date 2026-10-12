#include "app.hpp"
#include <sstream>
#include <array>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <windowsx.h>

namespace
{
    FishConfig makeDefaultFish(int idx)
    {
        FishConfig c;
        c.scale = 0.38f;
        c.segments = 28;
        c.type = static_cast<FishType>(idx % 4);
        c.color = { 0.65f + 0.05f * idx, 0.55f - 0.05f * idx, 0.6f };
        c.position = { -60.0f + idx * 40.0f, -10.0f + idx * 6.0f, -25.0f + idx * 12.0f };
        c.headingDeg = 25.0f * idx;
        c.pitchDeg = 0.0f;
        c.speed = 28.0f;
        c.velocity = {0,0,0};
        return c;
    }
}

bool App::exportPreview(const char* outputPath)
{
    sidebarWidth_ = 0;
    renderer_.resize(960, 640);
    renderFrame();
    const auto& pixels = renderer_.resolve();
    BITMAPFILEHEADER fileHeader{};
    BITMAPINFOHEADER imageHeader{};
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(imageHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + static_cast<DWORD>(pixels.size()*4);
    imageHeader.biSize = sizeof(imageHeader);
    imageHeader.biWidth = renderer_.width();
    imageHeader.biHeight = -renderer_.height();
    imageHeader.biPlanes = 1;
    imageHeader.biBitCount = 32;
    imageHeader.biCompression = BI_RGB;
    std::ofstream output(outputPath, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    output.write(reinterpret_cast<const char*>(&imageHeader), sizeof(imageHeader));
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()*4));
    return bool(output);
}

App::App() : renderer_(1200, 800, 2)
{
    sim_.setAquariumHalf(aquariumHalf_);
    camMinDist_ = aquariumHalf_ * 1.2f + 30.0f;
    camDist_ = aquariumHalf_ * 2.6f;
    editConfigs_.push_back(makeDefaultFish(0));
    editConfigs_.push_back(makeDefaultFish(1));
    for (auto& c : editConfigs_) updateVelocityFromHeading(c);
    rebuildPreview();
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&perfFreq_));
}

int App::run(HINSTANCE hInstance, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"FishSoftRenderer";
    WNDCLASS wc{};
    wc.lpfnWndProc = App::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    hwnd_ = CreateWindowEx(
        0, CLASS_NAME, L"Fish Aquarium (Software Renderer)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1300, 850,
        nullptr, nullptr, hInstance, this);
    ShowWindow(hwnd_, nCmdShow);

    bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi_.bmiHeader.biPlanes = 1;
    bmi_.bmiHeader.biBitCount = 32;
    bmi_.bmiHeader.biCompression = BI_RGB;
    bmi_.bmiHeader.biWidth = renderer_.width();
    bmi_.bmiHeader.biHeight = -renderer_.height();

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    lastTick_ = now.QuadPart;

    MSG msg;
    bool loop = true;
    while (loop)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                loop = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        LARGE_INTEGER cur;
        QueryPerformanceCounter(&cur);
        double dt = double(cur.QuadPart - lastTick_) / double(perfFreq_);
        lastTick_ = cur.QuadPart;
        frameMs_ = dt * 1000.0;
        dt = std::min(0.05, dt);

        update(static_cast<float>(dt));
        renderFrame();
    }

    return 0;
}

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    App* app = nullptr;
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        app = reinterpret_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else
    {
        app = reinterpret_cast<App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (app)
    {
        return app->handleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT App::handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        resize(w, h);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        if (testing_)
        {
            processSidebarClick(mx, my);
            return 0;
        }
        int viewW = viewportWidth();
        if (mx < viewW)
        {
            rotating_ = true;
            lastMouse_.x = mx;
            lastMouse_.y = my;
            SetCapture(hWnd);
        }
        else
        {
            processSidebarClick(mx, my);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        rotating_ = false;
        activeSlider_ = -1;
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE:
        if (testing_) return 0;
        if (activeSlider_ >= 0)
        {
            processSidebarDrag(GET_X_LPARAM(lParam));
        }
        if (rotating_)
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int dx = x - lastMouse_.x;
            int dy = y - lastMouse_.y;
            yaw_ += dx * 0.005f;
            pitch_ += dy * 0.005f;
            pitch_ = std::max(-1.2f, std::min(1.2f, pitch_));
            lastMouse_.x = x; lastMouse_.y = y;
        }
        return 0;
    case WM_MOUSEWHEEL:
    {
        if (testing_) return 0;
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int mx = GET_X_LPARAM(lParam);
        if (mx >= viewportWidth())
        {
            sidebarScroll_ -= (delta / WHEEL_DELTA) * 30;
            if (sidebarScroll_ < 0) sidebarScroll_ = 0;
            int maxScroll = std::max(0, sidebarContent_ - renderer_.height() + 20);
            if (sidebarScroll_ > maxScroll) sidebarScroll_ = maxScroll;
        }
        else
        {
            camDist_ -= delta * 0.08f;
            camDist_ = std::max(camMinDist_, std::min(700.0f, camDist_));
        }
        return 0;
    }
    case WM_CHAR:
        handleKey(wParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void App::resize(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    renderer_.resize(w, h);
    bmi_.bmiHeader.biWidth = renderer_.width();
    bmi_.bmiHeader.biHeight = -renderer_.height();
}

void App::startSimulation()
{
    for (auto& c : editConfigs_) updateVelocityFromHeading(c);
    running_ = true;
    sim_.setAquariumHalf(aquariumHalf_);
    sim_.setFromConfigs(editConfigs_);
    if (selectedFish_ >= static_cast<int>(sim_.fishes().size()))
        selectedFish_ = static_cast<int>(sim_.fishes().size()) - 1;
}

void App::stopSimulation()
{
    if (!running_) return;
    running_ = false;
    // copy current state back to editable configs so user can tweak after stop
    editConfigs_.clear();
    for (const auto& f : sim_.fishes())
    {
        editConfigs_.push_back(f.cfg);
    }
    if (selectedFish_ >= static_cast<int>(editConfigs_.size()))
        selectedFish_ = static_cast<int>(editConfigs_.size()) - 1;
    if (selectedFish_ < 0) selectedFish_ = 0;
    previewDirty_ = true;
}

void App::updateVelocityFromHeading(FishConfig& cfg)
{
    float yawRad = cfg.headingDeg * 3.14159265f / 180.0f;
    float pitchRad = cfg.pitchDeg * 3.14159265f / 180.0f;
    float cp = std::cos(pitchRad);
    Vec3 dir{ std::cos(yawRad) * cp, std::sin(pitchRad), std::sin(yawRad) * cp };
    dir = normalize(dir);
    cfg.velocity = dir * cfg.speed;
}

void App::rebuildPreview()
{
    if (selectedFish_ >= static_cast<int>(editConfigs_.size()))
        selectedFish_ = static_cast<int>(editConfigs_.size()) - 1;
    if (selectedFish_ < 0) selectedFish_ = 0;
    previewFishes_.clear();
    previewFishes_.reserve(editConfigs_.size());
    for (const auto& cfg : editConfigs_)
    {
        FishState fs;
        fs.cfg = cfg;
        fs.mesh = generateLatheFish(cfg.type, cfg.segments, cfg.scale);
        fs.radius = fs.mesh.boundingRadius;
        fs.mass = fs.radius * fs.radius * fs.radius;
        previewFishes_.push_back(fs);
    }
    previewDirty_ = false;
}

void App::update(float dt)
{
    if (running_)
    {
        float h = aquariumHalf_;
        float waterTop = -h + (2.0f * h * waterLevel_);
        sim_.setWaterTop(waterTop);
        sim_.update(dt);
    }
}

void App::drawFish(const FishState& fish, const Vec3& camPos, const Vec3& camDir, const Vec3& camUp, const Vec3& camRight)
{
    Vec3 worldForward = fish.cfg.velocity;
    if (length(worldForward) < 1e-3f) worldForward = { 1,0,0 };
    worldForward = normalize(worldForward);
    Vec3 upRef{0,1,0};
    Vec3 worldRight = cross(worldForward, upRef);
    if (length(worldRight) < 1e-3f) worldRight = {0,0,1};
    worldRight = normalize(worldRight);
    Vec3 worldUp = cross(worldRight, worldForward);

    for (const auto& tri : fish.mesh.tris)
    {
        VSOut out[3];
        bool ok = true;
        int idxs[3] = { tri.a, tri.b, tri.c };
        for (int i = 0; i < 3; ++i)
        {
            const Vertex& v = fish.mesh.vertices[idxs[i]];
            Vec3 local = v.pos;
            // Nose at x=0, body to positive X => forward is -X
            Vec3 worldPos = fish.cfg.position +
                (-worldForward) * local.x +
                worldUp * local.y +
                worldRight * local.z;
            Vec3 rel = worldPos - camPos;
            Vec3 viewPos{ dot(rel, camRight), dot(rel, camUp), dot(rel, camDir) };
            Vec3 worldNormal = normalize(
                (-worldForward) * v.normal.x +
                worldUp * v.normal.y +
                worldRight * v.normal.z);
            if (!renderer_.projectToScreen(viewPos, worldNormal, fish.cfg.color, v.alpha, out[i]))
            {
                ok = false; break;
            }
        }
        if (ok) renderer_.drawTriangle(out[0], out[1], out[2], true, true);
    }
}

void App::drawTransparentFaces(const std::vector<std::array<VSOut,3>>& faces)
{
    auto sorted = faces;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b)
    {
        float za = (a[0].z + a[1].z + a[2].z) / 3.0f;
        float zb = (b[0].z + b[1].z + b[2].z) / 3.0f;
        return za > zb;
    });
    for (auto& f : sorted)
    {
        renderer_.drawTriangle(f[0], f[1], f[2], false, true);
    }
}

void App::drawWalls(const Vec3& camPos, const Vec3& camDir, const Vec3& camUp, const Vec3& camRight)
{
    float h = aquariumHalf_;
    struct Face { Vec3 p0, p1, p2, p3; };
    std::array<Face,6> faces{
        Face{ { -h, -h, h }, { h, -h, h }, { h, h, h }, { -h, h, h } },
        Face{ { -h, -h,-h }, { -h, h,-h }, { h, h,-h }, { h,-h,-h } },
        Face{ { h,-h,-h }, { h,h,-h }, { h,h,h }, { h,-h,h } },
        Face{ { -h,-h,-h }, { -h,-h,h }, { -h,h,h }, { -h,h,-h } },
        Face{ { -h,h,-h }, { -h,h,h }, { h,h,h }, { h,h,-h } },
        Face{ { -h,-h,-h }, { h,-h,-h }, { h,-h,h }, { -h,-h,h } },
    };

    std::vector<std::array<VSOut,3>> triFaces;
    triFaces.reserve(12);
    Vec3 faceColor{ glassColor_.x, glassColor_.y, glassColor_.z };
    float alpha = glassAlpha_;
    std::array<Vec3,6> normals{
        Vec3{0,0,1}, Vec3{0,0,-1}, Vec3{1,0,0},
        Vec3{-1,0,0}, Vec3{0,1,0}, Vec3{0,-1,0}
    };
    for (size_t fi = 0; fi < faces.size(); ++fi)
    {
        auto& f = faces[fi];
        Vec3 verts[4] = { f.p0, f.p1, f.p2, f.p3 };
        int idxs[6] = {0,1,2, 0,2,3};
        Vec3 normal = normals[fi];
        for (int t = 0; t < 2; ++t)
        {
            std::array<VSOut,3> out{};
            bool ok = true;
            for (int i = 0; i < 3; ++i)
            {
                Vec3 worldPos = verts[idxs[t*3 + i]];
                Vec3 rel = worldPos - camPos;
                Vec3 viewPos{ dot(rel, camRight), dot(rel, camUp), dot(rel, camDir) };
                if (!renderer_.projectToScreen(viewPos, normal, faceColor, alpha, out[i]))
                {
                    ok = false; break;
                }
            }
            if (ok) triFaces.push_back(out);
        }
    }
    drawTransparentFaces(triFaces);
}

void App::drawWater(const Vec3& camPos, const Vec3& camDir, const Vec3& camUp, const Vec3& camRight)
{
    float h = aquariumHalf_;
    float waterTop = -h + (2.0f * h * waterLevel_);
    // render water volume with low alpha to keep tint visible through glass
    struct Face { Vec3 p0, p1, p2, p3; };
    std::array<Face,6> faces{
        Face{ { -h, -h, h }, { h, -h, h }, { h, waterTop, h }, { -h, waterTop, h } },
        Face{ { -h, -h,-h }, { -h, waterTop,-h }, { h, waterTop,-h }, { h,-h,-h } },
        Face{ { h,-h,-h }, { h, waterTop,-h }, { h, waterTop,h }, { h,-h,h } },
        Face{ { -h,-h,-h }, { -h,-h,h }, { -h,waterTop,h }, { -h,waterTop,-h } },
        Face{ { -h,waterTop,-h }, { -h,waterTop,h }, { h,waterTop,h }, { h,waterTop,-h } },
        Face{ { -h,-h,-h }, { h,-h,-h }, { h,-h,h }, { -h,-h,h } },
    };
    std::array<Vec3,6> normals{
        Vec3{0,0,1}, Vec3{0,0,-1}, Vec3{1,0,0},
        Vec3{-1,0,0}, Vec3{0,1,0}, Vec3{0,-1,0}
    };
    Vec3 faceColor{ waterColor_.x, waterColor_.y, waterColor_.z };
    float alpha = 0.08f;
    std::vector<std::array<VSOut,3>> triFaces;
    triFaces.reserve(12);
    for (size_t fi = 0; fi < faces.size(); ++fi)
    {
        auto& f = faces[fi];
        Vec3 verts[4] = { f.p0, f.p1, f.p2, f.p3 };
        int idxs[6] = {0,1,2, 0,2,3};
        Vec3 normal = normals[fi];
        for (int t = 0; t < 2; ++t)
        {
            std::array<VSOut,3> out{};
            bool ok = true;
            for (int i = 0; i < 3; ++i)
            {
                Vec3 worldPos = verts[idxs[t*3 + i]];
                Vec3 rel = worldPos - camPos;
                Vec3 viewPos{ dot(rel, camRight), dot(rel, camUp), dot(rel, camDir) };
                if (!renderer_.projectToScreen(viewPos, normal, faceColor, alpha, out[i])) { ok = false; break; }
            }
            if (ok) triFaces.push_back(out);
        }
    }
    drawTransparentFaces(triFaces);
}

void App::drawSidebar(HDC dc)
{
    int viewW = viewportWidth();
    RECT panel{ viewW, 0, renderer_.width(), renderer_.height() };
    HBRUSH bg = CreateSolidBrush(RGB(25, 30, 40));
    FillRect(dc, &panel, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    controls_.clear();

    auto drawTextLine = [&](int x, int y, const std::wstring& s, COLORREF color = RGB(230,230,230))
    {
        SetTextColor(dc, color);
        TextOut(dc, x, y, s.c_str(), static_cast<int>(s.size()));
    };

    auto addButton = [&](RECT r, std::wstring label, bool enabled, const std::string& id, COLORREF fill = RGB(70, 120, 160))
    {
        HBRUSH brush = CreateSolidBrush(enabled ? fill : RGB(70,70,70));
        FillRect(dc, &r, brush); DeleteObject(brush);
        drawTextLine(r.left + 6, r.top + 6, label);
        Control c; c.type = Control::Button; c.rect = r; c.enabled = enabled; c.id = id;
        controls_.push_back(c);
        return (int)controls_.size() - 1;
    };

    auto addSlider = [&](RECT r, float* value, float minV, float maxV, bool enabled, bool integer = false, const std::string& id = "")
    {
        HBRUSH track = CreateSolidBrush(RGB(60,60,70));
        FillRect(dc, &r, track); DeleteObject(track);
        float t = (*value - minV) / (maxV - minV);
        t = std::max(0.0f, std::min(1.0f, t));
        int handleX = r.left + 4 + int(t * (r.right - r.left - 8));
        RECT h{ handleX - 4, r.top + 2, handleX + 4, r.bottom - 2 };
        HBRUSH hb = CreateSolidBrush(enabled ? RGB(140, 200, 120) : RGB(90,90,90));
        FillRect(dc, &h, hb); DeleteObject(hb);
        Control c; c.type = Control::Slider; c.rect = r; c.value = value; c.minV = minV; c.maxV = maxV; c.enabled = enabled; c.integer = integer; c.id = id;
        controls_.push_back(c);
        return (int)controls_.size() - 1;
    };

    auto addCombo = [&](RECT r, int current, const std::vector<std::wstring>& opts, bool enabled, const std::string& id)
    {
        HBRUSH brush = CreateSolidBrush(enabled ? RGB(80,80,110) : RGB(60,60,60));
        FillRect(dc, &r, brush); DeleteObject(brush);
        if (current >= 0 && current < (int)opts.size())
            drawTextLine(r.left + 6, r.top + 6, opts[current]);
        Control c; c.type = Control::Combo; c.rect = r; c.index = current; c.options = opts; c.enabled = enabled; c.id = id;
        controls_.push_back(c);
        return (int)controls_.size() - 1;
    };

    auto addListItem = [&](RECT r, int idx, const std::wstring& label, bool selected, bool enabled)
    {
        HBRUSH brush = CreateSolidBrush(selected ? RGB(120, 180, 200) : RGB(50,50,70));
        FillRect(dc, &r, brush); DeleteObject(brush);
        drawTextLine(r.left + 6, r.top + 4, label, enabled ? RGB(240,240,240) : RGB(130,130,130));
        Control c; c.type = Control::ListItem; c.rect = r; c.enabled = enabled; c.index = idx; c.id = "fish_item";
        controls_.push_back(c);
        return (int)controls_.size() - 1;
    };

    bool uiEnabled = !testing_;
    bool editEnabled = uiEnabled && !running_;

    int x0 = viewW + 12;
    int y = 10 - sidebarScroll_;
    drawTextLine(x0, y, L"Меню", RGB(150, 210, 255)); y += 24;
    drawTextLine(x0, y, L"Старт/стоп: кнопки ниже или Enter/Space"); y += 30;

    RECT startBtn{ x0, y, x0 + sidebarWidth_ - 24, y + 28 };
    RECT stopBtn{ x0, y + 35, x0 + sidebarWidth_ - 24, y + 63 };
    addButton(startBtn, L"Старт симуляции", uiEnabled, "start");
    addButton(stopBtn, L"Стоп / редактирование", uiEnabled, "stop");
    RECT benchBtn{ x0, y + 70, x0 + sidebarWidth_ - 24, y + 98 };
    if (!testing_)
        addButton(benchBtn, L"Начать тестирование", true, "benchmark");
    else
        addButton(benchBtn, L"Принудительно завершить", true, "benchmark_abort", RGB(180, 70, 70));
    y += 110;
    if (testing_)
    {
        std::wstringstream bench;
        bench << L"Тест " << (currentTest_ + 1) << L" из " << testCases_.size();
        drawTextLine(x0, y, bench.str(), RGB(230, 190, 120));
        y += 22;
        drawTextLine(x0, y, L"Ввод заблокирован до конца теста.");
        y += 24;
    }

    drawTextLine(x0, y, L"Общие настройки", RGB(180, 210, 255)); y += 26;
    wchar_t buf[160];
    swprintf(buf, 160, L"Цвет воды (0 - 1)");
    drawTextLine(x0, y, buf); y += 20;
    RECT wr{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(wr, &waterColor_.x, 0.0f, 1.0f, uiEnabled, false, "waterR"); drawTextLine(wr.right - 40, wr.top - 16, L"1.0"); drawTextLine(wr.left, wr.top - 16, L"0.0"); y += 50;
    RECT wg{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(wg, &waterColor_.y, 0.0f, 1.0f, uiEnabled, false, "waterG"); drawTextLine(wg.right - 40, wg.top - 16, L"1.0"); drawTextLine(wg.left, wg.top - 16, L"0.0"); y += 50;
    RECT wb{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(wb, &waterColor_.z, 0.0f, 1.0f, uiEnabled, false, "waterB"); drawTextLine(wb.right - 40, wb.top - 16, L"1.0"); drawTextLine(wb.left, wb.top - 16, L"0.0"); y += 50;
    swprintf(buf, 160, L"Цвет стекла (0 - 1)");
    drawTextLine(x0, y, buf); y += 20;
    RECT gr{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(gr, &glassColor_.x, 0.0f, 1.0f, uiEnabled, false, "glassR"); drawTextLine(gr.right - 40, gr.top - 16, L"1.0"); drawTextLine(gr.left, gr.top - 16, L"0.0"); y += 50;
    RECT gg{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(gg, &glassColor_.y, 0.0f, 1.0f, uiEnabled, false, "glassG"); drawTextLine(gg.right - 40, gg.top - 16, L"1.0"); drawTextLine(gg.left, gg.top - 16, L"0.0"); y += 50;
    RECT gb{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(gb, &glassColor_.z, 0.0f, 1.0f, uiEnabled, false, "glassB"); drawTextLine(gb.right - 40, gb.top - 16, L"1.0"); drawTextLine(gb.left, gb.top - 16, L"0.0"); y += 50;
    swprintf(buf, 160, L"Прозрачность стекла (0.05 - 0.8)");
    drawTextLine(x0, y, buf); y += 20;
    RECT ga{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(ga, &glassAlpha_, 0.05f, 0.8f, uiEnabled, false, "glassA"); drawTextLine(ga.right - 60, ga.top - 16, L"0.8"); drawTextLine(ga.left, ga.top - 16, L"0.05"); y += 50;
    swprintf(buf, 160, L"Уровень воды (0.5 - 0.95 от высоты)");
    drawTextLine(x0, y, buf); y += 20;
    RECT wl{ x0, y + 15, x0 + sidebarWidth_ - 24, y + 33 }; addSlider(wl, &waterLevel_, 0.5f, 0.95f, uiEnabled, false, "waterLevel"); drawTextLine(wl.right - 60, wl.top - 16, L"0.95"); drawTextLine(wl.left, wl.top - 16, L"0.50"); y += 50;
    swprintf(buf, 160, L"Mode: %s", running_ ? L"SIMULATION" : L"SETUP");
    drawTextLine(x0, y, buf, running_ ? RGB(200,120,120) : RGB(120,200,120)); y += 28;

    drawTextLine(x0, y, L"Рыбы", RGB(180, 210, 255)); y += 22;
    RECT addBtn{ x0, y, x0 + (sidebarWidth_-28)/2, y + 26 };
    RECT delBtn{ addBtn.right + 6, y, x0 + sidebarWidth_-24, y + 26 };
    addButton(addBtn, L"+ Добавить", editEnabled && (int)editConfigs_.size() < 5, "add");
    addButton(delBtn, L"- Удалить", editEnabled && !editConfigs_.empty(), "del");
    y += 34;

    int listH = 24;
    if (running_)
    {
        for (int i = 0; i < (int)sim_.fishes().size(); ++i)
        {
            std::wstring label = L"Рыба " + std::to_wstring(i+1);
            RECT r{ x0, y, x0 + sidebarWidth_-24, y + listH };
            addListItem(r, i, label, i == selectedFish_, false);
            y += listH + 2;
        }
    }
    else
    {
        for (int i = 0; i < (int)editConfigs_.size(); ++i)
        {
            std::wstring label = L"Рыба " + std::to_wstring(i+1);
            RECT r{ x0, y, x0 + sidebarWidth_-24, y + listH };
            addListItem(r, i, label, i == selectedFish_, editEnabled);
            y += listH + 2;
        }
    }

    if ((!running_ && editConfigs_.empty()) || (running_ && sim_.fishes().empty()))
    {
        drawTextLine(x0, y, L"Нет рыб для настройки.");
        return;
    }

    const auto& cfg = running_ ? sim_.fishes()[selectedFish_].cfg : editConfigs_[selectedFish_];
    drawTextLine(x0, y, L"Параметры выбранной", RGB(180, 210, 255)); y += 22;
    std::vector<std::wstring> typeNames{L"Guppy", L"Discus", L"FanTail", L"Barbus"};
    RECT comboR{ x0, y, x0 + sidebarWidth_-24, y + 20 };
    addCombo(comboR, (int)cfg.type, typeNames, editEnabled, "type"); y += 26;
    drawTextLine(x0, y, L"Масштаб (0.3 - 2.0)"); y += 18;
    RECT scR{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(scR, const_cast<float*>(&cfg.scale), 0.3f, 2.0f, editEnabled, false, "scale"); drawTextLine(scR.right - 50, scR.top - 16, L"2.0"); drawTextLine(scR.left, scR.top - 16, L"0.3"); y += 50;
    drawTextLine(x0, y, L"Сегменты (8 - 96)"); y += 18;
    segProxy_ = (float)cfg.segments;
    RECT segR{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(segR, &segProxy_, 8.0f, 96.0f, editEnabled, true, "segments"); drawTextLine(segR.right - 40, segR.top - 16, L"96"); drawTextLine(segR.left, segR.top - 16, L"8"); y += 50;
    drawTextLine(x0, y, L"Угол по горизонтали (0-360)"); y += 18;
    RECT hdR{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(hdR, const_cast<float*>(&cfg.headingDeg), 0.0f, 360.0f, editEnabled, false, "heading"); drawTextLine(hdR.right - 50, hdR.top - 16, L"360"); drawTextLine(hdR.left, hdR.top - 16, L"0"); y += 50;
    drawTextLine(x0, y, L"Угол по вертикали (-80..80)"); y += 18;
    RECT pdR{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(pdR, const_cast<float*>(&cfg.pitchDeg), -80.0f, 80.0f, editEnabled, false, "pitch"); drawTextLine(pdR.right - 50, pdR.top - 16, L"+80"); drawTextLine(pdR.left, pdR.top - 16, L"-80"); y += 50;
    drawTextLine(x0, y, L"Скорость (2 - 60 ед/с)"); y += 18;
    RECT spR{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(spR, const_cast<float*>(&cfg.speed), 2.0f, 60.0f, editEnabled, false, "speed"); drawTextLine(spR.right - 40, spR.top - 16, L"60"); drawTextLine(spR.left, spR.top - 16, L"2"); y += 50;
    drawTextLine(x0, y, L"Цвет"); y += 18;
    RECT cr{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(cr, const_cast<float*>(&cfg.color.x), 0.0f, 1.0f, editEnabled, false, "colr"); drawTextLine(cr.right - 40, cr.top - 16, L"1.0"); drawTextLine(cr.left, cr.top - 16, L"0.0"); y += 50;
    RECT cg{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(cg, const_cast<float*>(&cfg.color.y), 0.0f, 1.0f, editEnabled, false, "colg"); drawTextLine(cg.right - 40, cg.top - 16, L"1.0"); drawTextLine(cg.left, cg.top - 16, L"0.0"); y += 50;
    RECT cb{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(cb, const_cast<float*>(&cfg.color.z), 0.0f, 1.0f, editEnabled, false, "colb"); drawTextLine(cb.right - 40, cb.top - 16, L"1.0"); drawTextLine(cb.left, cb.top - 16, L"0.0"); y += 50;
    drawTextLine(x0, y, L"Позиция X/Y/Z"); y += 18;
    wchar_t boundBuf[32];
    RECT px{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(px, const_cast<float*>(&cfg.position.x), -aquariumHalf_+10, aquariumHalf_-10, editEnabled, false, "posx"); swprintf(boundBuf, 32, L"+%d", (int)aquariumHalf_ - 10); drawTextLine(px.right - 50, px.top - 16, boundBuf); swprintf(boundBuf, 32, L"-%d", (int)aquariumHalf_ - 10); drawTextLine(px.left, px.top - 16, boundBuf); y += 50;
    RECT py{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(py, const_cast<float*>(&cfg.position.y), -aquariumHalf_+10, aquariumHalf_-10, editEnabled, false, "posy"); swprintf(boundBuf, 32, L"+%d", (int)aquariumHalf_ - 10); drawTextLine(py.right - 50, py.top - 16, boundBuf); swprintf(boundBuf, 32, L"-%d", (int)aquariumHalf_ - 10); drawTextLine(py.left, py.top - 16, boundBuf); y += 50;
    RECT pz{ x0, y + 15, x0 + sidebarWidth_-24, y + 33 }; addSlider(pz, const_cast<float*>(&cfg.position.z), -aquariumHalf_+10, aquariumHalf_-10, editEnabled, false, "posz"); swprintf(boundBuf, 32, L"+%d", (int)aquariumHalf_ - 10); drawTextLine(pz.right - 50, pz.top - 16, boundBuf); swprintf(boundBuf, 32, L"-%d", (int)aquariumHalf_ - 10); drawTextLine(pz.left, pz.top - 16, boundBuf); y += 50;
    drawTextLine(x0, y, running_ ? L"Редактирование заблокировано." : (uiEnabled ? L"Тяните ползунки для настройки." : L"Тестирование: ввод заблокирован."));
    sidebarContent_ = y - (10 - sidebarScroll_);
}

void App::renderFrame()
{
    renderer_.setViewportWidth(viewportWidth());
    if (previewDirty_ && !running_) rebuildPreview();
    renderer_.clear(bgColor_);

    Vec3 target{0,0,0};
    camDist_ = std::max(camMinDist_, camDist_);
    float cx = camDist_ * std::cos(pitch_) * std::cos(yaw_);
    float cy = camDist_ * std::sin(pitch_);
    float cz = camDist_ * std::cos(pitch_) * std::sin(yaw_);
    Vec3 camPos{ target.x + cx, target.y + cy, target.z + cz };
    if (length(camPos - target) < camMinDist_)
    {
        camPos = normalize(camPos - target) * camMinDist_;
    }
    Vec3 camDir = normalize(target - camPos);
    Vec3 upRef{0,1,0};
    Vec3 camRight = cross(camDir, upRef);
    if (length(camRight) < 1e-4f) camRight = {1,0,0};
    camRight = normalize(camRight);
    Vec3 camUp = cross(camRight, camDir);

    const auto& drawList = running_ ? sim_.fishes() : previewFishes_;
    // simple frustum cull by bounding sphere to save work
    float halfFov = 60.0f * 3.14159265f / 180.0f * 0.5f;
    float tanHalf = std::tan(halfFov);
    float aspect = static_cast<float>(viewportWidth()) / static_cast<float>(renderer_.height());
    for (auto& f : drawList)
    {
        Vec3 rel = f.cfg.position - camPos;
        float vx = dot(rel, camRight);
        float vy = dot(rel, camUp);
        float vz = dot(rel, camDir);
        float radius = f.radius;
        if (vz + radius < renderer_.nearPlane) continue;
        float maxX = (vz + radius) * tanHalf * aspect + radius;
        float maxY = (vz + radius) * tanHalf + radius;
        if (std::fabs(vx) > maxX || std::fabs(vy) > maxY) continue;
        drawFish(f, camPos, camDir, camUp, camRight);
    }

    drawWater(camPos, camDir, camUp, camRight);
    drawWalls(camPos, camDir, camUp, camRight);
    // water surface
    float waterY = (waterLevel_ * 2.0f - 1.0f) * aquariumHalf_;
    {
        Vec3 p0{ -aquariumHalf_, waterY, -aquariumHalf_ };
        Vec3 p1{ aquariumHalf_, waterY, -aquariumHalf_ };
        Vec3 p2{ aquariumHalf_, waterY, aquariumHalf_ };
        Vec3 p3{ -aquariumHalf_, waterY, aquariumHalf_ };
        std::vector<std::array<VSOut,3>> faces;
        faces.reserve(2);
        Vec3 normal{0,1,0};
        float alpha = 0.35f;
        Vec3 color = waterColor_;
        Vec3 verts[4] = { p0, p1, p2, p3 };
        int idxs[6] = {0,1,2, 0,2,3};
        for (int t = 0; t < 2; ++t)
        {
            std::array<VSOut,3> out{};
            bool ok = true;
            for (int i = 0; i < 3; ++i)
            {
                Vec3 worldPos = verts[idxs[t*3 + i]];
                Vec3 rel = worldPos - camPos;
                Vec3 viewPos{ dot(rel, camRight), dot(rel, camUp), dot(rel, camDir) };
                if (!renderer_.projectToScreen(viewPos, normal, color, alpha, out[i])) { ok = false; break; }
            }
            if (ok) faces.push_back(out);
        }
        drawTransparentFaces(faces);
    }

    const auto& pixels = renderer_.resolve();
    if (!hwnd_) return; // Offscreen preview exports do not draw onto the desktop.
    HDC dc = GetDC(hwnd_);
    StretchDIBits(dc, 0, 0, renderer_.width(), renderer_.height(),
                  0, 0, renderer_.width(), renderer_.height(),
                  pixels.data(), &bmi_, DIB_RGB_COLORS, SRCCOPY);

    drawSidebar(dc);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0,0,0));
    std::wstringstream ss;
    double fps = frameMs_ > 0.0001 ? 1000.0 / frameMs_ : 0.0;
    ss << L"FPS: " << (int)fps << L" | Рыб: " << drawList.size()
       << L" | Камера: " << camDist_;
    std::wstring line = ss.str();
    TextOut(dc, 10, 10, line.c_str(), static_cast<int>(line.size()));

    if (testing_)
    {
        if (forceAbort_)
        {
            finishBenchmark();
        }
        else if (!testCases_.empty())
        {
            ++frameCounter_;
            if (frameCounter_ > warmupFrames_)
            {
                timeAccum_ += frameMs_;
            }
            int totalFrames = warmupFrames_ + sampleFrames_;
            if (frameCounter_ >= totalFrames)
            {
                const auto& tc = testCases_[currentTest_];
                int samples = std::max(1, frameCounter_ - warmupFrames_);
                double avg = timeAccum_ / static_cast<double>(samples);
                testResults_.push_back({ tc.fish, tc.segments, tc.yaw, tc.pitch, avg, tc.group });
                ++currentTest_;
                if (currentTest_ >= testCases_.size())
                {
                    finishBenchmark();
                }
                else
                {
                    applyTestCase(currentTest_);
                }
            }
        }
    }

    ReleaseDC(hwnd_, dc);
}

void App::applySlider(Control& c, float value)
{
    if (!c.enabled) return;
    value = std::max(c.minV, std::min(c.maxV, value));
    if (c.integer) value = std::round(value);
    if (c.value) *c.value = value;

    if (c.id == "waterR") waterColor_.x = value;
    else if (c.id == "waterG") waterColor_.y = value;
    else if (c.id == "waterB") waterColor_.z = value;
    else if (c.id == "waterLevel") waterLevel_ = value;

    if (running_) return;
    if (editConfigs_.empty()) return;
    FishConfig& cfg = editConfigs_[selectedFish_];
    if (c.id == "scale") { cfg.scale = value; previewDirty_ = true; }
    else if (c.id == "segments") { cfg.segments = (int)value; previewDirty_ = true; }
    else if (c.id == "heading") { cfg.headingDeg = value; updateVelocityFromHeading(cfg); previewDirty_ = true; }
    else if (c.id == "pitch") { cfg.pitchDeg = value; updateVelocityFromHeading(cfg); previewDirty_ = true; }
    else if (c.id == "speed") { cfg.speed = value; updateVelocityFromHeading(cfg); previewDirty_ = true; }
    else if (c.id == "colr") { cfg.color.x = value; previewDirty_ = true; }
    else if (c.id == "colg") { cfg.color.y = value; previewDirty_ = true; }
    else if (c.id == "colb") { cfg.color.z = value; previewDirty_ = true; }
    else if (c.id == "posx") { cfg.position.x = value; previewDirty_ = true; }
    else if (c.id == "posy") { cfg.position.y = value; previewDirty_ = true; }
    else if (c.id == "posz") { cfg.position.z = value; previewDirty_ = true; }
}

void App::processSidebarClick(int mx, int my)
{
    POINT p{ mx, my };
    for (int i = (int)controls_.size() - 1; i >= 0; --i)
    {
        auto& c = controls_[i];
        if (!PtInRect(&c.rect, p)) continue;
        if (!c.enabled) return;
        if (c.type == Control::Button)
        {
            if (c.id == "start") startSimulation();
            else if (c.id == "stop") stopSimulation();
            else if (c.id == "benchmark") startBenchmark();
            else if (c.id == "benchmark_abort") abortBenchmark();
            else if (c.id == "add")
            {
                if (!running_ && (int)editConfigs_.size() < 5)
                {
                    editConfigs_.push_back(makeDefaultFish((int)editConfigs_.size()));
                    updateVelocityFromHeading(editConfigs_.back());
                    selectedFish_ = (int)editConfigs_.size() - 1;
                    previewDirty_ = true;
                }
            }
            else if (c.id == "del")
            {
                if (!running_ && !editConfigs_.empty())
                {
                    editConfigs_.erase(editConfigs_.begin() + selectedFish_);
                    if (selectedFish_ >= (int)editConfigs_.size()) selectedFish_ = (int)editConfigs_.size() - 1;
                    if (selectedFish_ < 0) selectedFish_ = 0;
                    previewDirty_ = true;
                }
            }
            return;
        }
        if (c.type == Control::ListItem)
        {
            if (!running_) { selectedFish_ = c.index; previewDirty_ = true; }
            else if (c.index < (int)sim_.fishes().size()) { selectedFish_ = c.index; }
            return;
        }
        if (c.type == Control::Combo)
        {
            if (!running_ && !editConfigs_.empty())
            {
                FishConfig& cfg = editConfigs_[selectedFish_];
                int next = ((int)cfg.type + 1) % 4;
                cfg.type = (FishType)next;
                previewDirty_ = true;
            }
            return;
        }
        if (c.type == Control::Slider)
        {
            activeSlider_ = i;
            processSidebarDrag(mx);
            return;
        }
    }
}

void App::processSidebarDrag(int mx)
{
    if (activeSlider_ < 0 || activeSlider_ >= (int)controls_.size()) return;
    auto& c = controls_[activeSlider_];
    int w = c.rect.right - c.rect.left;
    if (w <= 1) return;
    float t = float(mx - c.rect.left) / float(w);
    float value = c.minV + std::max(0.0f, std::min(1.0f, t)) * (c.maxV - c.minV);
    applySlider(c, value);
}

void App::handleKey(WPARAM key)
{
    if (testing_)
    {
        if (key == VK_ESCAPE) forceAbort_ = true;
        return;
    }
    if (key == VK_RETURN || key == VK_SPACE)
    {
        if (running_) stopSimulation();
        else startSimulation();
    }
}

void App::startBenchmark()
{
    if (testing_) return;
    savedConfigs_.clear();
    if (running_)
    {
        for (const auto& f : sim_.fishes()) savedConfigs_.push_back(f.cfg);
    }
    else
    {
        savedConfigs_ = editConfigs_;
    }
    savedYaw_ = yaw_;
    savedPitch_ = pitch_;
    savedCamDist_ = camDist_;
    savedRunning_ = running_;

    testing_ = true;
    forceAbort_ = false;
    testCases_.clear();
    testResults_.clear();
    currentTest_ = 0;
    frameCounter_ = 0;
    timeAccum_ = 0.0;

    // sweep by fish count (fixed mesh complexity)
    const float baseYaw = yaw_;
    const float basePitch = pitch_;
    const int baseSegments = 32;
    std::vector<int> fishCounts{1, 2, 4, 8, 12, 16};
    for (int f : fishCounts)
    {
        testCases_.push_back({ f, baseSegments, baseYaw, basePitch, "fish_sweep" });
    }

    // sweep by mesh complexity (fixed fish count)
    const int segmentFish = 8;
    std::vector<int> segmentCounts{8, 16, 24, 32, 48, 64, 80, 96};
    for (int seg : segmentCounts)
    {
        testCases_.push_back({ segmentFish, seg, baseYaw, basePitch, "segments_sweep" });
    }

    // optional camera angle impact with a mid-size scene
    const int camTestFish = 10;
    const int camTestSegs = 48;
    std::vector<std::pair<float,float>> cameraAngles{
        { baseYaw, basePitch },
        { baseYaw + 0.8f, basePitch },
        { baseYaw, basePitch + 0.4f },
    };
    for (auto [yaw, pitch] : cameraAngles)
    {
        testCases_.push_back({ camTestFish, camTestSegs, yaw, pitch, "camera_sweep" });
    }

    if (!testCases_.empty())
    {
        applyTestCase(0);
    }
    else
    {
        finishBenchmark();
    }
}

void App::abortBenchmark()
{
    if (!testing_) return;
    forceAbort_ = true;
}

void App::applyTestCase(size_t idx)
{
    if (idx >= testCases_.size())
    {
        finishBenchmark();
        return;
    }

    const auto& tc = testCases_[idx];
    running_ = false;
    editConfigs_.clear();

    float radius = aquariumHalf_ * 0.6f;
    for (int i = 0; i < tc.fish; ++i)
    {
        FishConfig cfg = makeDefaultFish(i);
        cfg.segments = tc.segments;
        float angle = tc.fish > 0 ? (6.2831853f * i) / std::max(1, tc.fish) : 0.0f;
        cfg.position = { std::cos(angle) * radius, -10.0f + (i % 3) * 8.0f, std::sin(angle) * radius };
        cfg.headingDeg = angle * 180.0f / 3.14159265f;
        cfg.pitchDeg = 0.0f;
        cfg.speed = 28.0f;
        updateVelocityFromHeading(cfg);
        editConfigs_.push_back(cfg);
    }

    yaw_ = tc.yaw;
    pitch_ = tc.pitch;
    camDist_ = savedCamDist_;
    selectedFish_ = 0;
    previewDirty_ = true;
    startSimulation();
    frameCounter_ = 0;
    timeAccum_ = 0.0;
}

void App::finishBenchmark()
{
    testing_ = false;
    running_ = false;
    forceAbort_ = false;
    currentTest_ = 0;
    frameCounter_ = 0;
    timeAccum_ = 0.0;

    editConfigs_ = savedConfigs_;
    yaw_ = savedYaw_;
    pitch_ = savedPitch_;
    camDist_ = savedCamDist_;
    selectedFish_ = 0;
    previewDirty_ = true;

    if (savedRunning_ && !editConfigs_.empty())
    {
        startSimulation();
    }

    if (!testResults_.empty())
    {
        std::filesystem::create_directories("test_results");
        auto toDeg = [](float rad) { return rad * 180.0f / 3.14159265f; };

        std::ofstream allFile("test_results/all_cases.csv");
        if (allFile)
        {
            allFile << "group,fish_count,segments,yaw_deg,pitch_deg,avg_ms,fps\n";
            allFile << std::fixed << std::setprecision(3);
            for (const auto& r : testResults_)
            {
                double fps = r.avgMs > 0.0 ? 1000.0 / r.avgMs : 0.0;
                allFile << r.group << "," << r.fish << "," << r.segments << ","
                        << toDeg(r.yaw) << "," << toDeg(r.pitch) << ","
                        << r.avgMs << "," << fps << "\n";
            }
        }

        auto writeGroup = [&](const std::string& group, const std::string& name)
        {
            std::ofstream f("test_results/" + name);
            if (!f) return;
            f << "fish_count,segments,yaw_deg,pitch_deg,avg_ms,fps\n";
            f << std::fixed << std::setprecision(3);
            for (const auto& r : testResults_)
            {
                if (r.group != group) continue;
                double fps = r.avgMs > 0.0 ? 1000.0 / r.avgMs : 0.0;
                f << r.fish << "," << r.segments << ","
                  << toDeg(r.yaw) << "," << toDeg(r.pitch) << ","
                  << r.avgMs << "," << fps << "\n";
            }
        };

        writeGroup("fish_sweep", "fish_vs_time.csv");
        writeGroup("segments_sweep", "segments_vs_time.csv");
        writeGroup("camera_sweep", "camera_vs_time.csv");
    }

    testCases_.clear();
    testResults_.clear();
}


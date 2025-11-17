// Tennis Ball Physics Simulator with Direct2D
// Simulates ball drops on 4 different court surfaces

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

// Constants
const float GRAVITY = 9.81f; // m/s^2
const float BALL_RADIUS = 0.0335f; // Tennis ball radius in meters (6.7cm diameter)
const float INITIAL_HEIGHT = 2.0f; // meters
const float PIXELS_PER_METER = 100.0f; // Scaling factor for visualization
const int WINDOW_WIDTH = 640;
const int WINDOW_HEIGHT = 480;
const int SECTION_WIDTH = WINDOW_WIDTH / 4;
const float DT = 0.016f; // ~60 FPS

// Court surface properties
enum CourtType {
    ROLAND_GARROS_CLAY,    // Clay court - slower, higher bounce
    WIMBLEDON_GRASS,       // Grass court - faster, lower bounce
    US_OPEN_HARD,          // Hard court - medium speed, consistent bounce
    LAVER_CUP_BLACK        // Special hard court - similar to hard court
};

struct CourtSurface {
    CourtType type;
    const wchar_t* name;
    float coefficientOfRestitution; // COR (bounce height ratio)
    float friction;
    D2D1_COLOR_F color;
    D2D1_COLOR_F ballColor;
};

// Define court surfaces with realistic physics properties
CourtSurface courts[4] = {
    {ROLAND_GARROS_CLAY, L"Roland Garros\n(Clay)", 0.75f, 0.6f, 
     D2D1::ColorF(0.82f, 0.52f, 0.30f), D2D1::ColorF(1.0f, 0.8f, 0.0f)},  // Orange clay, Yellow ball
    
    {WIMBLEDON_GRASS, L"Wimbledon\n(Grass)", 0.70f, 0.4f, 
     D2D1::ColorF(0.2f, 0.6f, 0.2f), D2D1::ColorF(0.0f, 1.0f, 0.0f)},      // Green grass, Bright green ball
    
    {US_OPEN_HARD, L"US Open\n(Hard Court)", 0.73f, 0.5f, 
     D2D1::ColorF(0.2f, 0.4f, 0.7f), D2D1::ColorF(1.0f, 0.3f, 0.3f)},      // Blue hard court, Red ball
    
    {LAVER_CUP_BLACK, L"Laver Cup\n(Black Court)", 0.72f, 0.5f, 
     D2D1::ColorF(0.15f, 0.15f, 0.15f), D2D1::ColorF(1.0f, 1.0f, 1.0f)}    // Black court, White ball
};

// Bounce data structure
struct BounceData {
    float time;
    float height;
};

// Tennis ball physics state
class TennisBall {
public:
    float y;              // Height in meters
    float vy;             // Vertical velocity in m/s
    float time;           // Elapsed time in seconds
    int bounceCount;
    bool isActive;
    CourtSurface* surface;
    std::vector<BounceData> trajectory;
    std::vector<BounceData> bounces;
    
    TennisBall(CourtSurface* courtSurface) {
        surface = courtSurface;
        reset();
    }
    
    void reset() {
        y = INITIAL_HEIGHT;
        vy = 0.0f;
        time = 0.0f;
        bounceCount = 0;
        isActive = true;
        trajectory.clear();
        bounces.clear();
        // Record initial position
        trajectory.push_back({time, y});
    }
    
    void update(float dt) {
        if (!isActive) return;
        
        time += dt;
        
        // Physics update
        vy -= GRAVITY * dt;  // Apply gravity
        y += vy * dt;        // Update position
        
        // Record trajectory
        trajectory.push_back({time, y});
        
        // Check for ground collision
        if (y <= 0.0f) {
            y = 0.0f;
            
            // Record bounce if we haven't recorded 3 yet
            if (bounceCount < 3) {
                bounces.push_back({time, 0.0f});
            }
            
            // Apply coefficient of restitution
            vy = -vy * surface->coefficientOfRestitution;
            
            bounceCount++;
            
            // Stop if velocity is too low or we've bounced enough
            if (fabs(vy) < 0.1f || bounceCount > 10) {
                isActive = false;
                vy = 0.0f;
            }
        }
    }
};

// Direct2D Application
class D2DApp {
private:
    HWND hwnd;
    ID2D1Factory* pFactory;
    ID2D1HwndRenderTarget* pRenderTarget;
    IDWriteFactory* pDWriteFactory;
    IDWriteTextFormat* pTextFormat;
    IDWriteTextFormat* pSmallTextFormat;
    
    ID2D1SolidColorBrush* pBrush;
    TennisBall* balls[4];
    bool simulationStarted;
    bool simulationComplete;
    
public:
    D2DApp() : hwnd(NULL), pFactory(NULL), pRenderTarget(NULL), 
               pDWriteFactory(NULL), pTextFormat(NULL), pSmallTextFormat(NULL),
               pBrush(NULL), simulationStarted(false), simulationComplete(false) {
        for (int i = 0; i < 4; i++) {
            balls[i] = new TennisBall(&courts[i]);
        }
    }
    
    ~D2DApp() {
        SafeRelease(&pBrush);
        SafeRelease(&pTextFormat);
        SafeRelease(&pSmallTextFormat);
        SafeRelease(&pRenderTarget);
        SafeRelease(&pFactory);
        SafeRelease(&pDWriteFactory);
        for (int i = 0; i < 4; i++) {
            delete balls[i];
        }
    }
    
    template <class T>
    void SafeRelease(T** ppT) {
        if (*ppT) {
            (*ppT)->Release();
            *ppT = NULL;
        }
    }
    
    HRESULT Initialize(HWND hwnd) {
        this->hwnd = hwnd;
        
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
        if (SUCCEEDED(hr)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            hr = pFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(
                    hwnd,
                    D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
                ),
                &pRenderTarget
            );
        }
        
        if (SUCCEEDED(hr)) {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pBrush);
        }
        
        if (SUCCEEDED(hr)) {
            hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&pDWriteFactory)
            );
        }
        
        if (SUCCEEDED(hr)) {
            hr = pDWriteFactory->CreateTextFormat(
                L"Arial",
                NULL,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                14.0f,
                L"en-us",
                &pTextFormat
            );
        }
        
        if (SUCCEEDED(hr)) {
            hr = pDWriteFactory->CreateTextFormat(
                L"Arial",
                NULL,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                10.0f,
                L"en-us",
                &pSmallTextFormat
            );
        }
        
        return hr;
    }
    
    void StartSimulation() {
        simulationStarted = true;
        simulationComplete = false;
        for (int i = 0; i < 4; i++) {
            balls[i]->reset();
        }
    }
    
    void Update() {
        if (!simulationStarted || simulationComplete) return;
        
        bool anyActive = false;
        for (int i = 0; i < 4; i++) {
            balls[i]->update(DT);
            if (balls[i]->isActive) anyActive = true;
        }
        
        if (!anyActive) {
            simulationComplete = true;
        }
    }
    
    void Render() {
        if (!pRenderTarget) return;
        
        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        
        // Draw each court section
        for (int i = 0; i < 4; i++) {
            float xOffset = i * SECTION_WIDTH;
            DrawCourtSection(i, xOffset);
        }
        
        // Draw combined graph at the bottom
        DrawCombinedGraph();
        
        // Draw instructions
        if (!simulationStarted) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            D2D1_RECT_F textRect = D2D1::RectF(10, WINDOW_HEIGHT - 30, WINDOW_WIDTH - 10, WINDOW_HEIGHT - 10);
            pRenderTarget->DrawTextW(
                L"Press SPACE to start simulation | Press R to reset",
                52,
                pTextFormat,
                textRect,
                pBrush
            );
        }
        
        pRenderTarget->EndDraw();
    }
    
    void DrawCourtSection(int index, float xOffset) {
        TennisBall* ball = balls[index];
        CourtSurface* surface = ball->surface;
        
        // Draw court floor
        pBrush->SetColor(surface->color);
        D2D1_RECT_F courtRect = D2D1::RectF(
            xOffset, 
            WINDOW_HEIGHT - 280, 
            xOffset + SECTION_WIDTH, 
            WINDOW_HEIGHT - 180
        );
        pRenderTarget->FillRectangle(courtRect, pBrush);
        
        // Draw court name
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F nameRect = D2D1::RectF(
            xOffset + 5, 
            WINDOW_HEIGHT - 275, 
            xOffset + SECTION_WIDTH - 5, 
            WINDOW_HEIGHT - 240
        );
        pRenderTarget->DrawTextW(
            surface->name,
            wcslen(surface->name),
            pSmallTextFormat,
            nameRect,
            pBrush
        );
        
        // Draw ball
        if (simulationStarted) {
            float ballX = xOffset + SECTION_WIDTH / 2;
            float ballY = WINDOW_HEIGHT - 180 - (ball->y * 50.0f); // Scale: 50 pixels per meter
            
            pBrush->SetColor(surface->ballColor);
            D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(
                D2D1::Point2F(ballX, ballY),
                8.0f, 8.0f
            );
            pRenderTarget->FillEllipse(ballEllipse, pBrush);
            
            // Draw height marker
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Gray));
            D2D1_POINT_2F p1 = D2D1::Point2F(xOffset + 5, ballY);
            D2D1_POINT_2F p2 = D2D1::Point2F(xOffset + 15, ballY);
            pRenderTarget->DrawLine(p1, p2, pBrush, 1.0f);
            
            // Draw telemetry
            wchar_t telemetry[256];
            swprintf_s(telemetry, L"Time: %.2fs\nHeight: %.2fm\nBounces: %d",
                ball->time, ball->y, ball->bounceCount);
            
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            D2D1_RECT_F telemetryRect = D2D1::RectF(
                xOffset + 5,
                WINDOW_HEIGHT - 230,
                xOffset + SECTION_WIDTH - 5,
                WINDOW_HEIGHT - 180
            );
            pRenderTarget->DrawTextW(
                telemetry,
                wcslen(telemetry),
                pSmallTextFormat,
                telemetryRect,
                pBrush
            );
            
            // Draw bounce markers
            for (size_t b = 0; b < ball->bounces.size() && b < 3; b++) {
                float bounceX = xOffset + SECTION_WIDTH / 2;
                float bounceY = WINDOW_HEIGHT - 180;
                
                pBrush->SetColor(D2D1::ColorF(1.0f, 0.0f, 0.0f, 0.7f));
                D2D1_ELLIPSE bounceMarker = D2D1::Ellipse(
                    D2D1::Point2F(bounceX, bounceY),
                    4.0f, 4.0f
                );
                pRenderTarget->FillEllipse(bounceMarker, pBrush);
            }
        }
    }
    
    void DrawCombinedGraph() {
        if (!simulationStarted || balls[0]->trajectory.size() < 2) return;
        
        const float graphX = 10;
        const float graphY = 10;
        const float graphWidth = WINDOW_WIDTH - 20;
        const float graphHeight = 150;
        
        // Draw graph background
        pBrush->SetColor(D2D1::ColorF(0.1f, 0.1f, 0.1f, 0.8f));
        D2D1_RECT_F graphRect = D2D1::RectF(graphX, graphY, graphX + graphWidth, graphY + graphHeight);
        pRenderTarget->FillRectangle(graphRect, pBrush);
        
        // Draw graph border
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRenderTarget->DrawRectangle(graphRect, pBrush, 1.0f);
        
        // Draw title
        D2D1_RECT_F titleRect = D2D1::RectF(graphX + 5, graphY + 5, graphX + graphWidth - 5, graphY + 25);
        pRenderTarget->DrawTextW(
            L"Height vs Time (All Courts)",
            29,
            pSmallTextFormat,
            titleRect,
            pBrush
        );
        
        // Find max time for scaling
        float maxTime = 0.0f;
        for (int i = 0; i < 4; i++) {
            if (!balls[i]->trajectory.empty()) {
                maxTime = max(maxTime, balls[i]->trajectory.back().time);
            }
        }
        
        if (maxTime < 0.1f) maxTime = 1.0f;
        
        const float maxHeight = 2.5f; // meters
        const float plotY = graphY + 30;
        const float plotHeight = graphHeight - 40;
        
        // Draw grid lines
        pBrush->SetColor(D2D1::ColorF(0.3f, 0.3f, 0.3f));
        for (int i = 0; i <= 5; i++) {
            float y = plotY + (plotHeight * i / 5.0f);
            pRenderTarget->DrawLine(
                D2D1::Point2F(graphX, y),
                D2D1::Point2F(graphX + graphWidth, y),
                pBrush, 0.5f
            );
        }
        
        // Draw trajectories
        for (int i = 0; i < 4; i++) {
            TennisBall* ball = balls[i];
            if (ball->trajectory.size() < 2) continue;
            
            pBrush->SetColor(ball->surface->ballColor);
            
            for (size_t j = 1; j < ball->trajectory.size(); j++) {
                float x1 = graphX + (ball->trajectory[j-1].time / maxTime) * graphWidth;
                float y1 = plotY + plotHeight - (ball->trajectory[j-1].height / maxHeight) * plotHeight;
                float x2 = graphX + (ball->trajectory[j].time / maxTime) * graphWidth;
                float y2 = plotY + plotHeight - (ball->trajectory[j].height / maxHeight) * plotHeight;
                
                // Clamp to graph bounds
                y1 = max(plotY, min(plotY + plotHeight, y1));
                y2 = max(plotY, min(plotY + plotHeight, y2));
                
                pRenderTarget->DrawLine(
                    D2D1::Point2F(x1, y1),
                    D2D1::Point2F(x2, y2),
                    pBrush, 2.0f
                );
            }
            
            // Draw legend
            float legendX = graphX + 10 + (i * 150);
            float legendY = graphY + graphHeight - 15;
            
            pBrush->SetColor(ball->surface->ballColor);
            D2D1_ELLIPSE legendDot = D2D1::Ellipse(
                D2D1::Point2F(legendX, legendY),
                4.0f, 4.0f
            );
            pRenderTarget->FillEllipse(legendDot, pBrush);
            
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            const wchar_t* labels[] = {L"Clay", L"Grass", L"Hard", L"Black"};
            D2D1_RECT_F legendRect = D2D1::RectF(
                legendX + 10, legendY - 8,
                legendX + 140, legendY + 8
            );
            pRenderTarget->DrawTextW(
                labels[i],
                wcslen(labels[i]),
                pSmallTextFormat,
                legendRect,
                pBrush
            );
        }
    }
    
    void OnKeyPress(WPARAM wParam) {
        if (wParam == VK_SPACE) {
            StartSimulation();
        } else if (wParam == 'R' || wParam == 'r') {
            simulationStarted = false;
            simulationComplete = false;
            for (int i = 0; i < 4; i++) {
                balls[i]->reset();
            }
        }
    }
};

// Global variables
D2DApp* g_pApp = NULL;

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            g_pApp = new D2DApp();
            if (g_pApp) {
                g_pApp->Initialize(hwnd);
            }
            SetTimer(hwnd, 1, 16, NULL); // ~60 FPS
            return 0;
            
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            delete g_pApp;
            PostQuitMessage(0);
            return 0;
            
        case WM_TIMER:
            if (g_pApp) {
                g_pApp->Update();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
            
        case WM_PAINT:
            if (g_pApp) {
                g_pApp->Render();
                ValidateRect(hwnd, NULL);
            }
            return 0;
            
        case WM_KEYDOWN:
            if (g_pApp) {
                g_pApp->OnKeyPress(wParam);
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Main entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Register window class
    const wchar_t CLASS_NAME[] = L"TennisBallPhysicsSimulator";
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClass(&wc);
    
    // Create window
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Tennis Ball Physics Simulator - 4 Court Surfaces",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH + 16, WINDOW_HEIGHT + 39, // Account for window borders
        NULL,
        NULL,
        hInstance,
        NULL
    );
    
    if (hwnd == NULL) {
        return 0;
    }
    
    ShowWindow(hwnd, nCmdShow);
    
    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}

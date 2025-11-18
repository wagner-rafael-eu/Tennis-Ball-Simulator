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
#include <cstdlib>
#include <ctime>
#include <commctrl.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "comctl32")

// Screen identifiers
const char* SCREEN_ALL = "All Courts View";
const char* SCREEN_CLAY = "Clay Court View";
const char* SCREEN_GRASS = "Grass Court View";
const char* SCREEN_HARD = "Hard Court View";
const char* SCREEN_LAVER = "Laver Cup View";

// Screen state
enum ScreenMode {
    MODE_ALL,
    MODE_CLAY,
    MODE_GRASS,
    MODE_HARD,
    MODE_LAVER
};

// Constants
const float GRAVITY = 9.81f; // m/s^2
const float BALL_RADIUS = 0.0335f; // Tennis ball radius in meters (6.7cm diameter)
const float INITIAL_HEIGHT = 2.0f; // meters
const float PIXELS_PER_METER = 100.0f; // Scaling factor for visualization
const int WINDOW_WIDTH = 640;
const int WINDOW_HEIGHT = 480;
const int SECTION_WIDTH = WINDOW_WIDTH / 4;
const float DT = 0.0083f; // ~120 FPS

// Clay court specific constants
const float COURT_WIDTH = 23.77f; // Tennis court width in meters (singles)
const float COURT_LENGTH = 23.77f; // Tennis court length in meters
const float NET_HEIGHT = 0.914f; // Net height at center in meters
const float BALL_MASS = 0.058f; // Tennis ball mass in kg
const float MIN_HORIZONTAL_FORCE = 0.0f; // Newtons
const float MAX_HORIZONTAL_FORCE = 1000.0f; // Newtons
const float MIN_ANGLE = 0.0f; // degrees
const float MAX_ANGLE = 90.0f; // degrees

// Configurable settings (loaded from settings.ini)
float DEFAULT_HORIZONTAL_FORCE = 270.0f; // Newtons
float DEFAULT_ANGLE = 39.0f; // degrees
float ANGLE_STEP = 3.0f; // degrees per scroll
float DEFAULT_SPIN = 120.0f; // RPM
float SPIN_STEP = 60.0f; // RPM per key press
float MAX_SPIN = 9000.0f; // Maximum spin in RPM
float MIN_SPIN = -3000.0f; // Minimum backspin in RPM
float DEFAULT_PACE = 2.0f; // Visual pace multiplier (2.0 = 200%)
float RIGHTY_SPEED = 4.0f; // RIGHTY movement speed in m/s

// Function to load settings from INI file
void LoadSettings() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of(L"\\");
    if (lastSlash != std::wstring::npos) {
        exeDir = exeDir.substr(0, lastSlash + 1);
    }
    std::wstring iniPath = exeDir + L"settings.ini";
    
    // Load settings with defaults
    DEFAULT_HORIZONTAL_FORCE = GetPrivateProfileIntW(L"Physics", L"DefaultForce", 270, iniPath.c_str());
    DEFAULT_ANGLE = GetPrivateProfileIntW(L"Physics", L"DefaultAngle", 39, iniPath.c_str());
    ANGLE_STEP = GetPrivateProfileIntW(L"Physics", L"AngleStep", 3, iniPath.c_str());
    DEFAULT_SPIN = GetPrivateProfileIntW(L"Physics", L"DefaultSpin", 120, iniPath.c_str());
    SPIN_STEP = GetPrivateProfileIntW(L"Physics", L"SpinStep", 60, iniPath.c_str());
    MIN_SPIN = GetPrivateProfileIntW(L"Physics", L"MinSpin", -3000, iniPath.c_str());
    MAX_SPIN = GetPrivateProfileIntW(L"Physics", L"MaxSpin", 9000, iniPath.c_str());
    DEFAULT_PACE = GetPrivateProfileIntW(L"Physics", L"DefaultPace", 200, iniPath.c_str()) / 100.0f; // Convert percentage to multiplier
    RIGHTY_SPEED = GetPrivateProfileIntW(L"Physics", L"RightySpeed", 4, iniPath.c_str());
}

// Air resistance modes
enum AirResistanceMode {
    AIR_VACUUM,
    AIR_SEA_LEVEL,
    AIR_1000M,
    AIR_2000M
};

struct AirResistanceData {
    AirResistanceMode mode;
    const wchar_t* name;
    float coefficient;
};

// Air resistance coefficients based on altitude
// Coefficient formula: 0.5 * Cd * rho * A, where:
// Cd ~= 0.5 (drag coefficient for sphere)
// A = pi * r^2 ~= 0.00352 m^2 (tennis ball cross-section)
// rho varies with altitude
AirResistanceData airModes[4] = {
    {AIR_VACUUM, L"Vacuum (no air)", 0.0f},
    {AIR_SEA_LEVEL, L"Sea Level", 0.0005f},      // rho = 1.225 kg/m^3
    {AIR_1000M, L"1000m altitude", 0.00044f},    // rho = 1.112 kg/m^3 (90% of sea level)
    {AIR_2000M, L"2000m altitude", 0.00039f}     // rho = 1.007 kg/m^3 (82% of sea level)
};

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

// RIGHTY hit dialog parameters
struct RightyHitParams {
    float force;
    float angle;
    float spin;
    bool confirmed;
};

// Forward declaration
bool ShowRightyHitDialog(HWND hwndParent, RightyHitParams* params);

// Tennis ball physics state
class TennisBall {
public:
    float y;              // Height in meters
    float vy;             // Vertical velocity in m/s
    float x;              // Horizontal position in meters
    float vx;             // Horizontal velocity in m/s
    float time;           // Elapsed time in seconds
    int bounceCount;
    bool isActive;
    CourtSurface* surface;
    float airResistanceCoeff; // Air resistance coefficient
    float spinRPM;        // Ball spin in revolutions per minute (positive = topspin, negative = backspin)
    std::vector<BounceData> trajectory;
    std::vector<BounceData> bounces;
    
    TennisBall(CourtSurface* courtSurface) {
        surface = courtSurface;
        airResistanceCoeff = 0.0f;
        spinRPM = 0.0f;
        reset();
    }
    
    void reset() {
        y = INITIAL_HEIGHT;
        vy = 0.0f;
        x = 0.0f;
        vx = 0.0f;
        time = 0.0f;
        bounceCount = 0;
        isActive = true;
        trajectory.clear();
        bounces.clear();
        // Record initial position
        trajectory.push_back({time, y});
    }
    
    void resetForHorizontalShot(float horizontalForce, float angleDegrees, float spin) {
        // LEFTY position: 20 pixels from left court edge
        // Convert 20 pixels to meters based on court scaling
        x = (20.0f / (WINDOW_WIDTH - 100.0f)) * COURT_LENGTH; // Start from LEFTY position
        y = 1.0f; // Start at net height
        // Map force (0-1000N) to realistic tennis velocities (0-50 m/s)
        // Professional tennis serves: 50-70 m/s, groundstrokes: 20-40 m/s
        float totalVelocity = (horizontalForce / MAX_HORIZONTAL_FORCE) * 50.0f;
        
        // Convert angle to radians and calculate velocity components
        float angleRad = angleDegrees * 3.14159265f / 180.0f;
        vx = totalVelocity * cos(angleRad);
        vy = totalVelocity * sin(angleRad);
        
        spinRPM = spin;
        time = 0.0f;
        bounceCount = 0;
        isActive = true;
        trajectory.clear();
        bounces.clear();
        trajectory.push_back({time, y});
    }
    
    void setAirResistance(float coefficient) {
        airResistanceCoeff = coefficient;
    }
    
    void update(float dt) {
        if (!isActive) return;
        
        // Store previous position for net collision detection
        float prevX = x;
        float prevY = y;
        
        time += dt;
        
        // Physics update
        vy -= GRAVITY * dt;  // Apply gravity
        
        // Magnus effect from spin
        // Convert RPM to rad/s: omega = RPM * 2*pi / 60
        float omega = spinRPM * 2.0f * 3.14159265f / 60.0f;
        // Magnus force coefficient: Cl ~= 0.3 for tennis ball
        // Magnus force = 0.5 * Cl * rho * A * r * omega * v
        // Simplified: F_magnus = k * omega * v, where k incorporates constants
        float magnusCoeff = 0.00015f; // Tuned coefficient
        float ballSpeed = sqrt(vx * vx + vy * vy);
        
        if (ballSpeed > 0.1f) {
            // Magnus force perpendicular to velocity
            // Topspin (positive) curves down, backspin (negative) curves up
            float magnusForce = magnusCoeff * omega * ballSpeed;
            float magnusAccelY = magnusForce / BALL_MASS;
            
            // Apply Magnus acceleration (perpendicular to velocity direction)
            vy -= magnusAccelY * dt;
        }
        
        y += vy * dt;        // Update position
        
        // Horizontal physics with air resistance
        float airResistanceForce = -airResistanceCoeff * vx * fabs(vx);
        float ax = airResistanceForce / BALL_MASS;
        vx += ax * dt;
        x += vx * dt;
        
        // Net collision detection
        const float NET_X = COURT_LENGTH / 2.0f; // Net is at center of court
        const float NET_ABSORPTION = 0.80f; // Net absorbs 80% of force, returns 20%
        
        // Check if ball crossed the net plane
        bool crossedNet = (prevX < NET_X && x >= NET_X) || (prevX > NET_X && x <= NET_X);
        
        if (crossedNet) {
            // Linear interpolation to find exact collision point
            float t = (NET_X - prevX) / (x - prevX); // Interpolation factor
            float collisionY = prevY + t * (y - prevY);
            
            // Check if ball hit the net (collision height is below net height + ball radius)
            if (collisionY <= NET_HEIGHT + BALL_RADIUS) {
                // Ball hit the net!
                // Position ball at net surface
                x = NET_X;
                y = collisionY;
                
                // Net absorbs 80% of force, reflects 20% back
                // Reflect horizontal velocity with 80% energy absorption
                vx = -vx * (1.0f - NET_ABSORPTION);
                
                // Apply 80% absorption to vertical velocity as well
                vy *= (1.0f - NET_ABSORPTION);
                
                // Add some random deflection for realism
                float randomDeflection = ((rand() % 100) / 100.0f - 0.5f) * 0.3f; // -0.15 to +0.15 m/s
                vy += randomDeflection;
                
                // Reduce spin on net collision (80% absorption)
                spinRPM *= (1.0f - NET_ABSORPTION);
                
                // If ball is moving very slowly after net collision, it might drop straight down
                if (fabs(vx) < 0.5f && fabs(vy) < 0.5f) {
                    vx = 0.0f;
                }
            }
        }
        
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
            vx *= 0.8f; // Horizontal velocity reduction on bounce
            
            // Spin affects bounce: topspin increases forward velocity, backspin decreases it
            float spinEffect = (spinRPM / 5000.0f) * 2.0f; // Normalized spin effect
            vx += spinEffect;
            
            // Spin decays on bounce
            spinRPM *= 0.7f;
            
            bounceCount++;
            
            // Stop if velocity is too low or we've bounced enough
            if (fabs(vy) < 0.1f || bounceCount > 10) {
                isActive = false;
                vy = 0.0f;
                vx = 0.0f;
            }
        }
        
        // Stop if ball goes out of bounds horizontally
        if (x < 0.0f || x > COURT_LENGTH) {
            isActive = false;
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
    
    ScreenMode currentScreen;
    TennisBall* clayBall;
    TennisBall* grassBall;
    TennisBall* hardBall;
    TennisBall* laverBall;
    float horizontalForce;
    float launchAngle; // Launch angle in degrees
    float ballSpin; // Ball spin in RPM
    float visualPaceMultiplier; // Visual speed multiplier
    AirResistanceMode airResistanceMode;
    D2D1_RECT_F comboBoxRect;
    
    // Auto-relaunch state
    bool waitingToRelaunch;
    float relaunchTimer;
    const float RELAUNCH_DELAY = 2.0f; // 2 seconds
    
    // RIGHTY position (in meters from left edge of court)
    float rightyPosition;
    
    // RIGHTY hit dialog parameters
    bool ballHitRighty;
    bool simulationPaused;
    float rightyHitForce;
    float rightyHitAngle;
    float rightyHitSpin;
    
public:
    D2DApp() : hwnd(NULL), pFactory(NULL), pRenderTarget(NULL), 
               pDWriteFactory(NULL), pTextFormat(NULL), pSmallTextFormat(NULL),
               pBrush(NULL), simulationStarted(false), simulationComplete(false),
               currentScreen(MODE_ALL), horizontalForce(DEFAULT_HORIZONTAL_FORCE), launchAngle(DEFAULT_ANGLE),
               ballSpin(DEFAULT_SPIN), visualPaceMultiplier(DEFAULT_PACE), airResistanceMode(AIR_SEA_LEVEL),
               waitingToRelaunch(false), relaunchTimer(0.0f), rightyPosition(COURT_LENGTH - 1.0f),
               ballHitRighty(false), simulationPaused(false), rightyHitForce(300.0f), rightyHitAngle(30.0f), rightyHitSpin(120.0f) {
        for (int i = 0; i < 4; i++) {
            balls[i] = new TennisBall(&courts[i]);
        }
        clayBall = new TennisBall(&courts[0]); // Use clay court properties
        grassBall = new TennisBall(&courts[1]); // Use grass court properties
        hardBall = new TennisBall(&courts[2]); // Use hard court properties
        laverBall = new TennisBall(&courts[3]); // Use Laver Cup properties
        
        // Initialize combo box position
        comboBoxRect = D2D1::RectF(10, 420, 200, 445);
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
        delete clayBall;
        delete grassBall;
        delete hardBall;
        delete laverBall;
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
        waitingToRelaunch = false;
        relaunchTimer = 0.0f;
        
        if (currentScreen == MODE_ALL) {
            for (int i = 0; i < 4; i++) {
                balls[i]->reset();
            }
        } else if (currentScreen == MODE_CLAY) {
            clayBall->setAirResistance(airModes[airResistanceMode].coefficient);
            clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        } else if (currentScreen == MODE_GRASS) {
            grassBall->setAirResistance(airModes[airResistanceMode].coefficient);
            grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        } else if (currentScreen == MODE_HARD) {
            hardBall->setAirResistance(airModes[airResistanceMode].coefficient);
            hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        } else if (currentScreen == MODE_LAVER) {
            laverBall->setAirResistance(airModes[airResistanceMode].coefficient);
            laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        }
    }
    
    void Update() {
        if (!simulationStarted || simulationComplete || simulationPaused) return;
        
        float adjustedDT = DT * visualPaceMultiplier;
        
        // Update RIGHTY position based on keyboard input (for individual court screens)
        if (currentScreen != MODE_ALL) {
            const float NET_X = COURT_LENGTH / 2.0f; // Net position at center of court
            
            if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
                rightyPosition -= ::RIGHTY_SPEED * DT; // Use raw DT, not affected by visual pace
                // Don't allow RIGHTY to cross the net to the left
                rightyPosition = max(NET_X, rightyPosition);
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
                rightyPosition += ::RIGHTY_SPEED * DT; // Use raw DT, not affected by visual pace
                rightyPosition = min(COURT_LENGTH, rightyPosition);
            }
        }
        
        if (currentScreen == MODE_ALL) {
            bool anyActive = false;
            for (int i = 0; i < 4; i++) {
                balls[i]->update(adjustedDT);
                if (balls[i]->isActive) anyActive = true;
            }
            
            if (!anyActive) {
                simulationComplete = true;
            }
        } else if (currentScreen == MODE_CLAY) {
            // Check if ball reached right end of court or stopped moving
            if (clayBall->isActive && clayBall->x >= COURT_LENGTH) {
                clayBall->isActive = false;
                waitingToRelaunch = true;
                relaunchTimer = 0.0f;
            }
            
            // Handle relaunch timer
            if (waitingToRelaunch) {
                relaunchTimer += adjustedDT;
                if (relaunchTimer >= RELAUNCH_DELAY) {
                    // Random force: 200-400N
                    horizontalForce = 200.0f + (rand() % 201);
                    // Random angle: 9-39 degrees
                    launchAngle = 9.0f + (rand() % 31);
                    // Random spin: 60-600 RPM
                    ballSpin = 60.0f + (rand() % 541);
                    
                    clayBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    waitingToRelaunch = false;
                    relaunchTimer = 0.0f;
                }
            } else {
                clayBall->update(adjustedDT);
                
                // Check for RIGHTY collision
                if (CheckRightyCollision(clayBall)) {
                    simulationPaused = true;
                    ballHitRighty = true;
                    
                    // Show dialog for hit parameters
                    RightyHitParams params;
                    params.force = rightyHitForce;
                    params.angle = rightyHitAngle;
                    params.spin = rightyHitSpin;
                    params.confirmed = false;
                    
                    if (ShowRightyHitDialog(hwnd, &params) && params.confirmed) {
                        rightyHitForce = params.force;
                        rightyHitAngle = params.angle;
                        rightyHitSpin = params.spin;
                        ApplyRightyHit(clayBall, params.force, params.angle, params.spin);
                    } else {
                        // User cancelled, just bounce back
                        clayBall->vx = -clayBall->vx * 0.5f;
                        clayBall->x = rightyPosition - 0.1f;
                        simulationPaused = false;
                        ballHitRighty = false;
                    }
                }
                
                // Check if ball stopped moving
                if (!clayBall->isActive) {
                    waitingToRelaunch = true;
                    relaunchTimer = 0.0f;
                }
            }
        } else if (currentScreen == MODE_GRASS) {
            // Check if ball reached right end of court or stopped moving
            if (grassBall->isActive && grassBall->x >= COURT_LENGTH) {
                grassBall->isActive = false;
                waitingToRelaunch = true;
                relaunchTimer = 0.0f;
            }
            
            // Handle relaunch timer
            if (waitingToRelaunch) {
                relaunchTimer += adjustedDT;
                if (relaunchTimer >= RELAUNCH_DELAY) {
                    // Random force: 200-400N
                    horizontalForce = 200.0f + (rand() % 201);
                    // Random angle: 9-39 degrees
                    launchAngle = 9.0f + (rand() % 31);
                    // Random spin: 60-600 RPM
                    ballSpin = 60.0f + (rand() % 541);
                    
                    grassBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    waitingToRelaunch = false;
                    relaunchTimer = 0.0f;
                }
            } else {
                grassBall->update(adjustedDT);
                
                // Check for RIGHTY collision
                if (CheckRightyCollision(grassBall)) {
                    simulationPaused = true;
                    ballHitRighty = true;
                    
                    // Show dialog for hit parameters
                    RightyHitParams params;
                    params.force = rightyHitForce;
                    params.angle = rightyHitAngle;
                    params.spin = rightyHitSpin;
                    params.confirmed = false;
                    
                    if (ShowRightyHitDialog(hwnd, &params) && params.confirmed) {
                        rightyHitForce = params.force;
                        rightyHitAngle = params.angle;
                        rightyHitSpin = params.spin;
                        ApplyRightyHit(grassBall, params.force, params.angle, params.spin);
                    } else {
                        // User cancelled, just bounce back
                        grassBall->vx = -grassBall->vx * 0.5f;
                        grassBall->x = rightyPosition - 0.1f;
                        simulationPaused = false;
                        ballHitRighty = false;
                    }
                }
                
                // Check if ball stopped moving
                if (!grassBall->isActive) {
                    waitingToRelaunch = true;
                    relaunchTimer = 0.0f;
                }
            }
        } else if (currentScreen == MODE_HARD) {
            // Check if ball reached right end of court or stopped moving
            if (hardBall->isActive && hardBall->x >= COURT_LENGTH) {
                hardBall->isActive = false;
                waitingToRelaunch = true;
                relaunchTimer = 0.0f;
            }
            
            // Handle relaunch timer
            if (waitingToRelaunch) {
                relaunchTimer += adjustedDT;
                if (relaunchTimer >= RELAUNCH_DELAY) {
                    // Random force: 200-400N
                    horizontalForce = 200.0f + (rand() % 201);
                    // Random angle: 9-39 degrees
                    launchAngle = 9.0f + (rand() % 31);
                    // Random spin: 60-600 RPM
                    ballSpin = 60.0f + (rand() % 541);
                    
                    hardBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    waitingToRelaunch = false;
                    relaunchTimer = 0.0f;
                }
            } else {
                hardBall->update(adjustedDT);
                
                // Check for RIGHTY collision
                if (CheckRightyCollision(hardBall)) {
                    simulationPaused = true;
                    ballHitRighty = true;
                    
                    // Show dialog for hit parameters
                    RightyHitParams params;
                    params.force = rightyHitForce;
                    params.angle = rightyHitAngle;
                    params.spin = rightyHitSpin;
                    params.confirmed = false;
                    
                    if (ShowRightyHitDialog(hwnd, &params) && params.confirmed) {
                        rightyHitForce = params.force;
                        rightyHitAngle = params.angle;
                        rightyHitSpin = params.spin;
                        ApplyRightyHit(hardBall, params.force, params.angle, params.spin);
                    } else {
                        // User cancelled, just bounce back
                        hardBall->vx = -hardBall->vx * 0.5f;
                        hardBall->x = rightyPosition - 0.1f;
                        simulationPaused = false;
                        ballHitRighty = false;
                    }
                }
                
                // Check if ball stopped moving
                if (!hardBall->isActive) {
                    waitingToRelaunch = true;
                    relaunchTimer = 0.0f;
                }
            }
        } else if (currentScreen == MODE_LAVER) {
            // Check if ball reached right end of court or stopped moving
            if (laverBall->isActive && laverBall->x >= COURT_LENGTH) {
                laverBall->isActive = false;
                waitingToRelaunch = true;
                relaunchTimer = 0.0f;
            }
            
            // Handle relaunch timer
            if (waitingToRelaunch) {
                relaunchTimer += adjustedDT;
                if (relaunchTimer >= RELAUNCH_DELAY) {
                    // Random force: 200-400N
                    horizontalForce = 200.0f + (rand() % 201);
                    // Random angle: 9-39 degrees
                    launchAngle = 9.0f + (rand() % 31);
                    // Random spin: 60-600 RPM
                    ballSpin = 60.0f + (rand() % 541);
                    
                    laverBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    waitingToRelaunch = false;
                    relaunchTimer = 0.0f;
                }
            } else {
                laverBall->update(adjustedDT);
                
                // Check for RIGHTY collision
                if (CheckRightyCollision(laverBall)) {
                    simulationPaused = true;
                    ballHitRighty = true;
                    
                    // Show dialog for hit parameters
                    RightyHitParams params;
                    params.force = rightyHitForce;
                    params.angle = rightyHitAngle;
                    params.spin = rightyHitSpin;
                    params.confirmed = false;
                    
                    if (ShowRightyHitDialog(hwnd, &params) && params.confirmed) {
                        rightyHitForce = params.force;
                        rightyHitAngle = params.angle;
                        rightyHitSpin = params.spin;
                        ApplyRightyHit(laverBall, params.force, params.angle, params.spin);
                    } else {
                        // User cancelled, just bounce back
                        laverBall->vx = -laverBall->vx * 0.5f;
                        laverBall->x = rightyPosition - 0.1f;
                        simulationPaused = false;
                        ballHitRighty = false;
                    }
                }
                
                // Check if ball stopped moving
                if (!laverBall->isActive) {
                    waitingToRelaunch = true;
                    relaunchTimer = 0.0f;
                }
            }
        }
    }
    
    void Render() {
        if (!pRenderTarget) return;
        
        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        
        if (currentScreen == MODE_ALL) {
            RenderAllCourts();
        } else if (currentScreen == MODE_CLAY) {
            RenderClayCourt();
        } else if (currentScreen == MODE_GRASS) {
            RenderGrassCourt();
        } else if (currentScreen == MODE_HARD) {
            RenderHardCourt();
        } else if (currentScreen == MODE_LAVER) {
            RenderLaverCourt();
        }
        
        pRenderTarget->EndDraw();
    }
    
    void RenderAllCourts() {
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
                L"SPACE: Start | R: Reset | C: Clay | G: Grass | H: Hard | L: Laver",
                68,
                pTextFormat,
                textRect,
                pBrush
            );
        }
    }
    
    void RenderClayCourt() {
        const float courtMargin = 50.0f;
        const float zoomFactor = 0.25f; // 4x wider court with 2x zoom out = 0.25 total
        const float courtPixelWidth = (WINDOW_WIDTH - 2 * courtMargin) * 4.0f * zoomFactor; // 4x width with zoom
        const float courtPixelHeight = 300.0f * zoomFactor;
        const float courtTop = 240.0f;
        const float courtBottom = courtTop + courtPixelHeight;
        
        // Draw clay court
        pBrush->SetColor(courts[0].color); // Clay color
        D2D1_RECT_F courtRect = D2D1::RectF(
            courtMargin,
            courtTop,
            courtMargin + courtPixelWidth,
            courtBottom
        );
        pRenderTarget->FillRectangle(courtRect, pBrush);
        
        // Draw court outline
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRenderTarget->DrawRectangle(courtRect, pBrush, 2.0f);
        
        // Draw net in the middle
        float netX = courtMargin + courtPixelWidth / 2.0f;
        float netPixelHeight = NET_HEIGHT * 50.0f * zoomFactor; // Scale with zoom
        
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F netRect = D2D1::RectF(
            netX - 2.0f,
            courtBottom - netPixelHeight,
            netX + 2.0f,
            courtBottom
        );
        pRenderTarget->FillRectangle(netRect, pBrush);
        
        // Draw net top line
        pRenderTarget->DrawLine(
            D2D1::Point2F(netX - 10.0f, courtBottom - netPixelHeight),
            D2D1::Point2F(netX + 10.0f, courtBottom - netPixelHeight),
            pBrush, 2.0f
        );
        
        // Draw ball if simulation started
        if (simulationStarted && clayBall) {
            float ballPixelX = courtMargin + (clayBall->x / COURT_LENGTH) * courtPixelWidth;
            float ballPixelY = courtBottom - (clayBall->y * 50.0f * zoomFactor); // Scale with zoom
            
            pBrush->SetColor(courts[0].ballColor); // Yellow ball
            D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(
                D2D1::Point2F(ballPixelX, ballPixelY),
                10.0f * zoomFactor, 10.0f * zoomFactor // Scale ball size with zoom
            );
            pRenderTarget->FillEllipse(ballEllipse, pBrush);
            
            // Draw BALL label
            DrawBallLabel(ballPixelX, ballPixelY, zoomFactor);
        }
        
        // Draw court labels (NET, LEFTY, RIGHTY)
        DrawCourtLabels(courtMargin, courtPixelWidth, courtTop, courtBottom, zoomFactor);
        
        // Draw title
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F titleRect = D2D1::RectF(10, 10, WINDOW_WIDTH - 10, 40);
        pRenderTarget->DrawTextW(
            L"Clay Court - Horizontal Shot",
            30,
            pTextFormat,
            titleRect,
            pBrush
        );
        
        // Draw telemetry
        if (simulationStarted && clayBall) {
            wchar_t telemetry[512];
            swprintf_s(telemetry, 
                L"Time: %.2fs | X: %.2fm | Y: %.2fm | Vx: %.2fm/s | Vy: %.2fm/s\nForce: %.0fN | Angle: %.0f° | Spin: %.0f RPM | Pace: %.0f%% | Bounces: %d",
                clayBall->time, clayBall->x, clayBall->y, clayBall->vx, clayBall->vy,
                horizontalForce, launchAngle, clayBall->spinRPM, visualPaceMultiplier * 100.0f, clayBall->bounceCount);
            
            D2D1_RECT_F telemetryRect = D2D1::RectF(10, 40, WINDOW_WIDTH - 10, 90);
            pRenderTarget->DrawTextW(
                telemetry,
                wcslen(telemetry),
                pSmallTextFormat,
                telemetryRect,
                pBrush
            );
        }
        
        // Draw instructions
        if (!simulationStarted) {
            wchar_t instructions[256];
            swprintf_s(instructions, 
                L"SPACE: Start | R: Reset | UP/DOWN: Force (%.0fN) | SCROLL: Angle (%.0f°) | +/-: Pace (%.0f%%) | A: Back",
                horizontalForce, launchAngle, visualPaceMultiplier * 100.0f);
            
            D2D1_RECT_F instructRect = D2D1::RectF(10, WINDOW_HEIGHT - 30, WINDOW_WIDTH - 10, WINDOW_HEIGHT - 10);
            pRenderTarget->DrawTextW(
                instructions,
                wcslen(instructions),
                pSmallTextFormat,
                instructRect,
                pBrush
            );
        }
        
        // Draw air resistance combo box
        DrawComboBox();
    }
    
    void RenderGrassCourt() {
        const float courtMargin = 50.0f;
        const float zoomFactor = 0.25f; // 4x wider court with 2x zoom out = 0.25 total
        const float courtPixelWidth = (WINDOW_WIDTH - 2 * courtMargin) * 4.0f * zoomFactor; // 4x width with zoom
        const float courtPixelHeight = 300.0f * zoomFactor;
        const float courtTop = 240.0f;
        const float courtBottom = courtTop + courtPixelHeight;
        
        // Draw grass court
        pBrush->SetColor(courts[1].color); // Grass color
        D2D1_RECT_F courtRect = D2D1::RectF(
            courtMargin,
            courtTop,
            courtMargin + courtPixelWidth,
            courtBottom
        );
        pRenderTarget->FillRectangle(courtRect, pBrush);
        
        // Draw court outline
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRenderTarget->DrawRectangle(courtRect, pBrush, 2.0f);
        
        // Draw net in the middle
        float netX = courtMargin + courtPixelWidth / 2.0f;
        float netPixelHeight = NET_HEIGHT * 50.0f * zoomFactor; // Scale with zoom
        
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F netRect = D2D1::RectF(
            netX - 2.0f,
            courtBottom - netPixelHeight,
            netX + 2.0f,
            courtBottom
        );
        pRenderTarget->FillRectangle(netRect, pBrush);
        
        // Draw net top line
        pRenderTarget->DrawLine(
            D2D1::Point2F(netX - 10.0f, courtBottom - netPixelHeight),
            D2D1::Point2F(netX + 10.0f, courtBottom - netPixelHeight),
            pBrush, 2.0f
        );
        
        // Draw ball if simulation started
        if (simulationStarted && grassBall) {
            float ballPixelX = courtMargin + (grassBall->x / COURT_LENGTH) * courtPixelWidth;
            float ballPixelY = courtBottom - (grassBall->y * 50.0f * zoomFactor); // Scale with zoom
            
            pBrush->SetColor(courts[1].ballColor); // Bright green ball
            D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(
                D2D1::Point2F(ballPixelX, ballPixelY),
                10.0f * zoomFactor, 10.0f * zoomFactor // Scale ball size with zoom
            );
            pRenderTarget->FillEllipse(ballEllipse, pBrush);
            
            // Draw BALL label
            DrawBallLabel(ballPixelX, ballPixelY, zoomFactor);
        }
        
        // Draw court labels (NET, LEFTY, RIGHTY)
        DrawCourtLabels(courtMargin, courtPixelWidth, courtTop, courtBottom, zoomFactor);
        
        // Draw title
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F titleRect = D2D1::RectF(10, 10, WINDOW_WIDTH - 10, 40);
        pRenderTarget->DrawTextW(
            L"Grass Court - Horizontal Shot",
            31,
            pTextFormat,
            titleRect,
            pBrush
        );
        
        // Draw telemetry
        if (simulationStarted && grassBall) {
            wchar_t telemetry[512];
            swprintf_s(telemetry, 
                L"Time: %.2fs | X: %.2fm | Y: %.2fm | Vx: %.2fm/s | Vy: %.2fm/s\nForce: %.0fN | Angle: %.0f° | Spin: %.0f RPM | Pace: %.0f%% | Bounces: %d",
                grassBall->time, grassBall->x, grassBall->y, grassBall->vx, grassBall->vy,
                horizontalForce, launchAngle, grassBall->spinRPM, visualPaceMultiplier * 100.0f, grassBall->bounceCount);
            
            D2D1_RECT_F telemetryRect = D2D1::RectF(10, 40, WINDOW_WIDTH - 10, 90);
            pRenderTarget->DrawTextW(
                telemetry,
                wcslen(telemetry),
                pSmallTextFormat,
                telemetryRect,
                pBrush
            );
        }
        
        // Draw instructions
        if (!simulationStarted) {
            wchar_t instructions[300];
            swprintf_s(instructions, 
                L"SPACE: Start | R: Reset | W/S: Angle (%.0f°) | A/D: Force (%.0fN)\n>/<: Spin (%.0f RPM) | +/-: Pace (%.0f%%)",
                launchAngle, horizontalForce, ballSpin, visualPaceMultiplier * 100.0f);
            
            D2D1_RECT_F instructRect = D2D1::RectF(10, WINDOW_HEIGHT - 30, WINDOW_WIDTH - 10, WINDOW_HEIGHT - 10);
            pRenderTarget->DrawTextW(
                instructions,
                wcslen(instructions),
                pSmallTextFormat,
                instructRect,
                pBrush
            );
        }
        
        // Draw air resistance combo box
        DrawComboBox();
    }
    
    void RenderHardCourt() {
        const float courtMargin = 50.0f;
        const float zoomFactor = 0.25f; // 4x wider court with 2x zoom out = 0.25 total
        const float courtPixelWidth = (WINDOW_WIDTH - 2 * courtMargin) * 4.0f * zoomFactor; // 4x width with zoom
        const float courtPixelHeight = 300.0f * zoomFactor;
        const float courtTop = 240.0f;
        const float courtBottom = courtTop + courtPixelHeight;
        
        // Draw hard court
        pBrush->SetColor(courts[2].color); // Hard court color (blue)
        D2D1_RECT_F courtRect = D2D1::RectF(
            courtMargin,
            courtTop,
            courtMargin + courtPixelWidth,
            courtBottom
        );
        pRenderTarget->FillRectangle(courtRect, pBrush);
        
        // Draw court outline
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRenderTarget->DrawRectangle(courtRect, pBrush, 2.0f);
        
        // Draw net in the middle
        float netX = courtMargin + courtPixelWidth / 2.0f;
        float netPixelHeight = NET_HEIGHT * 50.0f * zoomFactor; // Scale with zoom
        
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F netRect = D2D1::RectF(
            netX - 2.0f,
            courtBottom - netPixelHeight,
            netX + 2.0f,
            courtBottom
        );
        pRenderTarget->FillRectangle(netRect, pBrush);
        
        // Draw net top line
        pRenderTarget->DrawLine(
            D2D1::Point2F(netX - 10.0f, courtBottom - netPixelHeight),
            D2D1::Point2F(netX + 10.0f, courtBottom - netPixelHeight),
            pBrush, 2.0f
        );
        
        // Draw ball if simulation started
        if (simulationStarted && hardBall) {
            float ballPixelX = courtMargin + (hardBall->x / COURT_LENGTH) * courtPixelWidth;
            float ballPixelY = courtBottom - (hardBall->y * 50.0f * zoomFactor); // Scale with zoom
            
            pBrush->SetColor(courts[2].ballColor); // Yellow ball
            D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(
                D2D1::Point2F(ballPixelX, ballPixelY),
                10.0f * zoomFactor, 10.0f * zoomFactor // Scale ball size with zoom
            );
            pRenderTarget->FillEllipse(ballEllipse, pBrush);
            
            // Draw BALL label
            DrawBallLabel(ballPixelX, ballPixelY, zoomFactor);
        }
        
        // Draw court labels (NET, LEFTY, RIGHTY)
        DrawCourtLabels(courtMargin, courtPixelWidth, courtTop, courtBottom, zoomFactor);
        
        // Draw title
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F titleRect = D2D1::RectF(10, 10, WINDOW_WIDTH - 10, 40);
        pRenderTarget->DrawTextW(
            L"Hard Court - Horizontal Shot",
            30,
            pTextFormat,
            titleRect,
            pBrush
        );
        
        // Draw telemetry
        if (simulationStarted && hardBall) {
            wchar_t telemetry[512];
            swprintf_s(telemetry, 
                L"Time: %.2fs | X: %.2fm | Y: %.2fm | Vx: %.2fm/s | Vy: %.2fm/s\nForce: %.0fN | Angle: %.0f\u00b0 | Spin: %.0f RPM | Pace: %.0f%% | Bounces: %d",
                hardBall->time, hardBall->x, hardBall->y, hardBall->vx, hardBall->vy,
                horizontalForce, launchAngle, hardBall->spinRPM, visualPaceMultiplier * 100.0f, hardBall->bounceCount);
            
            D2D1_RECT_F telemetryRect = D2D1::RectF(10, 40, WINDOW_WIDTH - 10, 90);
            pRenderTarget->DrawTextW(
                telemetry,
                wcslen(telemetry),
                pSmallTextFormat,
                telemetryRect,
                pBrush
            );
        }
        
        // Draw instructions
        if (!simulationStarted) {
            wchar_t instructions[300];
            swprintf_s(instructions, 
                L"SPACE: Start | R: Reset | W/S: Angle (%.0f\u00b0) | A/D: Force (%.0fN)\n>/<: Spin (%.0f RPM) | +/-: Pace (%.0f%%)",
                launchAngle, horizontalForce, ballSpin, visualPaceMultiplier * 100.0f);
            
            D2D1_RECT_F instructRect = D2D1::RectF(10, WINDOW_HEIGHT - 30, WINDOW_WIDTH - 10, WINDOW_HEIGHT - 10);
            pRenderTarget->DrawTextW(
                instructions,
                wcslen(instructions),
                pSmallTextFormat,
                instructRect,
                pBrush
            );
        }
        
        // Draw air resistance combo box
        DrawComboBox();
    }
    
    void RenderLaverCourt() {
        const float courtMargin = 50.0f;
        const float zoomFactor = 0.25f; // 4x wider court with 2x zoom out = 0.25 total
        const float courtPixelWidth = (WINDOW_WIDTH - 2 * courtMargin) * 4.0f * zoomFactor; // 4x width with zoom
        const float courtPixelHeight = 300.0f * zoomFactor;
        const float courtTop = 240.0f;
        const float courtBottom = courtTop + courtPixelHeight;
        
        // Draw Laver Cup court
        pBrush->SetColor(courts[3].color); // Black court color
        D2D1_RECT_F courtRect = D2D1::RectF(
            courtMargin,
            courtTop,
            courtMargin + courtPixelWidth,
            courtBottom
        );
        pRenderTarget->FillRectangle(courtRect, pBrush);
        
        // Draw court outline
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRenderTarget->DrawRectangle(courtRect, pBrush, 2.0f);
        
        // Draw net in the middle
        float netX = courtMargin + courtPixelWidth / 2.0f;
        float netPixelHeight = NET_HEIGHT * 50.0f * zoomFactor; // Scale with zoom
        
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F netRect = D2D1::RectF(
            netX - 2.0f,
            courtBottom - netPixelHeight,
            netX + 2.0f,
            courtBottom
        );
        pRenderTarget->FillRectangle(netRect, pBrush);
        
        // Draw net top line
        pRenderTarget->DrawLine(
            D2D1::Point2F(netX - 10.0f, courtBottom - netPixelHeight),
            D2D1::Point2F(netX + 10.0f, courtBottom - netPixelHeight),
            pBrush, 2.0f
        );
        
        // Draw ball if simulation started
        if (simulationStarted && laverBall) {
            float ballPixelX = courtMargin + (laverBall->x / COURT_LENGTH) * courtPixelWidth;
            float ballPixelY = courtBottom - (laverBall->y * 50.0f * zoomFactor); // Scale with zoom
            
            pBrush->SetColor(courts[3].ballColor); // Yellow ball
            D2D1_ELLIPSE ballEllipse = D2D1::Ellipse(
                D2D1::Point2F(ballPixelX, ballPixelY),
                10.0f * zoomFactor, 10.0f * zoomFactor // Scale ball size with zoom
            );
            pRenderTarget->FillEllipse(ballEllipse, pBrush);
            
            // Draw BALL label
            DrawBallLabel(ballPixelX, ballPixelY, zoomFactor);
        }
        
        // Draw court labels (NET, LEFTY, RIGHTY)
        DrawCourtLabels(courtMargin, courtPixelWidth, courtTop, courtBottom, zoomFactor);
        
        // Draw title
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F titleRect = D2D1::RectF(10, 10, WINDOW_WIDTH - 10, 40);
        pRenderTarget->DrawTextW(
            L"Laver Cup - Horizontal Shot",
            30,
            pTextFormat,
            titleRect,
            pBrush
        );
        
        // Draw telemetry
        if (simulationStarted && laverBall) {
            wchar_t telemetry[512];
            swprintf_s(telemetry, 
                L"Time: %.2fs | X: %.2fm | Y: %.2fm | Vx: %.2fm/s | Vy: %.2fm/s\nForce: %.0fN | Angle: %.0f° | Spin: %.0f RPM | Pace: %.0f%% | Bounces: %d",
                laverBall->time, laverBall->x, laverBall->y, laverBall->vx, laverBall->vy,
                horizontalForce, launchAngle, laverBall->spinRPM, visualPaceMultiplier * 100.0f, laverBall->bounceCount);
            
            D2D1_RECT_F telemetryRect = D2D1::RectF(10, 40, WINDOW_WIDTH - 10, 90);
            pRenderTarget->DrawTextW(
                telemetry,
                wcslen(telemetry),
                pSmallTextFormat,
                telemetryRect,
                pBrush
            );
        }
        
        // Draw instructions
        if (!simulationStarted) {
            wchar_t instructions[300];
            swprintf_s(instructions, 
                L"SPACE: Start | R: Reset | W/S: Angle (%.0f°) | A/D: Force (%.0fN)\n>/<: Spin (%.0f RPM) | +/-: Pace (%.0f%%)",
                launchAngle, horizontalForce, ballSpin, visualPaceMultiplier * 100.0f);
            
            D2D1_RECT_F instructRect = D2D1::RectF(10, WINDOW_HEIGHT - 30, WINDOW_WIDTH - 10, WINDOW_HEIGHT - 10);
            pRenderTarget->DrawTextW(
                instructions,
                wcslen(instructions),
                pSmallTextFormat,
                instructRect,
                pBrush
            );
        }
        
        // Draw air resistance combo box
        DrawComboBox();
    }
    
    void DrawCourtLabels(float courtMargin, float courtPixelWidth, float courtTop, float courtBottom, float zoomFactor) {
        // Labels: NET, BALL, LEFTY, RIGHTY are defined but not rendered on screen
        // NET - white net in the center
        // LEFTY - launcher 20 pixels from left court edge (green icon)
        // RIGHTY - ball hitter at rightyPosition (white stick 2.5x NET height)
        // BALL - tennis ball
        
        // Draw LEFTY icon (small rectangle representing launcher) - 20 pixels from left edge
        float netX = courtMargin + courtPixelWidth / 2.0f;
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
        D2D1_RECT_F leftyIcon = D2D1::RectF(
            courtMargin + 20.0f,
            courtBottom - 10.0f * zoomFactor,
            courtMargin + 25.0f,
            courtBottom
        );
        pRenderTarget->FillRectangle(leftyIcon, pBrush);
        
        // Draw RIGHTY icon (white stick 2.5x NET height)
        float rightyHeight = NET_HEIGHT * 2.5f * 50.0f * zoomFactor; // 2.5x NET height, scaled
        float rightyPixelX = courtMargin + (rightyPosition / COURT_LENGTH) * courtPixelWidth;
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        D2D1_RECT_F rightyIcon = D2D1::RectF(
            rightyPixelX - 1.0f,
            courtBottom - rightyHeight,
            rightyPixelX + 1.0f,
            courtBottom
        );
        pRenderTarget->FillRectangle(rightyIcon, pBrush);
    }
    
    void DrawBallLabel(float ballPixelX, float ballPixelY, float zoomFactor) {
        // BALL label is defined but not rendered on screen
    }
    
    void DrawComboBox() {
        // Draw combo box background
        pBrush->SetColor(D2D1::ColorF(0.2f, 0.2f, 0.2f));
        pRenderTarget->FillRectangle(comboBoxRect, pBrush);
        
        // Draw combo box border
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRenderTarget->DrawRectangle(comboBoxRect, pBrush, 2.0f);
        
        // Draw current selection
        wchar_t labelText[128];
        swprintf_s(labelText, L"Air: %s", airModes[airResistanceMode].name);
        
        D2D1_RECT_F textRect = D2D1::RectF(
            comboBoxRect.left + 5,
            comboBoxRect.top + 3,
            comboBoxRect.right - 5,
            comboBoxRect.bottom - 3
        );
        
        pRenderTarget->DrawTextW(
            labelText,
            wcslen(labelText),
            pSmallTextFormat,
            textRect,
            pBrush
        );
        
        // Draw dropdown arrow
        float arrowX = comboBoxRect.right - 15;
        float arrowY = (comboBoxRect.top + comboBoxRect.bottom) / 2;
        
        D2D1_POINT_2F arrow1 = D2D1::Point2F(arrowX - 4, arrowY - 2);
        D2D1_POINT_2F arrow2 = D2D1::Point2F(arrowX, arrowY + 2);
        D2D1_POINT_2F arrow3 = D2D1::Point2F(arrowX + 4, arrowY - 2);
        
        pRenderTarget->DrawLine(arrow1, arrow2, pBrush, 1.5f);
        pRenderTarget->DrawLine(arrow2, arrow3, pBrush, 1.5f);
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
            if (currentScreen == MODE_ALL) {
                for (int i = 0; i < 4; i++) {
                    balls[i]->reset();
                }
            } else if (currentScreen == MODE_CLAY) {
                clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            } else if (currentScreen == MODE_GRASS) {
                grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            } else if (currentScreen == MODE_HARD) {
                hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            } else if (currentScreen == MODE_LAVER) {
                laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            }
        } else if (wParam == 'C' || wParam == 'c') {
            currentScreen = MODE_CLAY;
            simulationStarted = false;
            simulationComplete = false;
            clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        } else if (wParam == 'G' || wParam == 'g') {
            currentScreen = MODE_GRASS;
            simulationStarted = false;
            simulationComplete = false;
            grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        } else if (wParam == 'H' || wParam == 'h') {
            currentScreen = MODE_HARD;
            simulationStarted = false;
            simulationComplete = false;
            hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        } else if (wParam == 'L' || wParam == 'l') {
            currentScreen = MODE_LAVER;
            simulationStarted = false;
            simulationComplete = false;
            laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
        } else if (wParam == 'A' || wParam == 'a') {
            if (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER) {
                // A key - decrease force in Clay/Grass/Hard/Laver courts
                horizontalForce = max(MIN_HORIZONTAL_FORCE, horizontalForce - 10.0f);
                if (!simulationStarted) {
                    if (currentScreen == MODE_CLAY) {
                        clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_GRASS) {
                        grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_HARD) {
                        hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else {
                        laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    }
                }
            }
        } else if (wParam == VK_BACK) {
            // Backspace - return to all courts view
            currentScreen = MODE_ALL;
            simulationStarted = false;
            simulationComplete = false;
            for (int i = 0; i < 4; i++) {
                balls[i]->reset();
            }
        } else if (wParam == VK_UP && (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER)) {
            horizontalForce = min(MAX_HORIZONTAL_FORCE, horizontalForce + 10.0f);
            if (!simulationStarted) {
                if (currentScreen == MODE_CLAY) {
                    clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_GRASS) {
                    grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_HARD) {
                    hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else {
                    laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                }
            }
        } else if (wParam == VK_DOWN && (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER)) {
            horizontalForce = max(MIN_HORIZONTAL_FORCE, horizontalForce - 10.0f);
            if (!simulationStarted) {
                if (currentScreen == MODE_CLAY) {
                    clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_GRASS) {
                    grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_HARD) {
                    hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else {
                    laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                }
            }
        } else if ((wParam == 'W' || wParam == 'w') && (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER)) {
            // W key - increase angle
            launchAngle = min(MAX_ANGLE, launchAngle + ANGLE_STEP);
            if (!simulationStarted) {
                if (currentScreen == MODE_CLAY) {
                    clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_GRASS) {
                    grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_HARD) {
                    hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else {
                    laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                }
            }
        } else if ((wParam == 'S' || wParam == 's') && (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER)) {
            // S key - decrease angle
            launchAngle = max(MIN_ANGLE, launchAngle - ANGLE_STEP);
            if (!simulationStarted) {
                if (currentScreen == MODE_CLAY) {
                    clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_GRASS) {
                    grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_HARD) {
                    hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else {
                    laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                }
            }
        } else if ((wParam == 'D' || wParam == 'd') && (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER)) {
            // D key - increase force
            horizontalForce = min(MAX_HORIZONTAL_FORCE, horizontalForce + 10.0f);
            if (!simulationStarted) {
                if (currentScreen == MODE_CLAY) {
                    clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_GRASS) {
                    grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_HARD) {
                    hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else {
                    laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                }
            }
        } else if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == VK_OEM_PLUS || wParam == VK_ADD)) {
            // Ctrl + + for topspin
            if (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER) {
                ballSpin = min(MAX_SPIN, ballSpin + SPIN_STEP);
                if (!simulationStarted) {
                    if (currentScreen == MODE_CLAY) {
                        clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_GRASS) {
                        grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_HARD) {
                        hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else {
                        laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    }
                }
            }
        } else if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT)) {
            // Ctrl + - for backspin
            if (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER) {
                ballSpin = max(MIN_SPIN, ballSpin - SPIN_STEP);
                if (!simulationStarted) {
                    if (currentScreen == MODE_CLAY) {
                        clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_GRASS) {
                        grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_HARD) {
                        hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else {
                        laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    }
                }
            }
        } else if ((wParam == VK_OEM_PERIOD || wParam == '.') && (GetKeyState(VK_SHIFT) & 0x8000)) {
            // > key (Shift + .) for topspin
            if (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER) {
                ballSpin = min(MAX_SPIN, ballSpin + SPIN_STEP);
                if (!simulationStarted) {
                    if (currentScreen == MODE_CLAY) {
                        clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_GRASS) {
                        grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_HARD) {
                        hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else {
                        laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    }
                }
            }
        } else if ((wParam == VK_OEM_COMMA || wParam == ',') && (GetKeyState(VK_SHIFT) & 0x8000)) {
            // < key (Shift + ,) for backspin
            if (currentScreen == MODE_CLAY || currentScreen == MODE_GRASS || currentScreen == MODE_HARD || currentScreen == MODE_LAVER) {
                ballSpin = max(MIN_SPIN, ballSpin - SPIN_STEP);
                if (!simulationStarted) {
                    if (currentScreen == MODE_CLAY) {
                        clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_GRASS) {
                        grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else if (currentScreen == MODE_HARD) {
                        hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    } else {
                        laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                    }
                }
            }
        } else if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
            // + key (both regular and numpad) - visual pace
            visualPaceMultiplier = min(10.0f, visualPaceMultiplier * 1.1f);
        } else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
            // - key (both regular and numpad) - visual pace
            visualPaceMultiplier = max(0.1f, visualPaceMultiplier / 1.1f);
        }
    }
    
    void OnMouseClick(int x, int y) {
        if (currentScreen != MODE_CLAY && currentScreen != MODE_GRASS && currentScreen != MODE_HARD && currentScreen != MODE_LAVER) return;
        
        // Check if click is inside combo box
        if (x >= comboBoxRect.left && x <= comboBoxRect.right &&
            y >= comboBoxRect.top && y <= comboBoxRect.bottom) {
            // Cycle through air resistance modes
            airResistanceMode = (AirResistanceMode)((airResistanceMode + 1) % 4);
            
            // Update ball if not running simulation
            if (!simulationStarted) {
                if (currentScreen == MODE_CLAY) {
                    clayBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_GRASS) {
                    grassBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_HARD) {
                    hardBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                } else if (currentScreen == MODE_LAVER) {
                    laverBall->setAirResistance(airModes[airResistanceMode].coefficient);
                    laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
                }
            }
        }
    }
    
    void OnMouseWheel(int delta) {
        if (currentScreen != MODE_CLAY && currentScreen != MODE_GRASS && currentScreen != MODE_HARD && currentScreen != MODE_LAVER) return;
        
        // Positive delta = scroll up, negative = scroll down
        if (delta > 0) {
            launchAngle = min(MAX_ANGLE, launchAngle + ANGLE_STEP);
        } else {
            launchAngle = max(MIN_ANGLE, launchAngle - ANGLE_STEP);
        }
        
        // Update ball if not running simulation
        if (!simulationStarted) {
            if (currentScreen == MODE_CLAY) {
                clayBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            } else if (currentScreen == MODE_GRASS) {
                grassBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            } else if (currentScreen == MODE_HARD) {
                hardBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            } else if (currentScreen == MODE_LAVER) {
                laverBall->resetForHorizontalShot(horizontalForce, launchAngle, ballSpin);
            }
        }
    }
    
    bool CheckRightyCollision(TennisBall* ball) {
        if (!ball->isActive || simulationPaused) return false;
        
        const float RIGHTY_RADIUS = 0.05f; // 5cm radius for collision detection
        const float RIGHTY_HEIGHT = NET_HEIGHT * 2.5f; // Height of RIGHTY stick
        
        // Check if ball is in RIGHTY's horizontal range
        float distX = fabs(ball->x - rightyPosition);
        
        // Check if ball is within RIGHTY's height range and horizontal range
        if (distX <= (BALL_RADIUS + RIGHTY_RADIUS) && 
            ball->y >= 0.0f && ball->y <= RIGHTY_HEIGHT) {
            return true;
        }
        
        return false;
    }
    
    void ApplyRightyHit(TennisBall* ball, float force, float angle, float spin) {
        // RIGHTY hits the ball back to the left
        // Calculate velocity from force (force range 10-600N mapped to velocity 5-30 m/s)
        float totalVelocity = (force / 600.0f) * 30.0f;
        totalVelocity = max(5.0f, totalVelocity);
        
        // Convert angle to radians and calculate velocity components
        // Negative X velocity since hitting back to the left
        float angleRad = angle * 3.14159265f / 180.0f;
        ball->vx = -totalVelocity * cos(angleRad); // Negative for leftward direction
        ball->vy = totalVelocity * sin(angleRad);
        
        // Apply spin
        ball->spinRPM = spin;
        
        // Reset ball position slightly away from RIGHTY to avoid re-collision
        ball->x = rightyPosition - 0.1f;
        
        simulationPaused = false;
        ballHitRighty = false;
    }
};


// Global variables
D2DApp* g_pApp = NULL;

// Dialog procedure for RIGHTY hit
INT_PTR CALLBACK RightyHitDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static RightyHitParams* pParams = nullptr;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            pParams = (RightyHitParams*)lParam;
            
            // Center the dialog on parent window
            HWND hwndParent = GetParent(hwndDlg);
            RECT rcParent, rcDlg;
            GetWindowRect(hwndParent, &rcParent);
            GetWindowRect(hwndDlg, &rcDlg);
            int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
            int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
            SetWindowPos(hwndDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            // Set default values
            SetDlgItemInt(hwndDlg, 101, (UINT)pParams->force, FALSE);
            SetDlgItemInt(hwndDlg, 102, (UINT)pParams->angle, FALSE);
            SetDlgItemInt(hwndDlg, 103, (INT)pParams->spin, TRUE);
            
            // Set focus to first edit control
            SetFocus(GetDlgItem(hwndDlg, 101));
            return FALSE; // We set focus manually
        }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    // Get values from dialog
                    BOOL success;
                    
                    // Validate Force
                    pParams->force = (float)GetDlgItemInt(hwndDlg, 101, &success, FALSE);
                    if (!success) {
                        MessageBox(hwndDlg, L"Please enter a valid number for Force.", L"Invalid Input", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 101));
                        return TRUE;
                    }
                    if (pParams->force < 10.0f) {
                        MessageBox(hwndDlg, L"Force is too low. Minimum is 10 N.", L"Value Out of Range", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 101));
                        return TRUE;
                    }
                    if (pParams->force > 600.0f) {
                        MessageBox(hwndDlg, L"Force is too high. Maximum is 600 N.", L"Value Out of Range", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 101));
                        return TRUE;
                    }
                    
                    // Validate Angle
                    pParams->angle = (float)GetDlgItemInt(hwndDlg, 102, &success, FALSE);
                    if (!success) {
                        MessageBox(hwndDlg, L"Please enter a valid number for Angle.", L"Invalid Input", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 102));
                        return TRUE;
                    }
                    if (pParams->angle < 0.0f) {
                        MessageBox(hwndDlg, L"Angle is too low. Minimum is 0°.", L"Value Out of Range", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 102));
                        return TRUE;
                    }
                    if (pParams->angle > 75.0f) {
                        MessageBox(hwndDlg, L"Angle is too high. Maximum is 75°.", L"Value Out of Range", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 102));
                        return TRUE;
                    }
                    
                    // Validate Spin (allow negative values)
                    pParams->spin = (float)(INT)GetDlgItemInt(hwndDlg, 103, &success, TRUE); // TRUE allows signed
                    if (!success) {
                        MessageBox(hwndDlg, L"Please enter a valid number for Spin.", L"Invalid Input", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 103));
                        return TRUE;
                    }
                    if (pParams->spin < -3000.0f) {
                        MessageBox(hwndDlg, L"Spin is too low. Minimum is -3000 RPM (backspin).", L"Value Out of Range", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 103));
                        return TRUE;
                    }
                    if (pParams->spin > 9000.0f) {
                        MessageBox(hwndDlg, L"Spin is too high. Maximum is 9000 RPM (topspin).", L"Value Out of Range", MB_OK | MB_ICONWARNING);
                        SetFocus(GetDlgItem(hwndDlg, 103));
                        return TRUE;
                    }
                    
                    pParams->confirmed = true;
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }
                
                case IDCANCEL:
                    pParams->confirmed = false;
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

// Create dialog template in memory and show dialog
bool ShowRightyHitDialog(HWND hwndParent, RightyHitParams* params) {
    // Create dialog template in memory
    struct DialogTemplate {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD class_;
        WORD title;
        WORD pointsize;
        WCHAR font[13];
    };
    
    // Allocate memory for dialog template with controls
    const int TEMPLATE_SIZE = 2048;
    BYTE* pTemplate = new BYTE[TEMPLATE_SIZE];
    ZeroMemory(pTemplate, TEMPLATE_SIZE);
    
    DLGTEMPLATE* pDlg = (DLGTEMPLATE*)pTemplate;
    pDlg->style = DS_SETFONT | DS_MODALFRAME | DS_FIXEDSYS | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    pDlg->dwExtendedStyle = 0;
    pDlg->cdit = 9; // 3 labels + 3 edit boxes + 2 buttons + 1 title label
    pDlg->x = 0;
    pDlg->y = 0;
    pDlg->cx = 220;
    pDlg->cy = 140;
    
    WORD* pCurrent = (WORD*)(pDlg + 1);
    
    // Menu (none)
    *pCurrent++ = 0;
    // Class (default)
    *pCurrent++ = 0;
    // Title
    wcscpy_s((WCHAR*)pCurrent, 50, L"RIGHTY Hit Back");
    pCurrent += wcslen((WCHAR*)pCurrent) + 1;
    
    // Font
    *pCurrent++ = 8; // Font size
    wcscpy_s((WCHAR*)pCurrent, 20, L"MS Shell Dlg");
    pCurrent += wcslen((WCHAR*)pCurrent) + 1;
    
    // Align to DWORD boundary
    if ((ULONG_PTR)pCurrent % 4)
        pCurrent++;
    
    // Helper lambda to add control
    auto AddControl = [&](WORD x, WORD y, WORD cx, WORD cy, WORD id, DWORD style, const WCHAR* className, const WCHAR* text) {
        // Align to DWORD
        if ((ULONG_PTR)pCurrent % 4) pCurrent++;
        
        DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)pCurrent;
        pItem->style = style | WS_CHILD | WS_VISIBLE;
        pItem->dwExtendedStyle = 0;
        pItem->x = x;
        pItem->y = y;
        pItem->cx = cx;
        pItem->cy = cy;
        pItem->id = id;
        pCurrent = (WORD*)(pItem + 1);
        
        // Class
        if (wcscmp(className, L"STATIC") == 0) {
            *pCurrent++ = 0xFFFF;
            *pCurrent++ = 0x0082;
        } else if (wcscmp(className, L"EDIT") == 0) {
            *pCurrent++ = 0xFFFF;
            *pCurrent++ = 0x0081;
        } else if (wcscmp(className, L"BUTTON") == 0) {
            *pCurrent++ = 0xFFFF;
            *pCurrent++ = 0x0080;
        }
        
        // Text
        if (text && wcslen(text) > 0) {
            wcscpy_s((WCHAR*)pCurrent, 100, text);
            pCurrent += wcslen(text) + 1;
        } else {
            *pCurrent++ = 0;
        }
        
        // Creation data
        *pCurrent++ = 0;
    };
    
    // Add controls
    AddControl(10, 10, 200, 12, 1000, SS_LEFT, L"STATIC", L"Ball hit RIGHTY! Set return shot parameters:");
    
    AddControl(10, 30, 90, 10, -1, SS_LEFT, L"STATIC", L"Force (10-600 N):");
    AddControl(105, 28, 50, 12, 101, ES_NUMBER | WS_BORDER | WS_TABSTOP, L"EDIT", L"");
    
    AddControl(10, 50, 90, 10, -1, SS_LEFT, L"STATIC", L"Angle (0-75°):");
    AddControl(105, 48, 50, 12, 102, ES_NUMBER | WS_BORDER | WS_TABSTOP, L"EDIT", L"");
    
    AddControl(10, 70, 90, 10, -1, SS_LEFT, L"STATIC", L"Spin (-3000-9000):");
    AddControl(105, 68, 50, 12, 103, WS_BORDER | WS_TABSTOP, L"EDIT", L""); // Removed ES_NUMBER to allow negative
    
    AddControl(40, 100, 60, 14, IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP, L"BUTTON", L"Hit Back");
    AddControl(120, 100, 60, 14, IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, L"BUTTON", L"Bounce");
    
    // Show dialog
    INT_PTR result = DialogBoxIndirectParam(GetModuleHandle(NULL), pDlg, hwndParent, RightyHitDialogProc, (LPARAM)params);
    
    delete[] pTemplate;
    
    return (result == IDOK && params->confirmed);
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            g_pApp = new D2DApp();
            if (g_pApp) {
                g_pApp->Initialize(hwnd);
            }
            SetTimer(hwnd, 1, 8, NULL); // ~120 FPS (1000ms/120 = 8.33ms)
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
            
        case WM_LBUTTONDOWN:
            if (g_pApp) {
                int xPos = LOWORD(lParam);
                int yPos = HIWORD(lParam);
                g_pApp->OnMouseClick(xPos, yPos);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
            
        case WM_MOUSEWHEEL:
            if (g_pApp) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                g_pApp->OnMouseWheel(delta);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Main entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // Seed random number generator
    srand((unsigned int)time(NULL));
    
    // Load settings from INI file
    LoadSettings();
    
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

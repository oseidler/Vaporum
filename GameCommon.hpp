#include "Engine/Core/EngineCommon.hpp"
#pragma once


//forward declarations
struct Vec2;
struct Rgba8;
class App;
class Renderer;
class InputSystem;
class AudioSystem;
class Window;
class RandomNumberGenerator;
class Game;

//external declarations
extern App* g_theApp;
extern Renderer* g_theRenderer;
extern InputSystem* g_theInput;
extern AudioSystem* g_theAudio;
extern Window* g_theWindow;
extern Game* g_theGame;

extern RandomNumberGenerator g_rng;

//gameplay constants
constexpr float WORLD_CAMERA_MIN_X = -1.0f;
constexpr float WORLD_CAMERA_MAX_X = 1.0f;
constexpr float WORLD_CAMERA_MIN_Y = -1.0f;
constexpr float WORLD_CAMERA_MAX_Y = 1.0f;
constexpr float SCREEN_CAMERA_SIZE_X = 1600.0f;
constexpr float SCREEN_CAMERA_SIZE_Y = 800.0f;
constexpr float SCREEN_CAMERA_CENTER_X = SCREEN_CAMERA_SIZE_X / 2.0f;
constexpr float SCREEN_CAMERA_CENTER_Y = SCREEN_CAMERA_SIZE_Y / 2.0f;

constexpr float DEBUG_LINE_WIDTH = 0.1f;

//debug drawing functions
void DebugDrawLine(Vec2 const& startPosition, Vec2 const& endPosition, float width, Rgba8 const& color);
void DebugDrawRing(Vec2 const& center, float radius, float width, Rgba8 const& color);
#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/Camera.hpp"

class App
{
//public member functions
public:
	//game flow functions
	void Startup(char* commandLineString);
	void Run();
	void Shutdown();
	void RunFrame();

	//app utilities
	bool IsQuitting() const { return m_isQuitting; }
	bool HandleQuitRequested();
	void RestartGame();

	//static app utilites
	static bool Event_Quit(EventArgs& args);
	static bool Event_LoadGameConfig(EventArgs& args);
	static bool Event_RemoteCommand(EventArgs& args);
	static bool Event_BurstTest(EventArgs& args);

//private member variables
private:
	//game flow functions
	void BeginFrame();
	void Update();
	void Render() const;
	void EndFrame();

//private member variables
private:
	bool m_isQuitting = false;
	Camera m_devConsoleCamera;
};

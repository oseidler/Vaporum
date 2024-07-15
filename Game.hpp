#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Vertex_PCUTBN.hpp"
#include "Engine/Input/Button.hpp"


//forward declarations
class GameCamera;
class Map;
class MapDefinition;
class Shader;
class Texture;
class BitmapFont;


enum class GameState
{
	GAMEPLAY,
	SPLASH,
	START,
	PAUSE,
	WAITING
};


class Game 
{
//public member functions
public:
	//game flow functions
	void Startup();
	void Update();
	void Render() const;
	void Shutdown();

	void UpdateGameplay();
	void RenderGameplay() const;
	void UpdateSplash();
	void RenderSplash() const;
	void UpdateStartMenu();
	void RenderStartMenu() const;
	void UpdatePauseMenu();
	void RenderPauseMenu() const;
	void UpdateWaiting();
	void RenderWaiting() const;

	void EnterGameplay();
	void ExitGameplay();
	void EnterSplash();
	void ExitSplash();
	void EnterStartMenu();
	void ExitStartMenu();
	void EnterPauseMenu();
	void ExitPauseMenu();
	void EnterWaiting();
	void ExitWaiting();

	//gameplay functions
	void InitializeMap(MapDefinition const* definition);
	void UpdateCommandBar();
	void RenderCommandBar() const;

	//command functions
	static bool LoadMapCommand(EventArgs& args);
	static bool StartNewGame(EventArgs& args);
	static bool ResumeGame(EventArgs& args);
	static bool ReturnToMainMenu(EventArgs& args);
	static bool RemotePlayerReady(EventArgs& args);
	static bool OtherPlayerQuit(EventArgs& args);

	//data loading functions
	void LoadDefinitions();
	void LoadAssets();

//public member variables
public:
	//game state variables
	bool	  m_isFinished = false;
	bool	  m_isReturningToMainMenu = false;
	GameState m_gameState = GameState::SPLASH;
	bool	  m_remotePlayerReady = false;
	bool	  m_remotePlayerQuit = false;
	int		  m_playerID = 0;

	//game clock
	Clock m_gameClock = Clock();

	//game objects
	GameCamera*	m_gameCamera = nullptr;
	Map*		m_currentMap = nullptr;

	//assets
	Texture*	m_logo = nullptr;
	Texture*	m_ground = nullptr;
	BitmapFont* m_font = nullptr;

	//UI buttons
	Button* m_localGameButton = nullptr;
	Button* m_quitButton = nullptr;
	Button* m_resumeGameButton = nullptr;
	Button* m_mainMenuButton = nullptr;
	int m_currentMainMenuButton = 0;
	int m_numMainMenuButtons = 2;
	int m_currentPauseMenuButton = 0;
	int m_numPauseMenuButtons = 2;
	Button* m_leftButton = nullptr;
	Button* m_rightButton = nullptr;
	Button* m_escButton = nullptr;
	Button* m_enterButton = nullptr;

	//camera variables
	Camera m_screenCamera;

	//lighting variables
	Vec3  m_sunDirection = Vec3(0.5f, 0.5f, -1.0f);
	float m_sunIntensity = 0.9f;
	float m_ambientIntensity = 0.2f;
};

#include "Game/Game.hpp"
#include "Game/Entity.hpp"
#include "Game/GameCamera.hpp"
#include "Game/App.hpp"
#include "Game/Model.hpp"
#include "Game/Map.hpp"
#include "Game/MapDefinition.hpp"
#include "Game/TileDefinition.hpp"
#include "Game/UnitDefinition.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/OBJLoader.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/CPUMesh.hpp"
#include "Engine/Renderer/GPUMesh.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/NetSystem.hpp"


//game flow functions
void Game::Startup()
{
	//load data
	LoadDefinitions();

	//load assets
	LoadAssets();
	
	//add entities to scene
	m_gameCamera = new GameCamera(this);
	g_theInput->SetCursorMode(false, false);

	//load camera settings from game config
	m_gameCamera->m_position = g_gameConfigBlackboard.GetValue("cameraStartPosition", Vec3(0.0f, 0.0f, 5.0f));
	m_gameCamera->m_orientation = g_gameConfigBlackboard.GetValue("cameraFixedAngle", EulerAngles(90.0f, 60.0f, 0.0f));
	m_gameCamera->m_panSpeed = g_gameConfigBlackboard.GetValue("cameraPanSpeed", 10.0f);
	m_gameCamera->m_elevateSpeed = g_gameConfigBlackboard.GetValue("cameraElevateSpeed", 10.0f);
	m_gameCamera->m_minHeight = g_gameConfigBlackboard.GetValue("cameraMinHeight", 1.0f);

	//set camera bounds
	m_gameCamera->m_camera.SetOrthoView(Vec2(WORLD_CAMERA_MIN_X, WORLD_CAMERA_MIN_Y), Vec2(WORLD_CAMERA_MAX_X, WORLD_CAMERA_MAX_Y));
	float fov = g_gameConfigBlackboard.GetValue("cameraFOVDegrees", 60.0f);
	float near = g_gameConfigBlackboard.GetValue("cameraNearClip", 0.01f);
	float far = g_gameConfigBlackboard.GetValue("cameraFarClip", 100.0f);
	float aspect = g_gameConfigBlackboard.GetValue("windowAspect", 2.0f);
	if (!g_gameConfigBlackboard.GetValue("windowFullscreen", false) && g_gameConfigBlackboard.GetValue("windowSize", IntVec2()) != IntVec2())
	{
		IntVec2 screenDimensions = g_gameConfigBlackboard.GetValue("windowSize", IntVec2());
		float width = static_cast<float>(screenDimensions.x);
		float height = static_cast<float>(screenDimensions.y);
		aspect = width / height;
	}
	m_gameCamera->m_camera.SetPerspectiveView(aspect, fov, near, far);
	m_gameCamera->m_camera.SetRenderBasis(Vec3(0.0f, 0.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

	m_screenCamera.SetOrthoView(Vec2(0.f, 0.f), Vec2(SCREEN_CAMERA_SIZE_X, SCREEN_CAMERA_SIZE_Y));

	//set settings based on net mode
	if (g_theNetSystem->m_config.m_mode == NetSystemMode::NONE)
	{
		m_remotePlayerReady = true;
	}
	else if (g_theNetSystem->m_config.m_mode == NetSystemMode::SERVER)
	{
		m_playerID = 1;
	}
	else if (g_theNetSystem->m_config.m_mode == NetSystemMode::CLIENT)
	{
		m_playerID = 2;
	}

	//load initial map
	std::string mapName = g_gameConfigBlackboard.GetValue("defaultMap", "Grid12x12");
	MapDefinition const* definition = MapDefinition::GetMapDefinition(mapName);
	InitializeMap(definition);

	//subscribe to events
	SubscribeEventCallbackFunction("LoadMap", LoadMapCommand);
	SubscribeEventCallbackFunction("RemotePlayerReady", RemotePlayerReady);
	SubscribeEventCallbackFunction("OtherPlayerQuit", OtherPlayerQuit);

	//create menu buttons
	Vec2 screenBounds = Vec2(SCREEN_CAMERA_SIZE_X, SCREEN_CAMERA_SIZE_Y);

	AABB2 localGameButtonBounds = AABB2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.29f, SCREEN_CAMERA_CENTER_Y, SCREEN_CAMERA_CENTER_X + SCREEN_CAMERA_SIZE_X * 0.05f, 
		SCREEN_CAMERA_CENTER_Y + SCREEN_CAMERA_SIZE_Y * 0.04f);
	m_localGameButton = new Button(g_theRenderer, g_theInput, localGameButtonBounds, screenBounds, nullptr, "New Game", AABB2(0.05f, 0.1f, 1.0f, 1.0f), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(), 
		Rgba8(0, 0, 0), Vec2(0.0f, 0.5f));
	m_localGameButton->SubscribeToEvent("StartLocalGame", Game::StartNewGame);
	m_localGameButton->m_overrideMouseSelection = true;
	m_localGameButton->m_isSelected = true;

	AABB2 quitGameButtonBounds = AABB2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.29f, SCREEN_CAMERA_CENTER_Y - SCREEN_CAMERA_SIZE_Y * 0.04f, 
		SCREEN_CAMERA_CENTER_X + SCREEN_CAMERA_SIZE_X * 0.05f, SCREEN_CAMERA_CENTER_Y);
	m_quitButton = new Button(g_theRenderer, g_theInput, quitGameButtonBounds, screenBounds, nullptr, "Quit", AABB2(0.05f, 0.1f, 1.0f, 0.9f), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(), 
		Rgba8(0, 0, 0), Vec2(0.0f, 0.5f));
	m_quitButton->SubscribeToEvent("quit", App::Event_Quit);
	m_quitButton->m_overrideMouseSelection = true;

	m_resumeGameButton = new Button(g_theRenderer, g_theInput, localGameButtonBounds, screenBounds, nullptr, "Resume Game", AABB2(0.05f, 0.1f, 1.0f, 1.0f), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(),
		Rgba8(0, 0, 0), Vec2(0.0f, 0.5f));
	m_resumeGameButton->SubscribeToEvent("ResumeGame", Game::ResumeGame);
	m_resumeGameButton->m_overrideMouseSelection = true;
	m_resumeGameButton->m_isSelected = true;

	m_mainMenuButton = new Button(g_theRenderer, g_theInput, quitGameButtonBounds, screenBounds, nullptr, "Main Menu", AABB2(0.05f, 0.1f, 1.0f, 0.9f), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(),
		Rgba8(0, 0, 0), Vec2(0.0f, 0.5f));
	m_mainMenuButton->SubscribeToEvent("ReturnToMainMenu", Game::ReturnToMainMenu);
	m_mainMenuButton->m_overrideMouseSelection = true;

	AABB2 leftButtonBounds = AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f);
	m_leftButton = new Button(g_theRenderer, g_theInput, leftButtonBounds, screenBounds, nullptr, "", AABB2(), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(0, 0, 0, 0));
	EventArgs leftArgs;
	leftArgs.SetValue("KeyCode", Stringf("%d", KEYCODE_LEFT));
	m_leftButton->AddEvent("KeyPressed", leftArgs);
	m_leftButton->AddFollowupEvent("KeyReleased", leftArgs);

	AABB2 rightButtonBounds = AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f);
	m_rightButton = new Button(g_theRenderer, g_theInput, rightButtonBounds, screenBounds, nullptr, "", AABB2(), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(0, 0, 0, 0));
	EventArgs rightArgs;
	rightArgs.SetValue("KeyCode", Stringf("%d", KEYCODE_RIGHT));
	m_rightButton->AddEvent("KeyPressed", rightArgs);
	m_rightButton->AddFollowupEvent("KeyReleased", rightArgs);

	AABB2 escButtonBounds = AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f);
	m_escButton = new Button(g_theRenderer, g_theInput, escButtonBounds, screenBounds, nullptr, "", AABB2(), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(0, 0, 0, 0));
	EventArgs escArgs;
	escArgs.SetValue("KeyCode", Stringf("%d", KEYCODE_ESC));
	m_escButton->AddEvent("KeyPressed", escArgs);
	m_escButton->AddFollowupEvent("KeyReleased", escArgs);

	AABB2 enterButtonBounds = AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f);
	m_enterButton = new Button(g_theRenderer, g_theInput, enterButtonBounds, screenBounds, nullptr, "", AABB2(), Rgba8(0, 0, 0, 0), Rgba8(), Rgba8(0, 0, 0, 0));
	EventArgs enterArgs;
	enterArgs.SetValue("KeyCode", Stringf("%d", KEYCODE_ENTER));
	m_enterButton->AddEvent("KeyPressed", enterArgs);
	m_enterButton->AddFollowupEvent("KeyReleased", enterArgs);
}


void Game::Update()
{
	switch (m_gameState)
	{
		case GameState::GAMEPLAY: UpdateGameplay(); break;
		case GameState::SPLASH:	  UpdateSplash(); break;
		case GameState::START:	  UpdateStartMenu(); break;
		case GameState::PAUSE:	  UpdatePauseMenu(); break;
		case GameState::WAITING:  UpdateWaiting(); break;
	}
}


void Game::Render() const
{
	switch (m_gameState)
	{
		case GameState::GAMEPLAY: RenderGameplay(); break;
		case GameState::SPLASH:	  RenderSplash(); break;
		case GameState::START:	  RenderStartMenu(); break;
		case GameState::PAUSE:	  RenderPauseMenu(); break;
		case GameState::WAITING:  RenderWaiting(); break;
	}
}


void Game::Shutdown()
{
	//delete all allocated pointers here
	if (m_gameCamera != nullptr)
	{
		delete m_gameCamera;
	}
	if (m_currentMap != nullptr)
	{
		delete m_currentMap;
	}
	if (m_localGameButton != nullptr)
	{
		delete m_localGameButton;
	}
	if (m_quitButton != nullptr)
	{
		delete m_quitButton;
	}
	if (m_resumeGameButton != nullptr)
	{
		delete m_resumeGameButton;
	}
	if (m_mainMenuButton != nullptr)
	{
		delete m_mainMenuButton;
	}
	if (m_leftButton != nullptr)
	{
		delete m_leftButton;
	}
	if (m_rightButton != nullptr)
	{
		delete m_rightButton;
	}
	if (m_escButton != nullptr)
	{
		delete m_escButton;
	}
	if (m_enterButton != nullptr)
	{
		delete m_enterButton;
	}
}



void Game::UpdateGameplay()
{
	Clock& sysClock = Clock::GetSystemClock();
	std::string gameInfo = Stringf("Time: %.2f  FPS: %.1f  Time Scale: %.2f", sysClock.GetTotalSeconds(), 1.0f / sysClock.GetDeltaSeconds(), m_gameClock.GetTimeScale());
	DebugAddScreenText(gameInfo, Vec2(SCREEN_CAMERA_SIZE_X, SCREEN_CAMERA_SIZE_Y), 16.0f, Vec2(1.0f, 1.0f), 0.0f, Rgba8(), Rgba8());

	Vec3& pos = m_gameCamera->m_position;
	std::string posMessage = Stringf("Camera position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
	DebugAddMessage(posMessage, 0.0f);

	//reset camera when user presses O
	if (g_theInput->WasKeyJustPressed('O'))
	{
		m_gameCamera->m_position = g_gameConfigBlackboard.GetValue("cameraStartPosition", Vec3(0.0f, 0.0f, 5.0f));
		m_gameCamera->m_orientation = g_gameConfigBlackboard.GetValue("cameraFixedAngle", EulerAngles(90.0f, 60.0f, 0.0f));
	}

	if ((m_currentMap != nullptr && (m_currentMap->m_player1Units.size() == 0 || m_currentMap->m_player2Units.size() == 0)) || m_remotePlayerQuit)
	{
		if (g_theInput->WasKeyJustPressed(KEYCODE_ENTER) || g_theInput->WasKeyJustPressed(KEYCODE_LMB))
		{
			m_isReturningToMainMenu = true;
		}
		
		return;
	}

	//update game objects
	if (m_currentMap != nullptr && m_currentMap->m_playerState != PlayerState::READY)
	{
		UpdateCommandBar();
	}
	if (m_gameCamera != nullptr)
	{
		m_gameCamera->Update();
	}
	if (m_currentMap != nullptr)
	{
		m_currentMap->Update();
	}
}


void Game::RenderGameplay() const
{
	g_theRenderer->ClearScreen(Rgba8(50, 50, 50));	//clear screen to dark gray

	g_theRenderer->BeginCamera(m_gameCamera->m_camera);	//render game world with the world camera

	if (m_currentMap != nullptr)	//do map last to make sure it always renders on top
	{
		m_currentMap->Render();
	}

	g_theRenderer->EndCamera(m_gameCamera->m_camera);

	g_theRenderer->BindShader(nullptr);

	//debug world rendering
	DebugRenderWorld(m_gameCamera->m_camera);

	g_theRenderer->BeginCamera(m_screenCamera);	//render UI with the screen camera

	//screen camera rendering here
	if (m_currentMap != nullptr && m_currentMap->m_playerState == PlayerState::READY)
	{
		std::vector<Vertex_PCU> promptVerts;
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 305.0f, SCREEN_CAMERA_CENTER_Y - 155.0f, SCREEN_CAMERA_CENTER_X + 305.0f, SCREEN_CAMERA_CENTER_Y + 155.0f));
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 300.0f, SCREEN_CAMERA_CENTER_Y - 150.0f, SCREEN_CAMERA_CENTER_X + 300.0f, SCREEN_CAMERA_CENTER_Y + 150.0f), Rgba8(0, 0, 0));
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(promptVerts);

		std::string playerText = Stringf("Player %i's turn", m_currentMap->m_currentPlayerTurn);
		DebugAddScreenText(playerText, Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y + 100.0f), 30.0f, Vec2(0.5f, 0.5f), 0.0f);
		DebugAddScreenText("Press Enter or click to continue", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y - 100.0f), 15.0f, Vec2(0.5f, 0.5f), 0.0f);
	}
	else if (m_currentMap != nullptr && m_currentMap->m_playerState == PlayerState::ENDING_TURN)
	{
		std::vector<Vertex_PCU> promptVerts;
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 305.0f, SCREEN_CAMERA_CENTER_Y - 155.0f, SCREEN_CAMERA_CENTER_X + 305.0f, SCREEN_CAMERA_CENTER_Y + 155.0f));
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 300.0f, SCREEN_CAMERA_CENTER_Y - 150.0f, SCREEN_CAMERA_CENTER_X + 300.0f, SCREEN_CAMERA_CENTER_Y + 150.0f), Rgba8(0, 0, 0));
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(promptVerts);

		DebugAddScreenText("End turn?", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y + 100.0f), 40.0f, Vec2(0.5f, 0.5f), 0.0f);
		DebugAddScreenText("Press Enter again to end turn,\nESC to cancel", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y - 100.0f), 18.0f, Vec2(0.5f, 0.5f), 0.0f);
	}

	if (m_currentMap != nullptr && m_currentMap->m_player1Units.size() == 0)
	{
		std::vector<Vertex_PCU> promptVerts;
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 305.0f, SCREEN_CAMERA_CENTER_Y - 155.0f, SCREEN_CAMERA_CENTER_X + 305.0f, SCREEN_CAMERA_CENTER_Y + 155.0f));
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 300.0f, SCREEN_CAMERA_CENTER_Y - 150.0f, SCREEN_CAMERA_CENTER_X + 300.0f, SCREEN_CAMERA_CENTER_Y + 150.0f), Rgba8(0, 0, 0));
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(promptVerts);

		DebugAddScreenText("Player 2 Wins", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y + 100.0f), 30.0f, Vec2(0.5f, 0.5f), 0.0f);
		DebugAddScreenText("Press Enter or click to return\nto menu", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y - 100.0f), 15.0f, Vec2(0.5f, 0.5f), 0.0f);
	}
	else if (m_currentMap != nullptr && m_currentMap->m_player2Units.size() == 0)
	{
		std::vector<Vertex_PCU> promptVerts;
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 305.0f, SCREEN_CAMERA_CENTER_Y - 155.0f, SCREEN_CAMERA_CENTER_X + 305.0f, SCREEN_CAMERA_CENTER_Y + 155.0f));
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 300.0f, SCREEN_CAMERA_CENTER_Y - 150.0f, SCREEN_CAMERA_CENTER_X + 300.0f, SCREEN_CAMERA_CENTER_Y + 150.0f), Rgba8(0, 0, 0));
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(promptVerts);

		DebugAddScreenText("Player 1 Wins", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y + 100.0f), 30.0f, Vec2(0.5f, 0.5f), 0.0f);
		DebugAddScreenText("Press Enter or click to return\nto menu", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y - 100.0f), 15.0f, Vec2(0.5f, 0.5f), 0.0f);
	}
	else if (m_remotePlayerQuit)
	{
		std::vector<Vertex_PCU> promptVerts;
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 305.0f, SCREEN_CAMERA_CENTER_Y - 155.0f, SCREEN_CAMERA_CENTER_X + 305.0f, SCREEN_CAMERA_CENTER_Y + 155.0f));
		AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 300.0f, SCREEN_CAMERA_CENTER_Y - 150.0f, SCREEN_CAMERA_CENTER_X + 300.0f, SCREEN_CAMERA_CENTER_Y + 150.0f), Rgba8(0, 0, 0));
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(promptVerts);

		std::string playerWinText = Stringf("Player %i Wins", m_playerID);
		DebugAddScreenText(playerWinText, Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y + 100.0f), 30.0f, Vec2(0.5f, 0.5f), 0.0f);
		DebugAddScreenText("Press Enter or click to return\nto menu", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y - 100.0f), 15.0f, Vec2(0.5f, 0.5f), 0.0f);
	}

	if (m_currentMap != nullptr && m_currentMap->m_playerState != PlayerState::READY)
	{
		RenderCommandBar();
	}

	g_theRenderer->EndCamera(m_screenCamera);

	//debug screen rendering
	DebugRenderScreen(m_screenCamera);
}


void Game::UpdateSplash()
{
	if (g_theInput->WasKeyJustPressed(KEYCODE_ENTER) || g_theInput->WasKeyJustPressed(KEYCODE_LMB))
	{
		ExitSplash();
		EnterStartMenu();
	}
}


void Game::RenderSplash() const
{
	g_theRenderer->ClearScreen(Rgba8(0, 0, 0));

	g_theRenderer->BeginCamera(m_screenCamera);

	std::vector<Vertex_PCU> logoVerts;
	AddVertsForAABB2(logoVerts, AABB2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_Y - SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_X + SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_Y + SCREEN_CAMERA_SIZE_Y * 0.4f));
	g_theRenderer->BindTexture(m_logo);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(logoVerts);

	DebugAddScreenText("Vaporum", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_SIZE_Y * 0.9f), SCREEN_CAMERA_SIZE_Y * 0.08f, Vec2(0.5f, 0.5f), 0.0f);
	DebugAddScreenText("Press ENTER or click anywhere to start", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_SIZE_Y * 0.1f), SCREEN_CAMERA_SIZE_Y * 0.03f, Vec2(0.5f, 0.5f), 0.0f);

	g_theRenderer->EndCamera(m_screenCamera);

	DebugRenderScreen(m_screenCamera);
}


void Game::UpdateStartMenu()
{
	m_localGameButton->Update();
	m_quitButton->Update();

	if (g_theInput->WasKeyJustPressed(KEYCODE_UP))
	{
		m_currentMainMenuButton--;
		if (m_currentMainMenuButton < 0)
		{
			m_currentMainMenuButton = 0;
		}
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_DOWN))
	{
		m_currentMainMenuButton++;
		if (m_currentMainMenuButton >= m_numMainMenuButtons)
		{
			m_currentMainMenuButton = m_numMainMenuButtons - 1;
		}
	}

	if (m_localGameButton->IsMouseInsideBounds())
	{
		m_currentMainMenuButton = 0;
	}
	else if (m_quitButton->IsMouseInsideBounds())
	{
		m_currentMainMenuButton = 1;
	}

	switch (m_currentMainMenuButton)
	{
	case 0: m_localGameButton->m_isSelected = true; m_quitButton->m_isSelected = false; break;
	case 1: m_quitButton->m_isSelected = true; m_localGameButton->m_isSelected = false; break;
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_ENTER))
	{
		switch(m_currentMainMenuButton)
		{
		case 0: m_localGameButton->FireButtonEvent(); break;
		case 1: m_quitButton->FireButtonEvent(); break;
		}
	}
}


void Game::RenderStartMenu() const
{
	g_theRenderer->ClearScreen(Rgba8(0, 0, 0));

	g_theRenderer->BeginCamera(m_screenCamera);

	std::vector<Vertex_PCU> logoVerts;
	AddVertsForAABB2(logoVerts, AABB2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_Y - SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_X + SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_Y + SCREEN_CAMERA_SIZE_Y * 0.4f));
	g_theRenderer->BindTexture(m_logo);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(logoVerts);

	std::vector<Vertex_PCU> sideBarVerts;
	AddVertsForAABB2(sideBarVerts, AABB2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.3f, 0.0f, SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.29f, SCREEN_CAMERA_SIZE_Y));
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(sideBarVerts);

	std::vector<Vertex_PCU> textVerts;
	m_font->AddVertsForText2D(textVerts, Vec2(), 35.0f, "Main Menu");
	g_theRenderer->BindTexture(&m_font->GetTexture());
	TransformVertexArrayXY3D(static_cast<int>(textVerts.size()), textVerts.data(), 1.0f, 90.0f, Vec2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.30f, SCREEN_CAMERA_CENTER_Y - SCREEN_CAMERA_SIZE_Y * 0.2f));
	g_theRenderer->DrawVertexArray(textVerts);

	m_localGameButton->Render();
	m_quitButton->Render();

	g_theRenderer->EndCamera(m_screenCamera);

	DebugRenderScreen(m_screenCamera);
}


void Game::UpdatePauseMenu()
{
	m_resumeGameButton->Update();
	m_mainMenuButton->Update();

	if (g_theInput->WasKeyJustPressed(KEYCODE_UP))
	{
		m_currentPauseMenuButton--;
		if (m_currentPauseMenuButton < 0)
		{
			m_currentPauseMenuButton = 0;
		}
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_DOWN))
	{
		m_currentPauseMenuButton++;
		if (m_currentPauseMenuButton >= m_numPauseMenuButtons)
		{
			m_currentPauseMenuButton = m_numPauseMenuButtons - 1;
		}
	}

	if (m_resumeGameButton->IsMouseInsideBounds())
	{
		m_currentPauseMenuButton = 0;
	}
	else if (m_mainMenuButton->IsMouseInsideBounds())
	{
		m_currentPauseMenuButton = 1;
	}

	switch (m_currentPauseMenuButton)
	{
	case 0: m_resumeGameButton->m_isSelected = true; m_mainMenuButton->m_isSelected = false; break;
	case 1: m_mainMenuButton->m_isSelected = true; m_resumeGameButton->m_isSelected = false; break;
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_ENTER))
	{
		switch (m_currentPauseMenuButton)
		{
		case 0: m_resumeGameButton->FireButtonEvent(); break;
		case 1: m_mainMenuButton->FireButtonEvent(); break;
		}
	}
}


void Game::RenderPauseMenu() const
{
	g_theRenderer->ClearScreen(Rgba8(0, 0, 0));

	g_theRenderer->BeginCamera(m_screenCamera);

	std::vector<Vertex_PCU> logoVerts;
	AddVertsForAABB2(logoVerts, AABB2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_Y - SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_X + SCREEN_CAMERA_SIZE_Y * 0.4f, SCREEN_CAMERA_CENTER_Y + SCREEN_CAMERA_SIZE_Y * 0.4f));
	g_theRenderer->BindTexture(m_logo);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(logoVerts);

	std::vector<Vertex_PCU> sideBarVerts;
	AddVertsForAABB2(sideBarVerts, AABB2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.3f, 0.0f, SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.29f, SCREEN_CAMERA_SIZE_Y));
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(sideBarVerts);

	std::vector<Vertex_PCU> textVerts;
	m_font->AddVertsForText2D(textVerts, Vec2(), 35.0f, "Pause Menu");
	g_theRenderer->BindTexture(&m_font->GetTexture());
	TransformVertexArrayXY3D(static_cast<int>(textVerts.size()), textVerts.data(), 1.0f, 90.0f, Vec2(SCREEN_CAMERA_CENTER_X - SCREEN_CAMERA_SIZE_X * 0.30f, SCREEN_CAMERA_CENTER_Y - SCREEN_CAMERA_SIZE_Y * 0.2f));
	g_theRenderer->DrawVertexArray(textVerts);

	m_resumeGameButton->Render();
	m_mainMenuButton->Render();

	g_theRenderer->EndCamera(m_screenCamera);

	DebugRenderScreen(m_screenCamera);
}


void Game::UpdateWaiting()
{
	if (m_remotePlayerReady)
	{
		EnterGameplay();
		ExitWaiting();
	}
}


void Game::RenderWaiting() const
{
	g_theRenderer->ClearScreen(Rgba8(0, 0, 0));

	g_theRenderer->BeginCamera(m_screenCamera);

	std::vector<Vertex_PCU> promptVerts;
	AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 305.0f, SCREEN_CAMERA_CENTER_Y - 155.0f, SCREEN_CAMERA_CENTER_X + 305.0f, SCREEN_CAMERA_CENTER_Y + 155.0f));
	AddVertsForAABB2(promptVerts, AABB2(SCREEN_CAMERA_CENTER_X - 300.0f, SCREEN_CAMERA_CENTER_Y - 150.0f, SCREEN_CAMERA_CENTER_X + 300.0f, SCREEN_CAMERA_CENTER_Y + 150.0f), Rgba8(0, 0, 0));
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(promptVerts);

	DebugAddScreenText("Waiting for other player...", Vec2(SCREEN_CAMERA_CENTER_X, SCREEN_CAMERA_CENTER_Y), 15.0f, Vec2(0.5f, 0.5f), 0.0f);

	g_theRenderer->EndCamera(m_screenCamera);

	DebugRenderScreen(m_screenCamera);
}


void Game::EnterGameplay()
{
	m_gameState = GameState::GAMEPLAY;
}


void Game::ExitGameplay()
{
}


void Game::EnterSplash()
{
	m_gameState = GameState::SPLASH;
}


void Game::ExitSplash()
{
}


void Game::EnterStartMenu()
{
	m_gameState = GameState::START;
}


void Game::ExitStartMenu()
{
}


void Game::EnterPauseMenu()
{
	m_gameState = GameState::PAUSE;
}


void Game::ExitPauseMenu()
{
}


void Game::EnterWaiting()
{
	m_gameState = GameState::WAITING;
}


void Game::ExitWaiting()
{
}


//
//gameplay functions
//
void Game::InitializeMap(MapDefinition const* definition)
{
	//delete the old map
	if (m_currentMap != nullptr)
	{
		delete m_currentMap;
	}

	//create new map
	m_currentMap = new Map(definition);
}


void Game::UpdateCommandBar()
{
	if (m_leftButton != nullptr)
	{
		m_leftButton->Update();
	}
	if (m_rightButton != nullptr)
	{
		m_rightButton->Update();
	}
	if (m_escButton != nullptr)
	{
		m_escButton->Update();
	}
	if (m_enterButton != nullptr)
	{
		m_enterButton->Update();
	}
}


void Game::RenderCommandBar() const
{
	std::vector<Vertex_PCU> boxVerts;
	AddVertsForAABB2(boxVerts, AABB2(0.0f, 0.0f, SCREEN_CAMERA_SIZE_X, SCREEN_CAMERA_SIZE_Y * 0.03f), Rgba8());
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X - SCREEN_CAMERA_SIZE_X * 0.1667f, 0.0f, SCREEN_CAMERA_SIZE_X, SCREEN_CAMERA_SIZE_Y * 0.18f), Rgba8());
	AddVertsForAABB2(boxVerts, AABB2(1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 2.0f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 2.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 3.0f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 3.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 4.0f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 4.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 5.0f - 1.0f), Rgba8(0, 0, 0));
	AddVertsForAABB2(boxVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 6.0f - 1.0f), Rgba8(0, 0, 0));
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(boxVerts);

	std::vector<Vertex_PCU> textVerts;
	switch (m_currentMap->m_playerState)
	{
		case PlayerState::SELECTING:
		{
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Mouse] Select Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Left] Previous Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Right] Next Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[ESC] Pause", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Enter] End Turn", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			
			break;
		}
		case PlayerState::UNIT_SELECTED:
		{
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Mouse] Move", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Left] Previous Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Right] Next Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[ESC] Cancel", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Enter] End Turn", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);

			break;
		}
		case PlayerState::UNIT_MOVED:
		{
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Mouse] Confirm Move", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Left] Previous Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Right] Next Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[ESC] Cancel", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Enter] End Turn", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);

			break;
		}
		case PlayerState::UNIT_MOVE_CONFIRMED:
		{
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Mouse] Attack/Done", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Left] Previous Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Right] Next Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[ESC] Cancel", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Enter] End Turn", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);

			break;
		}
		case PlayerState::UNIT_ATTACKING:
		{
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Mouse] Confirm Attack", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Left] Previous Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Right] Next Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[ESC] Cancel", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Enter] End Turn", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);

			break;
		}
		case PlayerState::ENDING_TURN:
		{
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Mouse] Select Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Left] Previous Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 2.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Right] Next Unit", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 3.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[ESC] Cancel", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 4.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, "[Enter] Confirm End", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);

			break;
		}
	}

	m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 4.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 5.0f - 1.0f),
		SCREEN_CAMERA_SIZE_Y * 0.015f, "Attack: ", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
	m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 3.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 4.0f - 1.0f),
		SCREEN_CAMERA_SIZE_Y * 0.015f, "Defense: ", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
	m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 2.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 3.0f - 1.0f),
		SCREEN_CAMERA_SIZE_Y * 0.015f, "Range: ", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
	m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 2.0f - 1.0f),
		SCREEN_CAMERA_SIZE_Y * 0.015f, "Movement: ", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);
	m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 0.5f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
		SCREEN_CAMERA_SIZE_Y * 0.015f, "Health: ", Rgba8(), 1.0f, Vec2(0.0f, 0.5f), TextBoxMode::OVERRUN);

	if (m_currentMap != nullptr)
	{
		Unit* unit = m_currentMap->GetUnitAtCoords(m_currentMap->m_selectedTileCoords, 0);
		if (unit != nullptr)
		{
			UnitDefinition const* def = unit->m_definition;
			std::string unitName = def->m_name;
			std::string unitAttack = Stringf("%i", def->m_groundAttackDamage);
			std::string unitDefense = Stringf("%i", def->m_defense);
			std::string unitRange = Stringf("%i - %i", def->m_groundAttackRangeMin, def->m_groundAttackRangeMax);
			std::string unitMove = Stringf("%i", def->m_movementRange);
			std::string unitHealth = Stringf("%i / %i", unit->m_currentHealth, def->m_health);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 6.0f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, unitName, Rgba8(), 1.0f, Vec2(0.5f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 4.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 5.0f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, unitAttack, Rgba8(), 1.0f, Vec2(1.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 3.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 4.0f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, unitDefense, Rgba8(), 1.0f, Vec2(1.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 2.0f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 3.0f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, unitRange, Rgba8(), 1.0f, Vec2(1.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f + 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f * 2.0f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, unitMove, Rgba8(), 1.0f, Vec2(1.0f, 0.5f), TextBoxMode::OVERRUN);
			m_font->AddVertsForTextInBox2D(textVerts, AABB2(SCREEN_CAMERA_SIZE_X * 0.1667f * 5.0f + 1.0f, 1.0f, SCREEN_CAMERA_SIZE_X * 0.1667f * 6.0f - 1.0f, SCREEN_CAMERA_SIZE_Y * 0.03f - 1.0f),
				SCREEN_CAMERA_SIZE_Y * 0.015f, unitHealth, Rgba8(), 1.0f, Vec2(1.0f, 0.5f), TextBoxMode::OVERRUN);
		}
	}

	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(&m_font->GetTexture());
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(textVerts);
}


//
//command functions
//
bool Game::LoadMapCommand(EventArgs& args)
{
	if (g_theGame == nullptr) return true;
	
	std::string const& mapName = args.GetValue("Name", "invalid file path");
	MapDefinition const* definition = MapDefinition::GetMapDefinition(mapName);
	if (definition != nullptr)
	{
		g_theGame->InitializeMap(definition);

		return true;
	}
	
	g_theDevConsole->AddLine(DevConsole::COLOR_ERROR, "Could not find map!");
	return true;
}


bool Game::StartNewGame(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr) return true;

	g_theGame->ExitStartMenu();
	g_theGame->EnterWaiting();
	EventArgs remoteArgs;
	remoteArgs.SetValue("Command", "RemotePlayerReady");
	FireEvent("RemoteCommand", remoteArgs);

	return true;
}


bool Game::ResumeGame(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr) return true;
	
	g_theGame->ExitPauseMenu();
	g_theGame->EnterGameplay();

	return true;
}


bool Game::ReturnToMainMenu(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr) return true;
	
	g_theGame->m_isReturningToMainMenu = true;

	return true;
}


bool Game::RemotePlayerReady(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr) return true;

	g_theGame->m_remotePlayerReady = true;

	return true;
}


bool Game::OtherPlayerQuit(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr) return true;

	if (g_theGame->m_gameState == GameState::GAMEPLAY)
	{
		g_theGame->m_remotePlayerQuit = true;
	}

	return true;
}


//
//data loading functions
//
void Game::LoadDefinitions()
{
	if (MapDefinition::s_mapDefinitions.size() == 0)
	{
		MapDefinition::InitializeMapDefs();
	}
	if (TileDefinition::s_tileDefinitions.size() == 0)
	{
		TileDefinition::InitializeTileDefs();
	}
	if (UnitDefinition::s_unitDefinitions.size() == 0)
	{
		UnitDefinition::InitializeUnitDefs();
	}
}


void Game::LoadAssets()
{
	m_logo = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/Logo.png");
	m_ground = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/MoonSurface.png");

	m_font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/SquirrelFixedFont");
}

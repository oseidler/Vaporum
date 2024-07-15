#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Model.hpp"
#include "Game/UnitDefinition.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/NetSystem.hpp"
#include "Engine/Core/StringUtils.hpp"


App* g_theApp = nullptr;

Renderer*	 g_theRenderer = nullptr;
InputSystem* g_theInput = nullptr;
AudioSystem* g_theAudio = nullptr;
Window*		 g_theWindow = nullptr;

Game* g_theGame = nullptr;


//public game flow functions
void App::Startup(char* commandLineString)
{
	XmlDocument gameConfigXml;
	char const* filePath = "Data/GameConfig.xml";
	XmlError result = gameConfigXml.LoadFile(filePath);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open game config file!"));
	XmlElement* root = gameConfigXml.RootElement();
	g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*root);

	Strings commandLineSplit = SplitStringOnDelimiter(commandLineString, ' ');
	if (commandLineSplit.size() == 2)
	{
		Strings configArgs = SplitStringOnDelimiter(commandLineSplit[1], '=');
		if (configArgs.size() == 2)
		{
			XmlDocument batchGameConfigXml;
			std::string batchFilePath = configArgs[1];
			XmlError batchResult = batchGameConfigXml.LoadFile(batchFilePath.c_str());
			GUARANTEE_OR_DIE(batchResult == tinyxml2::XML_SUCCESS, Stringf("Failed to open game config file!"));
			XmlElement* batchRoot = batchGameConfigXml.RootElement();
			g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*batchRoot);
		}
	}

	EventSystemConfig eventSystemConfig;
	g_theEventSystem = new EventSystem(eventSystemConfig);
	
	InputSystemConfig inputSystemConfig;
	g_theInput = new InputSystem(inputSystemConfig);

	WindowConfig windowConfig;
	windowConfig.m_windowTitle = g_gameConfigBlackboard.GetValue("windowTile", "Vaporum");
	windowConfig.m_clientAspect = g_gameConfigBlackboard.GetValue("windowAspect", 2.0f);
	windowConfig.m_inputSystem = g_theInput;
	windowConfig.m_isFullscreen = g_gameConfigBlackboard.GetValue("windowFullscreen", true);
	windowConfig.m_windowPosition = g_gameConfigBlackboard.GetValue("windowPosition", IntVec2());
	windowConfig.m_windowSize = g_gameConfigBlackboard.GetValue("windowSize", IntVec2());
	g_theWindow = new Window(windowConfig);

	RendererConfig rendererConfig;
	rendererConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(rendererConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_renderer = g_theRenderer;
	devConsoleConfig.m_camera = &m_devConsoleCamera;
	g_theDevConsole = new DevConsole(devConsoleConfig);
	
	AudioSystemConfig audioSystemConfig;
	g_theAudio = new AudioSystem(audioSystemConfig);
	
	g_theEventSystem->Startup();
	g_theDevConsole->Startup();
	g_theInput->Startup();
	g_theWindow->Startup();
	g_theRenderer->Startup();
	g_theAudio->Startup();

	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MAJOR, "Controls: ");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " L:     Load Model");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " W/S:   Move Forward/Back");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " A/D:   Move Left/Right");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " Z/C:   Move Up/Down");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " Q/E:   Roll Left/Right");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " Mouse: Look Around");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " Shift: Sprint");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " O:		Reset Camera");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " ~:     Open Dev Console");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " ESC:   Exit Game");
	g_theDevConsole->AddLine(DevConsole::COLOR_INFO_MINOR, " Space: Start Game");

	//SubscribeEventCallbackFunction("LoadGameConfig", Event_LoadGameConfig);

	//g_theDevConsole->Execute(commandLineString);

	NetSystemConfig netSystemConfig;
	std::string modeString = g_gameConfigBlackboard.GetValue("netMode", "none");
	if (modeString == "Client")
	{
		netSystemConfig.m_mode = NetSystemMode::CLIENT;
	}
	else if (modeString == "Server")
	{
		netSystemConfig.m_mode = NetSystemMode::SERVER;
	}
	netSystemConfig.m_hostAddress = g_gameConfigBlackboard.GetValue("netHostAddress", "127.0.0.1:27015");
	netSystemConfig.m_sendBufferSize = g_gameConfigBlackboard.GetValue("netSendBufferSize", 1024);
	netSystemConfig.m_recvBufferSize = g_gameConfigBlackboard.GetValue("netRecvBufferSize", 1024);
	g_theNetSystem = new NetSystem(netSystemConfig);
	g_theNetSystem->Startup();

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;
	DebugRenderSystemStartup(debugRenderConfig);

	g_theGame = new Game();
	g_theGame->Startup();

	SubscribeEventCallbackFunction("quit", Event_Quit);
	
	SubscribeEventCallbackFunction("RemoteCommand", Event_RemoteCommand);
	SubscribeEventCallbackFunction("BurstTest", Event_BurstTest);

	m_devConsoleCamera.SetOrthoView(Vec2(0.f, 0.f), Vec2(SCREEN_CAMERA_SIZE_X, SCREEN_CAMERA_SIZE_Y));
}


void App::Run()
{
	while (!IsQuitting())
	{
		RunFrame();
	}
}


void App::Shutdown()
{
	//delete all models in unit definitions (gotta do it here so they last until the very end
	for (int defIndex = 0; defIndex < UnitDefinition::s_unitDefinitions.size(); defIndex++)
	{
		Model* model = UnitDefinition::s_unitDefinitions[defIndex].m_model;
		delete model;
	}
	
	g_theGame->Shutdown();
	delete g_theGame;
	g_theGame = nullptr;

	DebugRenderSystemShutdown();

	g_theNetSystem->Shutdown();
	delete g_theNetSystem;
	g_theNetSystem = nullptr;

	g_theAudio->Shutdown();
	delete g_theAudio;
	g_theAudio = nullptr;

	g_theRenderer->Shutdown();
	delete g_theRenderer;
	g_theRenderer = nullptr;

	g_theWindow->Shutdown();
	delete g_theWindow;
	g_theWindow = nullptr;

	g_theInput->Shutdown();
	delete g_theInput;
	g_theInput = nullptr;

	g_theDevConsole->Shutdown();
	delete g_theDevConsole;
	g_theDevConsole = nullptr;

	g_theEventSystem->Shutdown();
	delete g_theEventSystem;
	g_theEventSystem = nullptr;
}


void App::RunFrame()
{
	//tick the system clock
	Clock::TickSystemClock();

	//run through the four parts of the frame
	BeginFrame();
	Update();
	Render();
	EndFrame();
}


//
//public app utilities
//
bool App::HandleQuitRequested()
{
	m_isQuitting = true;
	return m_isQuitting;
}


//
//static app utilities
//
bool App::Event_Quit(EventArgs& args)
{
	UNUSED(args);

	if (g_theApp != nullptr)
	{
		g_theApp->HandleQuitRequested();
	}

	return true;
}


bool App::Event_LoadGameConfig(EventArgs& args)
{
	XmlDocument gameConfigXml;
	std::string filePath = args.GetValue("File", "Data/GameConfig.xml");
	XmlError result = gameConfigXml.LoadFile(filePath.c_str());
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open game config file!"));
	XmlElement* root = gameConfigXml.RootElement();
	g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*root);

	return true;
}


bool App::Event_RemoteCommand(EventArgs& args)
{
	std::string command = args.GetValue("Command", "");
	if (!command.empty())
	{
		TrimString(command, '\"');
		g_theNetSystem->m_sendQueue.emplace_back(command);
	}
	
	return true;
}


bool App::Event_BurstTest(EventArgs& args)
{
	UNUSED(args);

	for (int burstIndex = 1; burstIndex <= 20; burstIndex++)
	{
		std::string command = Stringf("Echo Message=%i", burstIndex);
		g_theNetSystem->m_sendQueue.emplace_back(command);
	}

	return true;
}


//
//private game flow functions
//
void App::BeginFrame()
{
	g_theEventSystem->BeginFrame();
	g_theDevConsole->BeginFrame();
	g_theInput->BeginFrame();
	g_theWindow->BeginFrame();
	g_theRenderer->BeginFrame();
	g_theAudio->BeginFrame();
	g_theNetSystem->BeginFrame();

	DebugRenderBeginFrame();
}


void App::Update()
{
	//quit or leave attract mode if q is pressed
	if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		if (g_theGame->m_gameState == GameState::SPLASH || g_theGame->m_gameState == GameState::START)
		{
			HandleQuitRequested();
		}
		else if (g_theGame->m_gameState == GameState::PAUSE)
		{
			EventArgs args;
			Game::ReturnToMainMenu(args);
		}
		//handle ESC in gameplay state in Game code
	}

	//recreate game if f8 is pressed
	if (g_theInput->WasKeyJustPressed(KEYCODE_F8))
	{
		RestartGame();
	}

	//set mouse state based on game
	/*if (!g_theDevConsole->IsOpen() && Window::GetWindowContext()->HasFocus())
	{
		g_theInput->SetCursorMode(true, true);
	}
	else
	{
		g_theInput->SetCursorMode(false, false);
	}*/

	//update the game
	g_theGame->Update();

	//go back to the start if the game finishes
	if (g_theGame->m_isFinished)
	{
		RestartGame();
	}
}


void App::Render() const
{	
	g_theGame->Render();

	//render dev console separately from and after rest of game
	g_theRenderer->BeginCamera(m_devConsoleCamera);
	g_theDevConsole->Render(AABB2(0.0f, 0.0f, SCREEN_CAMERA_SIZE_X * 0.9f, SCREEN_CAMERA_SIZE_Y * 0.9f));
	g_theRenderer->EndCamera(m_devConsoleCamera);
}


void App::EndFrame()
{
	if (g_theGame->m_isReturningToMainMenu)
	{
		EventArgs args;
		args.SetValue("Command", "OtherPlayerQuit");
		FireEvent("RemoteCommand", args);
		g_theApp->RestartGame();
		g_theGame->ExitSplash();
		g_theGame->EnterStartMenu();
	}
	
	g_theEventSystem->EndFrame();
	g_theDevConsole->EndFrame();
	g_theInput->EndFrame();
	g_theWindow->EndFrame();
	g_theRenderer->EndFrame();
	g_theAudio->EndFrame();
	g_theNetSystem->EndFrame();

	DebugRenderEndFrame();
}


//
//private app utilities
//
void App::RestartGame()
{
	//delete old game
	g_theGame->Shutdown();
	delete g_theGame;
	g_theGame = nullptr;

	//initialize new game
	g_theGame = new Game();
	g_theGame->Startup();
}

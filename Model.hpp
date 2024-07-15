#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"


//forward declarations
class CPUMesh;
class GPUMesh;
class Shader;


class Model
{
//public member functions
public:
	Model();
	~Model();
	
	//model creation and rendering
	bool ParseXMLFileForOBJ(std::string const& fileName);
	void RenderGPUMesh(Vec3 sunDirection, float sunIntensity, float ambientIntensity, Vec3 position, EulerAngles orientation, Rgba8 color) const;

//public member variables
public:
	CPUMesh* m_cpuMesh = nullptr;
	GPUMesh* m_gpuMesh = nullptr;
	Shader*  m_shader = nullptr;
};

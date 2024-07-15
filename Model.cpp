#include "Game/GameCommon.hpp"
#include "Game/Model.hpp"
#include "Engine/Renderer/CPUMesh.hpp"
#include "Engine/Renderer/GPUMesh.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Core/OBJLoader.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/XmlUtils.hpp"


//
//constructor and destructor
//
Model::Model()
{
	m_cpuMesh = new CPUMesh();
	m_gpuMesh = new GPUMesh();
}

Model::~Model()
{
	if (m_cpuMesh != nullptr)
	{
		delete m_cpuMesh;
	}
	if (m_gpuMesh != nullptr)
	{
		delete m_gpuMesh;
	}
}

//
//model creation and rendering
//
bool Model::ParseXMLFileForOBJ(std::string const& fileName)
{
	//parse xml for obj file name and fixup matrix
	XmlDocument modelXml;
	XmlError result = modelXml.LoadFile(fileName.c_str());
	if (result != tinyxml2::XML_SUCCESS)
	{
		ERROR_RECOVERABLE("Failed to open model xml file!");
		return false;
	}
	XmlElement* rootElement = modelXml.RootElement();
	if (rootElement == nullptr)
	{
		ERROR_RECOVERABLE("Failed to read model xml root element!");
		return false;
	}
	
	std::string const& objFilePath = ParseXmlAttribute(*rootElement, "path", "invalid path");
	std::string const& shaderName = ParseXmlAttribute(*rootElement, "shader", "invalid shader path");
	if (shaderName == "invalid shader path")
	{
		ERROR_RECOVERABLE("Couldn't find shader in xml!");
		return false;
	}
	m_shader = g_theRenderer->CreateShader(shaderName.c_str());

	XmlElement* transformElement = rootElement->FirstChildElement();
	Mat44 matrix = Mat44();
	Vec3 iBasis = ParseXmlAttribute(*transformElement, "x", Vec3());
	Vec3 jBasis = ParseXmlAttribute(*transformElement, "y", Vec3());
	Vec3 kBasis = ParseXmlAttribute(*transformElement, "z", Vec3());
	Vec3 translation = ParseXmlAttribute(*transformElement, "t", Vec3());
	matrix.SetIJKT3D(iBasis, jBasis, kBasis, translation);
	matrix.AppendScaleUniform3D(ParseXmlAttribute(*transformElement, "scale", 1.0f));

	//pass into obj loader along with vertex and index vectors from cpu mesh
	OBJLoader::LoadObjFile(objFilePath, matrix, m_cpuMesh->m_vertexes, m_cpuMesh->m_indexes);

	m_gpuMesh->m_vertexBuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	g_theRenderer->CopyCPUToGPU(m_cpuMesh->m_vertexes.data(), static_cast<int>(m_cpuMesh->m_vertexes.size()) * sizeof(Vertex_PCUTBN), m_gpuMesh->m_vertexBuffer);

	if (m_cpuMesh->m_indexes.size() > 0)
	{
		m_gpuMesh->m_indexBuffer = g_theRenderer->CreateIndexBuffer(sizeof(int));
		g_theRenderer->CopyCPUToGPU(m_cpuMesh->m_indexes.data(), static_cast<int>(m_cpuMesh->m_indexes.size()) * sizeof(int), m_gpuMesh->m_indexBuffer);
	}

	return true;
}


void Model::RenderGPUMesh(Vec3 sunDirection, float sunIntensity, float ambientIntensity, Vec3 position, EulerAngles orientation, Rgba8 color) const
{
	Mat44 modelMatrix = orientation.GetAsMatrix_XFwd_YLeft_ZUp();
	modelMatrix.AppendTranslation3D(position);
	
	g_theRenderer->BindShader(m_shader);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->SetModelConstants(modelMatrix, color);
	g_theRenderer->SetLightConstants(sunDirection, sunIntensity, ambientIntensity);

	if (m_cpuMesh->m_indexes.size() > 0)
	{
		g_theRenderer->DrawVertexBufferIndexed(m_gpuMesh->m_vertexBuffer, m_gpuMesh->m_indexBuffer, static_cast<int>(m_cpuMesh->m_indexes.size()));
	}
	else
	{
		g_theRenderer->DrawVertexBuffer(m_gpuMesh->m_vertexBuffer, static_cast<int>(m_cpuMesh->m_vertexes.size()));
	}
}

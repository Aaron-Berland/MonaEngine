#include "MeshManager.hpp"
#include <vector>
#include <stack>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../Core/Log.hpp"
#include "Mesh.hpp"
#include "../Core/RootDirectory.hpp"
#include <glad/glad.h>

namespace Mona {

	std::shared_ptr<Mesh> MeshManager::LoadMesh(PrimitiveType type) noexcept
	{
		switch (type)
		{
			case Mona::MeshManager::PrimitiveType::Plane:
			{
				auto it = m_meshMap.find("Plane");
				if (it != m_meshMap.end())
				{
					return it->second;
				}
				return LoadPlane();
				break;
			}
			case Mona::MeshManager::PrimitiveType::Cube:
			{
				auto it = m_meshMap.find("Cube");
				if (it != m_meshMap.end())
				{
					return it->second;
				}
				return LoadCube();
				break;
			}
			case Mona::MeshManager::PrimitiveType::Sphere:
			{
				auto it = m_meshMap.find("Sphere");
				if (it != m_meshMap.end())
				{
					return it->second;
				}
				return LoadSphere();
				break;
			}
			default:
			{
				auto it = m_meshMap.find("Sphere");
				if (it != m_meshMap.end())
				{
					return it->second;
				}
				return LoadSphere();
				break;
			}
		}
	}

	std::shared_ptr<Mesh> MeshManager::LoadMesh(const std::filesystem::path& filePath) noexcept {
		const std::string& stringPath = filePath.string();
		auto it = m_meshMap.find(stringPath);
		if (it != m_meshMap.end()) {
			return it->second;
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(stringPath, aiProcess_Triangulate | aiProcess_GenSmoothNormals);
		if (!scene) {
			MONA_LOG_ERROR("MeshManager Error: Failed to open file with path {0}", stringPath);
			MONA_LOG_INFO("Loading default model");
			return LoadCube();
		}
		std::vector<float> vertices;
		std::vector<unsigned int> faces;
		size_t numVertices = 0;
		size_t numFaces = 0;
		for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
			numVertices += scene->mMeshes[i]->mNumVertices;
			numFaces += scene->mMeshes[i]->mNumFaces;
		}

		vertices.reserve(numVertices * 6);
		faces.reserve(numFaces);


		std::stack<const aiNode*> sceneNodes;
		std::stack<aiMatrix4x4> sceneTransforms;
		sceneNodes.push(scene->mRootNode);
		sceneTransforms.push(scene->mRootNode->mTransformation);
		unsigned int offset = 0;
		while (!sceneNodes.empty())
		{
			const aiNode* currentNode = sceneNodes.top();
			const auto& currentTransform = sceneTransforms.top();
			auto currentInvTranspose = currentTransform;
			currentInvTranspose.Inverse().Transpose();
			sceneNodes.pop();
			sceneTransforms.pop();
			for (uint32_t j = 0; j < currentNode->mNumMeshes; j++) {
				const aiMesh* meshOBJ = scene->mMeshes[currentNode->mMeshes[j]];
				for (uint32_t i = 0; i < meshOBJ->mNumVertices; i++) {
					const aiVector3D position = currentTransform * meshOBJ->mVertices[i];
					const aiVector3D normal = currentInvTranspose * meshOBJ->mNormals[i];
					vertices.push_back(position.x);
					vertices.push_back(position.y);
					vertices.push_back(position.z);
					vertices.push_back(normal.x);
					vertices.push_back(normal.y);
					vertices.push_back(normal.z);
				}

				for (uint32_t i = 0; i < meshOBJ->mNumFaces; i++) {
					const aiFace& face = meshOBJ->mFaces[i];
					MONA_ASSERT(face.mNumIndices == 3, "Model Manager Error: Can load meshes with faces that have {0} vertices", face.mNumIndices);
					faces.push_back(face.mIndices[0] + offset);
					faces.push_back(face.mIndices[1] + offset);
					faces.push_back(face.mIndices[2] + offset);
				}
				offset += meshOBJ->mNumVertices;


			}
			
			for (uint32_t j = 0; j < currentNode->mNumChildren; j++) {
				sceneNodes.push(currentNode->mChildren[j]);
				sceneTransforms.push(currentTransform * currentNode->mChildren[j]->mTransformation);
			}
		}
		
		unsigned int modelVBO, modelIBO, modelVAO;
		glGenVertexArrays(1, &modelVAO);
		glBindVertexArray(modelVAO);

		glGenBuffers(1, &modelVBO);
		glGenBuffers(1, &modelIBO);
		glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
		glBufferData(GL_ARRAY_BUFFER, static_cast<unsigned int>(vertices.size()) * sizeof(float), vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelIBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<unsigned int>(faces.size()) * sizeof(unsigned int), faces.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		auto sharedPtr = std::make_shared<Mesh>(modelVAO, modelVBO, modelIBO, static_cast<uint32_t>(faces.size()));
		m_meshMap.insert({ stringPath, sharedPtr });
		return sharedPtr;

	}

	std::shared_ptr<Mesh> MeshManager::LoadSphere() noexcept {
		std::vector<float> vertices;
		std::vector<unsigned int> indices;
		unsigned int stackCount = 16;
		unsigned int sectorCount = 32;
		constexpr float PI = glm::pi<float>();
		float sectorStep = 2 * PI / sectorCount;
		float stackStep = PI / stackCount;
		float sectorAngle, stackAngle;
		float radius = 1.0f;

		float x, y, z, xy;
		for (unsigned int i = 0; i <= stackCount; i++)
		{
			stackAngle = PI / 2.0f - i * stackStep;
			xy = std::cos(stackAngle);             
			z = std::sin(stackAngle);
			for (unsigned int j = 0; j <= sectorCount; j++)
			{
				sectorAngle = sectorStep * j;
				x = xy * std::cos(sectorAngle);
				y = xy * std::sin(sectorAngle);
				vertices.push_back(radius * x);
				vertices.push_back(radius * y);
				vertices.push_back(radius * z);
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(z);
			}

		}

		unsigned int k1, k2;
		for (unsigned int i = 0; i < stackCount; ++i)
		{
			k1 = i * (sectorCount + 1);     // beginning of current stack
			k2 = k1 + sectorCount + 1;      // beginning of next stack

			for (unsigned int j = 0; j < sectorCount; ++j, ++k1, ++k2)
			{
				if (i != 0)
				{
					indices.push_back(k1);
					indices.push_back(k2);
					indices.push_back(k1 + 1);
				}

				if (i != (stackCount - 1))
				{
					indices.push_back(k1 + 1);
					indices.push_back(k2);
					indices.push_back(k2 + 1);
				}
			}
		}
		unsigned int sphereVBO, sphereIBO, sphereVAO;
		glGenVertexArrays(1, &sphereVAO);
		glBindVertexArray(sphereVAO);

		glGenBuffers(1, &sphereVBO);
		glGenBuffers(1, &sphereIBO);
		glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
		glBufferData(GL_ARRAY_BUFFER, static_cast<unsigned int>(vertices.size())*sizeof(float), vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereIBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<unsigned int>(indices.size()) * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		auto sharedPtr = std::make_shared<Mesh>(sphereVAO, sphereVBO, sphereIBO, static_cast<uint32_t>(indices.size()));
		m_meshMap.insert({ "Sphere", sharedPtr });
		return sharedPtr;
	}

	std::shared_ptr<Mesh> MeshManager::LoadCube() noexcept {
		float vertices[] = {
	   -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
		1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
	   -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,
	   -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,

	   -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
		1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
	   -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,
	   -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,

	   -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
	   -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
	   -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
	   -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,
	   -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f,
	   -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,

		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
		1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
		1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,
		1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,
		1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,

	   -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
		1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
		1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
	   -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f,
	   -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f,

	   -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
		1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
		1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
	   -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f,
	   -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f
		};
		unsigned int indices[] = {
			0,1,2,3,4,5,
			6,7,8,9,10,11,
			12,13,14,15,16,17,
			18,19,20,21,22,23,
			24,25,26,27,28,29,
			30,31,32,33,34,35
		};
		unsigned int cubeVBO, cubeIBO, cubeVAO;
		glGenVertexArrays(1, &cubeVAO); 
		glBindVertexArray(cubeVAO);

		glGenBuffers(1, &cubeVBO);
		glGenBuffers(1, &cubeIBO);
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		auto sharedPtr = std::make_shared<Mesh>(cubeVAO, cubeVBO, cubeIBO, 36);
		m_meshMap.insert({ "Cube", sharedPtr });
		return sharedPtr;
	
	}

	std::shared_ptr<Mesh> MeshManager::LoadPlane() noexcept {
		float planeVertices[] = {
		-1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
		1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
		1.0f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
		1.0f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
		-1.0f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
		-1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
		};

		unsigned int planeIndices[] =
		{
			0,1,2,3,4,5
		};

		unsigned int planeVBO, planeIBO, planeVAO;
		glGenVertexArrays(1, &planeVAO);
		glBindVertexArray(planeVAO);
		glGenBuffers(1, &planeVBO);
		glGenBuffers(1, &planeIBO);

		glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeIBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeIndices), planeIndices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		auto sharedPtr = std::make_shared<Mesh>(planeVAO, planeVBO, planeIBO, 6);
		m_meshMap.insert({ "Plane", sharedPtr });
		return sharedPtr;
	}
	void MeshManager::CleanUnusedMeshes() noexcept {
		for (auto i = m_meshMap.begin(), last = m_meshMap.end(); i != last;) {
			if (i->second.use_count() == 1) {
				i = m_meshMap.erase(i);
			}
			else {
				++i;
			}

		}
	}
	void MeshManager::ShutDown() noexcept {
		for (auto& entry : m_meshMap) {
			entry.second->ClearData();
		}
		m_meshMap.clear();
	}

}
#include "Scene.hpp"
#pragma comment (lib, "assimp-vc145-mt.lib")
#include <Windows.h>

Scene::Scene(std::string path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder);

    uint32_t imageArrayOffset = 0;
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        aiMaterial* processedMaterial = scene->mMaterials[i];
        aiString str;
        aiReturn ret = processedMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &str);
        if (ret != aiReturn_SUCCESS)
        {
            materialTextureIdx.push_back(0);
        }
        else
        {
            materialTextureIdx.push_back(imageArrayOffset);
            imageArrayOffset++;
        }

    }
    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        if (scene->mMeshes[i]->mPrimitiveTypes != aiPrimitiveType_TRIANGLE)
        {
            OutputDebugString(L"Unsupported mesh type");
            exit(-1);
        }
        vertexCount += scene->mMeshes[i]->mNumVertices;
        indexCount += scene->mMeshes[i]->mNumFaces * 3;
    }

    vertices = new Vec3[vertexCount];
    normals = new Vec3[vertexCount];
    texCoords = new Vec2[vertexCount];
	indecies = new uint32_t[indexCount];    
	size_t vecOffest = 0;
    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        for (size_t j = 0; j < scene->mMeshes[i]->mNumVertices; j++)
        {

            vertices[vecOffest].x = scene->mMeshes[i]->mVertices[j].y;
            vertices[vecOffest].y = scene->mMeshes[i]->mVertices[j].z;
            vertices[vecOffest].z = scene->mMeshes[i]->mVertices[j].x;

            normals[vecOffest].x = scene->mMeshes[i]->mNormals[j].x;
            normals[vecOffest].y = scene->mMeshes[i]->mNormals[j].y;
            normals[vecOffest].z = scene->mMeshes[i]->mNormals[j].z;

            texCoords[vecOffest].x = scene->mMeshes[i]->mTextureCoords[0][j].x;
            texCoords[vecOffest].y = scene->mMeshes[i]->mTextureCoords[0][j].y;
           
            vecOffest++;
        }

        for (size_t j = 0; j < scene->mMeshes[i]->mNumFaces; j++)
        {
            indecies[j * 3 + 0] = scene->mMeshes[i]->mFaces[j].mIndices[0];
            indecies[j * 3 + 1] = scene->mMeshes[i]->mFaces[j].mIndices[1];
            indecies[j * 3 + 2] = scene->mMeshes[i]->mFaces[j].mIndices[2];
            
        }

    }

}

#include "Scene.hpp"
#pragma comment (lib, "assimp-vc145-mt.lib")
#include <Windows.h>

Scene::Scene(std::string path)
    :
    uboOffset(0)
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

    vertices.resize(vertexCount);
    normals.resize(vertexCount);
    texCoords.resize(vertexCount);
	indecies.resize(indexCount);
	size_t vecOffest = 0;
    size_t idxOffset = 0;
    for (size_t i = 0; i < scene->mNumMeshes; i++)
    {
        for (size_t j = 0; j < scene->mMeshes[i]->mNumVertices; j++)
        {

            vertices.at(vecOffest + j).x = scene->mMeshes[i]->mVertices[j].y;
            vertices.at(vecOffest + j).y = scene->mMeshes[i]->mVertices[j].z;
            vertices.at(vecOffest + j).z = scene->mMeshes[i]->mVertices[j].x;

            normals.at(vecOffest + j).x = scene->mMeshes[i]->mNormals[j].x;
            normals.at(vecOffest + j).y = scene->mMeshes[i]->mNormals[j].y;
            normals.at(vecOffest + j).z = scene->mMeshes[i]->mNormals[j].z;

            texCoords.at(vecOffest + j).x = scene->mMeshes[i]->mTextureCoords[0][j].x;
            texCoords.at(vecOffest + j).y = scene->mMeshes[i]->mTextureCoords[0][j].y;
           
        }

        for (size_t j = 0; j < scene->mMeshes[i]->mNumFaces; j++)
        {
            indecies.at((idxOffset + j) * 3 + 0) = scene->mMeshes[i]->mFaces[j].mIndices[0];
            indecies.at((idxOffset + j) * 3 + 1) = scene->mMeshes[i]->mFaces[j].mIndices[1];
            indecies.at((idxOffset + j) * 3 + 2) = scene->mMeshes[i]->mFaces[j].mIndices[2];
        }

        sceneGeometry.vbOffset.push_back(vecOffest);
        sceneGeometry.ibOffset.push_back(idxOffset * 3);
        sceneGeometry.indexCount.push_back(scene->mMeshes[i]->mNumFaces * 3);
        sceneGeometry.materialIndex.push_back(scene->mMeshes[i]->mMaterialIndex);

        vecOffest += scene->mMeshes[i]->mNumVertices;
        idxOffset += scene->mMeshes[i]->mNumFaces;
    }
    Eigen::Matrix4f init = Eigen::Matrix4f::Zero();
    for (int i = 0; i < 4; i++) { init(i, i) = 1;}
    parseObjectTree(scene->mRootNode, init);
}

void Scene::parseObjectTree(
    aiNode* node, 
    const Eigen::Matrix4f& transform)
{
    Eigen::Matrix4f localTransform;
    memcpy(localTransform.data(), &node->mTransformation, sizeof(float) * 4 * 4);
    // assimp stores transforms in row major
    localTransform.transposeInPlace();
    localTransform = localTransform * transform;
    if (node->mNumMeshes != 0)
    {
        RenderItem obj = {};
        obj.transformation = localTransform;
        obj.meshIdx.resize(node->mNumMeshes);
        for (size_t i = 0; i < node->mNumMeshes; i++)
        {
            obj.meshIdx[i] = node->mMeshes[i];
        }
        obj.index = Eigen::Vector4i::Zero();
        obj.name = node->mName.C_Str();
        obj.uboOffset = uboOffset;
        uboOffset += sizeof(float) * 4 * 4;

        renderItems.push_back(std::move(obj));
    }
    for (size_t i = 0; i < node->mNumChildren; i++)
    {
        parseObjectTree(node->mChildren[i], localTransform);
    }
}

void Scene::UploadObjectTransforms(char* mmap)
{
    for (size_t i = 0; i < renderItems.size(); i++)
    {
        RenderItem* ri = &renderItems[i];
        memcpy(mmap + ri->uboOffset, ri->transformation.data(), sizeof(float) * 4 * 4);
    }
}

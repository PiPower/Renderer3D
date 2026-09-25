#include "Scene.hpp"
#pragma comment (lib, "assimp-vc145-mt.lib")
#include <Windows.h>
#include <thread>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


static void imgLoadThread(
    size_t i,
    std::vector<Material>* materials,
    const std::string& rootPath,
    char* stagingPtr,
    size_t imgSize)
{
    if (materials->at(i).baseColorPath == "")
    {
        return;
    }

    std::string filePath = rootPath + materials->at(i).baseColorPath;
    int x, y, comp;
    int forceRGBA = 4;
    stbi_uc* imgData = stbi_load(filePath.c_str(), &x, &y, &comp, forceRGBA);
    memcpy(stagingPtr, imgData, imgSize);
    stbi_image_free(imgData);
}


Scene::Scene(
    const std::string& rootPath,
    const std::string& sceneName)
    :
    rootPath(rootPath), sceneName(sceneName), uboOffset(0), nonEmptyMaterials(0)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(rootPath + sceneName, 
                        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder);

    if (scene == nullptr)
    {
        throw std::runtime_error("Scene path incorrect\n");
    }

    uint32_t imageArrayOffset = 0;
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        aiMaterial* processedMaterial = scene->mMaterials[i];
        aiString baseColor, normals, path;
        aiReturn ret = processedMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &baseColor);
        ret = processedMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &normals);
        aiString d = processedMaterial->GetName();
        materials.emplace_back(processedMaterial->GetName().C_Str(), baseColor.C_Str(), TextureDesc{}, normals.C_Str(), nonEmptyMaterials);
        if (materials.back().name.length() == 0)
        {
            continue;
        }

        nonEmptyMaterials++;

        TextureDesc tex;
        int ok;
        std::string pathName = rootPath + materials.back().baseColorPath;
        ok = stbi_info(pathName.c_str(), &tex.width, &tex.height, &tex.components);
        materials.back().colorTex = tex;

        if (tex.height != materials[0].colorTex.height ||
            tex.width != materials[0].colorTex.width)
        {
            throw std::runtime_error("Textures have unequal dimensions\n");
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

            vertices.at(vecOffest + j).x = scene->mMeshes[i]->mVertices[j].x;
            vertices.at(vecOffest + j).y = scene->mMeshes[i]->mVertices[j].y;
            vertices.at(vecOffest + j).z = scene->mMeshes[i]->mVertices[j].z;

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

        scene->mMeshes[i]->mName;
        sceneGeometry.vbOffset.push_back(vecOffest);
        sceneGeometry.ibOffset.push_back(idxOffset * 3);
        sceneGeometry.indexCount.push_back(scene->mMeshes[i]->mNumFaces * 3);
        sceneGeometry.materialIndex.push_back(materials[scene->mMeshes[i]->mMaterialIndex].index);

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

void Scene::UploadTextureData(
    Renderer* renderer, 
    Image* imageResource)
{
    uint64_t stagingSize = renderer->GetStagingSize();
    uint64_t imgSize = materials[0].colorTex.width * materials[0].colorTex.height * 4;
    uint64_t imgsPerIteraiton = stagingSize / imgSize;
    char* stagingPtr = renderer->GetStagingPtr();

    if (imgsPerIteraiton < 1)
    {
        throw std::runtime_error("Staging buffer size is too small\n");
    }
    uint32_t imgCount = 0;
    size_t currMaterial = 0;
    while (currMaterial < materials.size())
    {
        {
        std::vector<std::jthread> threadPool;
        for (size_t i = 0; i < imgsPerIteraiton && currMaterial < materials.size(); i++, currMaterial++)
        {
            if (materials[currMaterial].baseColorPath != "")
            {
                threadPool.emplace_back(imgLoadThread, currMaterial, &materials, rootPath, stagingPtr + imgCount * imgSize, imgSize);
                imgCount++;
            }
        }
        }
        VkBufferImageCopy copyRegion;
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = imgsPerIteraiton;
        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageExtent = { (uint32_t)materials[0].colorTex.width, (uint32_t)materials[0].colorTex.width, 1 };
        renderer->UploadStagingToImage(imageResource, 1, &copyRegion);
        int x = 2;

    }


}

void Scene::UploadObjectTransforms(char* mmap)
{
    for (size_t i = 0; i < renderItems.size(); i++)
    {
        RenderItem* ri = &renderItems[i];
        //ri->transformation = Eigen::Matrix4f::Zero();
        //ri->transformation(0, 0) = 1;
        //ri->transformation(1, 1) = 1;
        //ri->transformation(2, 2) = 1;
        //ri->transformation(3, 3) = 1;
        //ri->transformation.transposeInPlace();
        memcpy(mmap + ri->uboOffset, ri->transformation.data(), sizeof(float) * 4 * 4);
    }
}


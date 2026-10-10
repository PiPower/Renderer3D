#include "window.hpp"
#include "Renderer/RenderGraph.hpp"
#include <string>
#include "Renderer/Scene.hpp"
#include "Camera.h"
#define SKYBOX_WIDTH 1920
#define SKYBOX_HEIGHT 1920
#define SHADOWMAP_DIM 4096
#undef max
using namespace std;

struct Texel
{
    unsigned char r, g, b, a;
};

void CreateSkybox(
    uint32_t width,
    uint32_t height,
    Image* skyboxImg,
    Renderer* renderer);

void ShadowpassStep(
	const RenderResources& args,
	VkCommandBuffer cmdBuff,
	const RenderingPipeline* pipeline,
	void* args2);

void RenderStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2,
    bool isOpaqueRender);

void RenderStepOpaque(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2)
{
    RenderStep(args, cmdBuff, pipeline, args2, true);
}

void RenderStepTransparent(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2)
{
    RenderStep(args, cmdBuff, pipeline, args2, false);
}

void SkyboxStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2);

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    char pathBuffer[2048];
    DWORD ret = GetEnvironmentVariable(L"SCENE", (LPWSTR)pathBuffer, 1024);
    if (ret == 0)
    {
        MessageBox(NULL, L"Enviroment variable \"SCENE\" not found", NULL, MB_OK);
        exit(-1);
    }
    // this is trivial utf16 to ascii conversion. it probably does not support 
    // every possible path but screw that
    int i = 0;
    std::string rootPath;
    rootPath.reserve(1024);
    while (pathBuffer[i] != '\0')
    {
        rootPath += pathBuffer[i];
        i += 2;
    }

    Window wnd(1600, 900, L"yolo", L"test");
	Renderer renderer(hInstance, wnd.GetWindowHWND(), 1'000'000'000);
    wnd.RegisterResizezable(&renderer, Renderer::OnResize);
	Scene scene(rootPath, "NewSponza_Main_glTF_003.gltf");
    TextureDesc texDesc = scene.GetColorTextureDesc();
    uint32_t trsfMatrixSize = 16 * sizeof(float);

    RenderGraph rg;
    rg.DescribeBuffer("vertex", scene.GetVertexByteSize());
    rg.DescribeBuffer("normal", scene.GetNormalsByteSize());
    rg.DescribeBuffer("texcoord", scene.GetTexByteSize());
    rg.DescribeBuffer("index", scene.GetIndexByteSize());
    rg.DescribeBuffer("camera", 2 * trsfMatrixSize, true, true);
    rg.DescribeBuffer("light_camera", 2 * trsfMatrixSize, true, true);
    rg.DescribeBuffer("object_transform", scene.GetRenderItemCount() * trsfMatrixSize, true, true);
    rg.DescribeImage("output", SWAPCHAIN_RELATIVE, SWAPCHAIN_RELATIVE, 1, renderer.GetSwapchainFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D);
    rg.DescribeImage("depth_image", SWAPCHAIN_RELATIVE, SWAPCHAIN_RELATIVE, 1, VK_FORMAT_D24_UNORM_S8_UINT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D);
    rg.DescribeImage("colorTex", texDesc.width, texDesc.height, scene.GetColorMaterialCount(), VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    rg.DescribeImage("skybox_tex", SKYBOX_WIDTH, SKYBOX_HEIGHT, 6, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_CUBE);
    rg.DescribeImage("shadowmap", SHADOWMAP_DIM, SHADOWMAP_DIM, 1, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D);

    rg.DescribeShader("simple_vert", "main", "shaders/simple.vert");
    rg.DescribeShader("shadowpass_vert", "main", "shaders/shadowpass.vert");
    rg.DescribeShader("simple_frag", "main", "shaders/simple.frag");
    rg.DescribeShader("skybox_vert", "main", "shaders/skybox.vert");
    rg.DescribeShader("skybox_frag", "main", "shaders/skybox.frag");
    rg.MarkAsDisplayImage("output");
    // ------- Shadow Pass -------
    RenderPass* rpShadow = rg.CreateRenderPass("ShadowPass", true);
    rpShadow->AddVertexBuffer("vertex", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpShadow->AddIndexBuffer("index", VK_INDEX_TYPE_UINT32);
    rpShadow->AddDepthImage("shadowmap");
	rpShadow->AddUniformBuffer("light_camera", 2 * trsfMatrixSize, BindLevel::PER_PASS);
	rpShadow->AddUniformBuffer("object_transform", trsfMatrixSize, BindLevel::PER_OBJECT, true);
	rpShadow->AddVertexShader("shadowpass_vert");
	rpShadow->SetRenderFunction(ShadowpassStep);
	rpShadow->SetDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);

	// ------- Non transparent objects -------
	RenderPass* rpSimple = rg.CreateRenderPass("SimpleMainPass", true);
	rpSimple->AddVertexBuffer("vertex", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("normal", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("texcoord", sizeof(Vec2), { VK_FORMAT_R32G32_SFLOAT }, { 0u });
    rpSimple->AddIndexBuffer("index", VK_INDEX_TYPE_UINT32);
    rpSimple->AddDepthImage("depth_image");
    rpSimple->AddUniformBuffer("camera", 2 * trsfMatrixSize, BindLevel::PER_PASS);
    rpSimple->AddUniformBuffer("light_camera", 2 * trsfMatrixSize, BindLevel::PER_PASS);
    rpSimple->AddUniformBuffer("object_transform", trsfMatrixSize, BindLevel::PER_OBJECT, true);
    rpSimple->AddTextureImage("colorTex", scene.GetColorMaterialCount(), BindLevel::PER_MATERIAL, VK_SHADER_STAGE_FRAGMENT_BIT);
    rpSimple->AddTextureImage("shadowmap", 1, BindLevel::PER_PASS, VK_SHADER_STAGE_FRAGMENT_BIT);

	rpSimple->AddColorAttachment("output");

    rpSimple->AddVertexShader("simple_vert");
	rpSimple->AddFragmentShader("simple_frag");
    rpSimple->SetRenderFunction(RenderStepOpaque);

    rpSimple->AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4);
    rpSimple->SetBlendEnable(0, VK_FALSE);

	// ------- Transparent objects -------
    //RenderPass* rpTransparent = rg.CreateRenderPass("TransparentMainPass", true);
    //rpTransparent->AddVertexBuffer("vertex", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    //rpTransparent->AddVertexBuffer("normal", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    //rpTransparent->AddVertexBuffer("texcoord", sizeof(Vec2), { VK_FORMAT_R32G32_SFLOAT }, { 0u });
    //rpTransparent->AddIndexBuffer("index", VK_INDEX_TYPE_UINT32);
    //rpTransparent->AddDepthImage("depth_image");
    //rpTransparent->SetDepthWriteEnable(VK_FALSE);
    //rpTransparent->AddUniformBuffer("camera", 2 * trsfMatrixSize, BindLevel::PER_PASS);
    //rpTransparent->AddUniformBuffer("object_transform", trsfMatrixSize, BindLevel::PER_OBJECT, true);
    //rpTransparent->AddTextureImage("colorTex", scene.GetColorMaterialCount(), BindLevel::PER_MATERIAL, VK_SHADER_STAGE_FRAGMENT_BIT);
    //rpTransparent->AddColorAttachment("output");
    //rpTransparent->AddTextureImage("shadowmap", 1, BindLevel::PER_PASS, VK_SHADER_STAGE_FRAGMENT_BIT);
    //
    //rpTransparent->AddVertexShader("simple_vert");
    //rpTransparent->AddFragmentShader("simple_frag");
    //rpTransparent->SetRenderFunction(RenderStepTransparent);
    //
    //rpTransparent->AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4);
    //rpTransparent->SetBlendEnable(0, VK_TRUE)
    //    .SetSrcColorBlendFactor(0, VK_BLEND_FACTOR_SRC_ALPHA)
    //    .SetDstColorBlendFactor(0, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
    //    .SetColorBlendOp(0, VK_BLEND_OP_ADD)
    //    .SetSrcAlphaBlendFactor(0, VK_BLEND_FACTOR_SRC_ALPHA)
    //    .SetDstAlphaBlendFactor(0, VK_BLEND_FACTOR_CONSTANT_ALPHA)
    //    .SetAlphaBlendOp(0, VK_BLEND_OP_ADD)
    //    .SetBlendConstant3(0.35f);

    // ------- Skybox Pass -------
    RenderPass* rpSkybox = rg.CreateRenderPass("Skybox", true);
    rpSkybox->AddUniformBuffer("camera", 2 * trsfMatrixSize, BindLevel::PER_PASS);
	rpSkybox->AddTextureImage("skybox_tex", 6, BindLevel::PER_PASS, VK_SHADER_STAGE_FRAGMENT_BIT);
    rpSkybox->AddColorAttachment("output");
    rpSkybox->AddDepthImage("depth_image");

    rpSkybox->AddVertexShader("skybox_vert");
    rpSkybox->AddFragmentShader("skybox_frag");
    rpSkybox->SetRenderFunction(SkyboxStep);

    rpSkybox->SetDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);
    // ------- Setting up resources -------
	rg.Compile(&renderer);
    rg.UploadDataToBuffer("vertex", scene.GetVertexByteSize(), (const char*)scene.GetVertexPtr(), 0, 0);
    rg.UploadDataToBuffer("normal", scene.GetNormalsByteSize(), (const char*)scene.GetNormalsPtr(), 0, 0);
    rg.UploadDataToBuffer("texcoord", scene.GetTexByteSize(), (const char*)scene.GetTexPtr(), 0, 0);
    rg.UploadDataToBuffer("index", scene.GetIndexByteSize(), (const char*)scene.GetIndexPtr(), 0, 0);

    char* cameraUbo = rg.GetPtrToVisibleBuffer("camera");
    char* lightUbo = rg.GetPtrToVisibleBuffer("light_camera");
    char* objectUbo = rg.GetPtrToVisibleBuffer("object_transform");
    scene.UploadObjectTransforms(objectUbo);
    scene.UploadTextureData(&renderer, rg.GetImage("colorTex"));
    CreateSkybox(SKYBOX_WIDTH, SKYBOX_HEIGHT, rg.GetImage("skybox_tex"), &renderer);
    RenderingData rd = scene.GetRenderingData();

    Eigen::Vector3f pos { 0, 2, -3 };
    Eigen::Vector3f lookDir{ 0, 0, 1 };
    Eigen::Vector3f up{ 0 ,1, 0 };
    Eigen::Vector3f lightPos{ 2.5, 30, 0 };
    Eigen::Vector3f lightDir{ 0, -1, 0 };
    Eigen::Vector3f lightUp{ -1 , 0, 0 };

    Camera cam(pos, lookDir, up, cameraUbo);
    Camera lightCam(lightPos, lightDir, lightUp, lightUbo);
    VkExtent2D screenRes = renderer.GetSwapchainCapabilities().currentExtent;
    cam.UpdateViewMatrix();
    cam.UpdateProjMatrix(3.14f / 4.0f, (float)screenRes.width/ (float)screenRes.height, 0.3f, 80.0f);

	lightCam.UpdateViewMatrix();
	lightCam.UpdateOrthographicProjMatrix(30, 40, 0.3f, 50.0f);

    float dt = 0.001f;

    while (wnd.ProcessMessages() == 0)
    {
        cam.ProcessUserInput(&wnd, dt * 10);

        auto t1 = chrono::high_resolution_clock::now();
        rg.Render(&rd);
        auto t2 = chrono::high_resolution_clock::now();

        chrono::duration duration = t2 - t1;
        dt = (float)duration.count() / 1'000'000'000.0f;
        //dt = 0.001;
    }

}

void CreateSkybox(
    uint32_t width,
    uint32_t height,
    Image* skyboxImg,
    Renderer* renderer)
{
    std::vector<Texel> texelBuffer(width * height);
    Texel* texData = texelBuffer.data();

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float x0 = ((float)x / (float)width) * 3 - 2.2f;
            float y0 = ((float)y / (float)height) * 3 - 1.4f;
            float z_x = 0;
            float z_y = 0;
            constexpr uint16_t max_iteration = 300;
            uint16_t i = 0;
            while (z_x * z_x + z_y * z_y < 4.0f && i < max_iteration)
            {
                float xtemp = z_x * z_x - z_y * z_y + x0;
                z_y = 2 * z_x * z_y + y0;
                z_x = xtemp;
                i++;
            }


            texData[width * y + x].a = 255;
            texData[width * y + x].r = ((float)i / (float)max_iteration) * 255;
            texData[width * y + x].g = sinf(((float)i / (float)max_iteration) * 2 * 3.14) * 255;
            texData[width * y + x].b = ((float)i / (float)max_iteration) * 255;
        }
    }
    uint64_t stagingSize = renderer->GetStagingSize(); 
    if (stagingSize < width * height * sizeof(Texel) * 6)
    {
        throw std::runtime_error("Staging buffer to small for cube mapping texture data\n");
    }
    char* data = renderer->GetStagingPtr();
    for (int i = 0; i < 6; i++)
    {
        memcpy(data + i * width * height * sizeof(Texel), texData, width * height * sizeof(Texel));
    }

    VkBufferImageCopy copyRegion;
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 6;
    copyRegion.imageOffset = { 0, 0, 0 };
    copyRegion.imageExtent = { width, height, 1 };
    renderer->UploadStagingToImage(skyboxImg, 1, &copyRegion);

}

void ShadowpassStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2)
{
    RenderingData* rd = (RenderingData*)args2;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = SHADOWMAP_DIM;
    viewport.height = SHADOWMAP_DIM;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuff, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { SHADOWMAP_DIM, SHADOWMAP_DIM };
    vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

    uint32_t offsets[1] = { 0 };
    vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 3, pipeline->sets.data(), 1, offsets);

    for (size_t item = 0; item < rd->opaqueMaterialsCount; item++)
    {
        const RenderItem* renderItem = &rd->renderItems[item];
        uint32_t dynamicOffset[1] = { renderItem->uboOffset };
        vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 2, 1, pipeline->sets.data() + 2, 1, dynamicOffset);

        for (size_t i = 0; i < renderItem->meshIdx.size(); i++)
        {
            uint32_t currentMesh = renderItem->meshIdx[i];
            uint32_t colorIdx = rd->sceneGeometry.colorTexIndex[currentMesh];
            if (colorIdx == std::numeric_limits<uint32_t>::max())
            {
                continue;
            }
            vkCmdDrawIndexed(cmdBuff,
                rd->sceneGeometry.indexCount[currentMesh],
                1,
                rd->sceneGeometry.ibOffset[currentMesh],
                rd->sceneGeometry.vbOffset[currentMesh],
                0);
        }

    }
}

void RenderStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2,
    bool isOpaqueRender)
{
    RenderingData* rd = (RenderingData*)args2;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 1600;
    viewport.height = 900;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuff, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { 1600, 900 };
    vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

    uint32_t offsets[1] = { 0 };
    vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 3, pipeline->sets.data(), 1, offsets);

	size_t bound = isOpaqueRender ? rd->opaqueMaterialsCount : rd->renderItems.size();

    for (size_t item = isOpaqueRender? 0 : rd->opaqueMaterialsCount; item < bound; item++)
    {
        const RenderItem* renderItem = &rd->renderItems[item];
        uint32_t dynamicOffset[1] = { renderItem->uboOffset };
        vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 2, 1, pipeline->sets.data() + 2, 1, dynamicOffset);

        for (size_t i = 0; i < renderItem->meshIdx.size(); i++)
        {
            uint32_t currentMesh = renderItem->meshIdx[i];
            uint32_t colorIdx = rd->sceneGeometry.colorTexIndex[currentMesh];
            if (colorIdx == std::numeric_limits<uint32_t>::max())
            {
                continue;
            }
            vkCmdPushConstants(cmdBuff, pipeline->layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &colorIdx);
            vkCmdDrawIndexed(cmdBuff,
                rd->sceneGeometry.indexCount[currentMesh],
                1, 
                rd->sceneGeometry.ibOffset[currentMesh],
                rd->sceneGeometry.vbOffset[currentMesh],
                0);
        }

    }
}

void SkyboxStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2)
{
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 1600;
    viewport.height = 900;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuff, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { 1600, 900 };
    vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 3, pipeline->sets.data(), 0, nullptr);
    vkCmdDraw(cmdBuff, 36, 1, 0, 0);
}

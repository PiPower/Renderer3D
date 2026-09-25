#include "window.hpp"
#include "Renderer/RenderGraph.hpp"
#include <string>
#include "Renderer/Scene.hpp"
#include "Camera.h"
using namespace std;

void RenderStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2);

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    char pathBuffer[2048];
    GetEnvironmentVariable(L"SCENE", (LPWSTR)pathBuffer, 1024);
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
    rg.DescribeBuffer("object_transform", scene.GetRenderItemCount() * trsfMatrixSize, true, true);
    rg.DescribeImage("output", SWAPCHAIN_RELATIVE, SWAPCHAIN_RELATIVE, 1, renderer.GetSwapchainFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D);
    rg.DescribeImage("depth_image", SWAPCHAIN_RELATIVE, SWAPCHAIN_RELATIVE, 1, VK_FORMAT_D24_UNORM_S8_UINT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D);
    rg.DescribeImage("colorTex", texDesc.width, texDesc.height, scene.GetColorMaterialCount(), VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY);

    rg.DescribeShader("simple_vert", "main", "shaders/simple.vert");
    rg.DescribeShader("simple_frag", "main", "shaders/simple.frag");
    rg.MarkAsDisplayImage("output");

	RenderPass* rpSimple = rg.CreateRenderPass("SimpleMainPass", true);
	rpSimple->AddVertexBuffer("vertex", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("normal", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("texcoord", sizeof(Vec2), { VK_FORMAT_R32G32_SFLOAT }, { 0u });
    rpSimple->AddIndexBuffer("index", VK_INDEX_TYPE_UINT32);
    rpSimple->AddDepthImage("depth_image");
    rpSimple->AddUniformBuffer("camera", 2 * trsfMatrixSize, BindLevel::PER_PASS);
    rpSimple->AddUniformBuffer("object_transform", trsfMatrixSize, BindLevel::PER_OBJECT, true);
    rpSimple->AddTextureImage("colorTex", scene.GetColorMaterialCount(), BindLevel::PER_MATERIAL, VK_SHADER_STAGE_FRAGMENT_BIT);
	rpSimple->AddColorAttachment("output");

    rpSimple->AddVertexShader("simple_vert");
	rpSimple->AddFragmentShader("simple_frag");
    rpSimple->SetRenderFunction(RenderStep);
    //RenderPass* rpSkybox = rg.CreateRenderPass("Skybox", true);
	//rpSimple->AddTextureImage("skybox");
    //rpSimple->AddOutputImage("output");

	rg.Compile(&renderer);
    rg.UploadDataToBuffer("vertex", scene.GetVertexByteSize(), (const char*)scene.GetVertexPtr(), 0, 0);
    rg.UploadDataToBuffer("normal", scene.GetNormalsByteSize(), (const char*)scene.GetNormalsPtr(), 0, 0);
    rg.UploadDataToBuffer("texcoord", scene.GetTexByteSize(), (const char*)scene.GetTexPtr(), 0, 0);
    rg.UploadDataToBuffer("index", scene.GetIndexByteSize(), (const char*)scene.GetIndexPtr(), 0, 0);

    char* cameraUbo = rg.GetPtrToVisibleBuffer("camera");
    char* objectUbo = rg.GetPtrToVisibleBuffer("object_transform");
    scene.UploadObjectTransforms(objectUbo);
    scene.UploadTextureData(&renderer, rg.GetImage("colorTex"));
    RenderingData rd = scene.GetRenderingData();

    Eigen::Vector3f pos { 0, 0, -7 };
    Eigen::Vector3f lookDir{ 0, 0, 1 };
    Eigen::Vector3f up{ 0 ,1, 0 };
    Camera cam(pos, lookDir, up, cameraUbo);
    VkExtent2D screenRes = renderer.GetSwapchainCapabilities().currentExtent;
    cam.UpdateViewMatrix();
    cam.UpdateProjMatrix(3.14f / 4.0f, (float)screenRes.width/ (float)screenRes.height, 0.3f, 80.0f);

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

void RenderStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline,
    void* args2)
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

    for (size_t item = 0; item < rd->renderItems.size(); item++)
    {
        const RenderItem* renderItem = &rd->renderItems[item];
        uint32_t dynamicOffset[1] = { renderItem->uboOffset };
        vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 2, 1, pipeline->sets.data() + 2, 1, dynamicOffset);

        for (size_t i = 0; i < renderItem->meshIdx.size(); i++)
        {
            uint32_t currentMesh = renderItem->meshIdx[i];
            uint32_t materialIndex = rd->sceneGeometry.colorTexIndex[currentMesh];
            vkCmdDrawIndexed(cmdBuff,
                rd->sceneGeometry.indexCount[currentMesh],
                1, 
                rd->sceneGeometry.ibOffset[currentMesh],
                rd->sceneGeometry.vbOffset[currentMesh],
                0);
        }

    }
}

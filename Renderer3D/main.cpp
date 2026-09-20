#include "window.hpp"
#include "Renderer/RenderGraph.hpp"
#include <string>
#include "Renderer/Scene.hpp"
#include "Camera.h"
using namespace std;

void RenderStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline);

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    Window wnd(1600, 900, L"yolo", L"test");
	Renderer renderer(hInstance, wnd.GetWindowHWND(), 100'000'000);
    wnd.RegisterResizezable(&renderer, Renderer::OnResize);
	Scene scene("D:\\main1_sponza\\NewSponza_Main_glTF_003.gltf");
    
    uint32_t trsfMatrixSize = 16 * sizeof(float);

    RenderGraph rg;
    rg.DescribeBuffer("vertex", scene.GetVertexByteSize());
    rg.DescribeBuffer("normal", scene.GetNormalsByteSize());
    rg.DescribeBuffer("texcoord", scene.GetTexByteSize());
    rg.DescribeBuffer("index", scene.GetIndexByteSize());
    rg.DescribeBuffer("camera", 2 * trsfMatrixSize, true, true);
    rg.DescribeBuffer("object_transform", 80 * trsfMatrixSize, true, true);
    rg.DescribeImage("output", SWAPCHAIN_RELATIVE, SWAPCHAIN_RELATIVE, 1, renderer.GetSwapchainFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D);
    rg.DescribeShader("simple_vert", "main", "shaders/simple.vert");
    rg.DescribeShader("simple_frag", "main", "shaders/simple.frag");

	RenderPass* rpSimple = rg.CreateRenderPass("SimpleMainPass", true);
	rpSimple->AddVertexBuffer("vertex", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("normal", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("texcoord", sizeof(Vec2), { VK_FORMAT_R32G32_SFLOAT }, { 0u });
    rpSimple->AddIndexBuffer("index", VK_INDEX_TYPE_UINT32);

    rpSimple->AddUniformBuffer("camera", 2 * trsfMatrixSize, BindLevel::PER_PASS);
    rpSimple->AddUniformBuffer("object_transform", trsfMatrixSize, BindLevel::PER_OBJECT, true);

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


    Eigen::Vector3f pos { 0, 0, 0 };
    Eigen::Vector3f lookDir{ 0, 0, 1 };
    Eigen::Vector3f up{ 0 ,1, 0 };
    Camera cam(pos, lookDir, up, cameraUbo);

    VkExtent2D screenRes = renderer.GetSwapchainCapabilities().currentExtent;
    //float viewHeight = winRect.
    cam.UpdateViewMatrix();
    cam.UpdateProjMatrix(3.14f / 4.0f, (float)screenRes.width/ (float)screenRes.height, 0.1f, 128.0f);

    float dt = 0.001f;
    while (wnd.ProcessMessages() == 0)
    {
        auto t1 = chrono::high_resolution_clock::now();
        rg.Render();
        renderer.RenderFrame();

        auto t2 = chrono::high_resolution_clock::now();
        chrono::duration duration = t2 - t1;
        dt = (float)duration.count() / 1'000'000'000.0f;
        //dt = 0.001;
    }

}

void RenderStep(
    const RenderResources& args,
    VkCommandBuffer cmdBuff,
    const RenderingPipeline* pipeline)
{
    uint32_t offsets[1] = { 0 };
    vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, 3, pipeline->sets.data(), 1, offsets);
}

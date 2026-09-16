#include "window.hpp"
#include "Renderer/RenderGraph.hpp"
#include <string>
#include "Renderer/Scene.hpp"
using namespace std;


int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	Scene scene("D:\\main1_sponza\\NewSponza_Main_glTF_003.gltf");
    Window wnd(1600, 900, L"yolo", L"test");
	Renderer renderer(hInstance, wnd.GetWindowHWND());
    wnd.RegisterResizezable(&renderer, Renderer::OnResize);
    
    std::vector<std::string> bufferNames = { "vertex", "normal" , "texcoord", "index", "camera", "object_transform" };
    std::vector<std::string> imageNames = {"output", "skybox"};
	std::vector<ShaderDesc> shaderDescs = {
		{"simple_vert", "main", "shaders/simple.vert"},
		{"simple_frag", "main", "shaders/simple.frag"},
	};  
    RenderGraph rg(bufferNames, imageNames, shaderDescs);
    rg.DescribeBuffer("vertex", 1000 * sizeof(float));
    rg.DescribeBuffer("normal", 1000 * sizeof(float));
    rg.DescribeBuffer("texcoord", 1000 * sizeof(float));
    rg.DescribeImage("output", SWAPCHAIN_RELATIVE, SWAPCHAIN_RELATIVE, 1, renderer.GetSwapchainFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D);

	RenderPass* rpSimple = rg.CreateRenderPass("SimpleMainPass", true);
	rpSimple->AddVertexBuffer("vertex", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("normal", sizeof(Vec3), { VK_FORMAT_R32G32B32_SFLOAT }, { 0u });
    rpSimple->AddVertexBuffer("texcoord", sizeof(Vec2), { VK_FORMAT_R32G32_SFLOAT }, { 0u });
    rpSimple->AddIndexBuffer("index");

    rpSimple->AddUniformBuffer("camera", BindLevel::PER_PASS);
    rpSimple->AddUniformBuffer("object_transform", BindLevel::PER_OBJECT, true);

	rpSimple->AddColorAttachment("output");

	rpSimple->AddVertexShader("simple_vert");
	rpSimple->AddFragmentShader("simple_frag");

    //RenderPass* rpSkybox = rg.CreateRenderPass("Skybox", true);
	//rpSimple->AddTextureImage("skybox");
    //rpSimple->AddOutputImage("output");

	rg.Compile(&renderer);

    float dt = 0.001f;
    while (wnd.ProcessMessages() == 0)
    {
        auto t1 = chrono::high_resolution_clock::now();

        renderer.RenderFrame();

        auto t2 = chrono::high_resolution_clock::now();
        chrono::duration duration = t2 - t1;
        dt = (float)duration.count() / 1'000'000'000.0f;
        //dt = 0.001;
    }

}


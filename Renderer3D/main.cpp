#include "window.hpp"
#include "Renderer/Renderer.hpp"
#include <string>
#include "Renderer/Scene.hpp"
using namespace std;

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	Scene scene("D:\\main1_sponza\\NewSponza_Main_glTF_003.gltf");
    Window wnd(1600, 900, L"yolo", L"test");
	Renderer renderer(hInstance, wnd.GetWindowHWND());
    wnd.RegisterResizezable(&renderer, Renderer::OnResize);
    
    std::vector<std::string> bufferNames = { "vertex", "normal" , "texcoord", "index"};
    std::vector<std::string> imageNames = {"output", "skybox"};
    RenderGraph rg(bufferNames, imageNames);

	RenderPass* rpSimple = rg.CreateRenderPass("SimpleMainPass", true);
	rpSimple->AddVertexBuffer("vertex");
    rpSimple->AddVertexBuffer("normal");
    rpSimple->AddVertexBuffer("texcoord");
	rpSimple->AddIndexBuffer("index");
	rpSimple->AddOutputImage("output");

    //RenderPass* rpSkybox = rg.CreateRenderPass("Skybox", true);
	//rpSimple->AddTextureImage("skybox");
    //rpSimple->AddOutputImage("output");


    float dt = 0.001f;
    while (wnd.ProcessMessages() == 0)
    {
        auto t1 = chrono::high_resolution_clock::now();

		renderer.ExecuteGraph(rg.GetExecutionGraph());
        renderer.RenderFrame();

        auto t2 = chrono::high_resolution_clock::now();
        chrono::duration duration = t2 - t1;
        dt = (float)duration.count() / 1'000'000'000.0f;
        //dt = 0.001;
    }

}


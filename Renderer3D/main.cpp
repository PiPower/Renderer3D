#include "window.hpp"
#include "Renderer/Renderer.hpp"
#include <string>

using namespace std;

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    Window wnd(1600, 900, L"yolo", L"test");
	Renderer renderer(hInstance, wnd.GetWindowHWND());
    wnd.RegisterResizezable(&renderer, Renderer::OnResize);

    float dt = 0.001;
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
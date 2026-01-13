import RendererCore;    

int main()
{
    // Start the Renderer
    bool runRenderLoop = InitRenderer();
    
    if (runRenderLoop)
        RenderLoop();
    else
        CleanUpAndShutdown();    
}

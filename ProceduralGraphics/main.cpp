import RendererCore;    

int main()
{
    // Start the Renderer
    bool runRenderLoop = InitRenderer();
    
    if (runRenderLoop)
        RenderLoop();
    
    // Clean up and release everything after application shutdown
    TerminateAndCleanUp();
}

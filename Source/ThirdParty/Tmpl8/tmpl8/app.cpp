#include "app.h"
#include "opengl.h"

using namespace Tmpl8;

#pragma comment( linker, "/subsystem:console /ENTRY:mainCRTStartup" )

// Enable usage of dedicated GPUs in notebooks
#ifdef WIN32
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}

extern "C"
{
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

GLFWwindow* window = 0;
GLFWwindow* GetGLFWWindow() { return window; }
static bool hasFocus = true, running = true;
static GLTexture* renderTarget = 0;
static int scrwidth = 0, scrheight = 0;
static Application* app = 0;
uint keystate[512] = { 0 };

// static member data for instruction set support class
static const CPUCaps cpucaps;

// provide access to the render target, for OpenCL / OpenGL interop
GLTexture* GetRenderTarget() { return renderTarget; }

// provide access to window focus state
bool WindowHasFocus() { return hasFocus; }

// provide access to key state array
bool IsKeyDown( const uint key ) { return keystate[key & 511] == 1; }

// GLFW callbacks
void InitRenderTarget( int w, int h )
{
	// allocate render target and surface
	scrwidth = w, scrheight = h;
	renderTarget = new GLTexture( scrwidth, scrheight, GLTexture::INTTARGET );
}
void ReshapeWindowCallback( GLFWwindow*, int w, int h )
{
	glViewport( 0, 0, w, h );
}
void KeyEventCallback( GLFWwindow*, int key, int, int action, int )
{
	if (key == GLFW_KEY_ESCAPE) running = false;
	if (action == GLFW_PRESS) { if (app) if (key >= 0) app->KeyDown( key ); keystate[key & 511] = 1; }
	else if (action == GLFW_RELEASE) { if (app) if (key >= 0) app->KeyUp( key ); keystate[key & 511] = 0; }
}
void CharEventCallback( GLFWwindow*, uint ) { /* nothing here yet */ }
void WindowFocusCallback( GLFWwindow*, int focused ) { hasFocus = (focused == GL_TRUE); }
void MouseButtonCallback( GLFWwindow*, int button, int action, int )
{
	if (action == GLFW_PRESS) { if (app) app->MouseDown( button ); }
	else if (action == GLFW_RELEASE) { if (app) app->MouseUp( button ); }
}
void MouseScrollCallback( GLFWwindow*, double, double y )
{
	if (app) app->MouseWheel( (float)y );
}
void MousePosCallback( GLFWwindow*, double x, double y )
{
	if (app) app->MouseMove( (int)x, (int)y );
}
void ErrorCallback( int, const char* description )
{
	fprintf( stderr, "GLFW Error: %s\n", description );
}

int main()
{
	_mm_setcsr( _mm_getcsr() | (_MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON) );
	if (!glfwInit()) FatalError( "glfwInit failed." );
	glfwSetErrorCallback( ErrorCallback );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
	glfwWindowHint( GLFW_STENCIL_BITS, GL_FALSE );
	glfwWindowHint( GLFW_RESIZABLE, GL_FALSE );
#ifdef FULLSCREEN
	window = glfwCreateWindow( SCRWIDTH, SCRHEIGHT, "voxpopuli", glfwGetPrimaryMonitor(), 0 );
#else
#ifdef DOUBLESIZE
	window = glfwCreateWindow( SCRWIDTH * 2, SCRHEIGHT * 2, "voxpopuli", 0, 0 );
#else
	window = glfwCreateWindow( SCRWIDTH, SCRHEIGHT, "voxpopuli", 0, 0 );
#endif
#endif
	if (!window) FatalError( "glfwCreateWindow failed." );
	glfwMakeContextCurrent( window );
	glfwSetWindowSizeCallback( window, ReshapeWindowCallback );
	glfwSetKeyCallback( window, KeyEventCallback );
	glfwSetWindowFocusCallback( window, WindowFocusCallback );
	glfwSetMouseButtonCallback( window, MouseButtonCallback );
	glfwSetScrollCallback( window, MouseScrollCallback );
	glfwSetCursorPosCallback( window, MousePosCallback );
	glfwSetCharCallback( window, CharEventCallback );
	if (!gladLoadGLLoader( (GLADloadproc)glfwGetProcAddress )) FatalError( "gladLoadGLLoader failed." );
	glfwSwapInterval( 0 );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	CheckGL();

	// Initialize render target and app
	InitRenderTarget( SCRWIDTH, SCRHEIGHT );
	Surface* screen = new Surface( SCRWIDTH, SCRHEIGHT );

	app = CreateApplication();
	app->screen = screen;
	app->Init();

	// Setup ImGui
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL( window, true );
	ImGui_ImplOpenGL3_Init( "#version 330" );
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.IniFilename = "assets/imgui.ini";

	Shader* shader = new Shader(
		"#version 330\nin vec4 p;\nout vec2 u;void main(){u=vec2((p.x+1)/2,1-(p.y+1)/2);gl_Position=vec4(p.x,p.y,1,1);}",
		"#version 330\nuniform sampler2D c;in vec2 u;out vec4 f;void main(){f=texture(c,u);}", true );

	Timer timer;
	static int frameNr = 0;
	while (!glfwWindowShouldClose( window ) && running)
	{
		float deltaTime = timer.elapsed();
		timer.reset();
		app->Tick( deltaTime );
		// Skip first frame
		if (frameNr++ > 1)
		{
			// Draw the voxel render target
			if (app->screen) renderTarget->CopyFrom( app->screen );
			shader->Bind();
			shader->SetInputTexture( 0, "c", renderTarget );
			DrawQuad();
			shader->Unbind();
			// Update ImGui
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			app->UI();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
			// Handle window resize
			int display_w, display_h;
			glfwGetFramebufferSize( window, &display_w, &display_h );
			glViewport( 0, 0, display_w, display_h );
			glfwSwapBuffers( window );
			glfwPollEvents();
		}
	}
	app->Shutdown();
	app->screen = nullptr;

	delete screen;
	delete app;

	Kernel::KillCL();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	delete shader;
	glfwDestroyWindow( window );
	glfwTerminate();
	return 0;
}

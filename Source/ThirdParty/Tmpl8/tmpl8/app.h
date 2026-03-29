#pragma once

#include "template.h"

namespace Tmpl8 {

class Application
{
public:
	virtual ~Application() = default;
	virtual void Init() = 0;
	virtual void Tick( float deltaTime ) = 0;
	virtual void UI() {}
	virtual void Shutdown() {}

	virtual void MouseUp( int ) {}
	virtual void MouseDown( int ) {}
	virtual void MouseMove( int , int ) {}
	virtual void MouseWheel( float ) {}
	virtual void KeyUp( int ) {}
	virtual void KeyDown( int ) {}

	Surface* screen = nullptr;
};

// Entry point function that the application must implement
Application* CreateApplication();

} // namespace Tmpl8

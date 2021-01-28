//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platformWin32/platformWin32.h"
#include "console/console.h"
#include "platformWin32/win32WinMgr.h"
#include "core/unicode.h"


//------------------------------------------------------------------
// Win32WinMgr
//------------------------------------------------------------------
Win32WinMgr::Win32WinMgr( U32 uniqueIDNum, WNDPROC winProc)
{
   char *buffer = new char[64];
   dSprintf( buffer, 64, "%s_%d", "Torque2", uniqueIDNum );

#ifdef UNICODE
   UTF16 *buff = new UTF16[64];
   convertUTF8toUTF16( (UTF8 *)buffer, buff, 64 );

   delete [] buffer;
#else
   char *buff = buffer;
#endif

   winState.appWindow = NULL;

   WNDCLASSEX wc = { sizeof( WNDCLASSEX ), CS_CLASSDC, winProc, 0L, 0L, 
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      buff, NULL };

   winState.wc = wc;

   if( !RegisterClassEx( &winState.wc ) )
   {
      AssertFatal( false, "Unable to register window class" );
      Con::errorf( "*** Unable to register window class!! ***" );
   }

   // Do NOT delete buff. It is now owned by wc
}

//--------------------------------------
Win32WinMgr::~Win32WinMgr()
{
   destroyWindow();
   UnregisterClass( winState.wc.lpszClassName, winState.wc.hInstance );
}

//--------------------------------------
RectI Win32WinMgr::getCenteredWindowRect( const U32 width, const U32 height, bool fullscreen )
{
   DWORD	dwExStyle;
   DWORD dwStyle;
   
   dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
   dwStyle=WS_OVERLAPPEDWINDOW;

   
   RECT windowRect;
   windowRect.left = 0;
   windowRect.top = 0;
   windowRect.right = width;
   windowRect.bottom = height;

   AdjustWindowRectEx(&windowRect, dwStyle, false, dwExStyle);
   RectI newRect;
   newRect.point = Point2I(0,0);
   newRect.extent.x = windowRect.right - windowRect.left;
   newRect.extent.y = windowRect.bottom - windowRect.top;

   //don't "center" the screen if it's in fullscreen - we're gonna take up the entire thing!
   if( !fullscreen )
   {
      newRect.point.x += (winState.desktopWidth - newRect.extent.x) / 2;
      newRect.point.y += (winState.desktopHeight - newRect.extent.y) / 2;
   }

   return newRect;
}

//--------------------------------------
void Win32WinMgr::resizeWindow( const U32 width, const U32 height, bool fullscreen )
{

   RectI newRect = getCenteredWindowRect( width, height, fullscreen );

   SetWindowPos( winState.appWindow, HWND_NOTOPMOST, newRect.point.x, newRect.point.y,
                 newRect.extent.x, newRect.extent.y, NULL );
}

//--------------------------------------
void Win32WinMgr::createWindow( const char *windowTitle, const U32 x, const U32 y, const U32 width, const U32 height, const U32 frequency, bool fullscreen )
{
   destroyWindow();

   RectI newRect = getCenteredWindowRect( width, height, fullscreen );

   DWORD	dwExStyle;
   DWORD dwStyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
   dwStyle |= WS_OVERLAPPED | WS_THICKFRAME | WS_CAPTION;

   dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

#ifdef UNICODE
   UTF16 winTitle[64];
   convertUTF8toUTF16( (UTF8 *)windowTitle, winTitle, sizeof(winTitle) );
#else
   const char *winTitle = windowTitle;
#endif


   bool result = winState.appWindow = CreateWindowEx(
         dwExStyle,
         winState.wc.lpszClassName,          //class name
         winTitle,                        //window title
         dwStyle,                            //style - need clip siblings/children for opengl
         newRect.point.x,
         newRect.point.y,
         newRect.extent.x,
         newRect.extent.y,
         NULL,//GetDesktopWindow(),          //parent window
         NULL,                               //menu? No.
         winState.wc.hInstance,              //the hInstance
         NULL );                             //no funky params


   AssertFatal(result, "Could not create window");

   ShowWindow( winState.appWindow, SW_SHOWDEFAULT );
   UpdateWindow( winState.appWindow ); // Repaint for good measure
}

//--------------------------------------
void Win32WinMgr::destroyWindow()
{
   //One of two things could cause us not to have the hWnd.
   //1. The GFX Device hasn't been created yet,
   //   which will cause the fullscreen check to crash the engine
   //2. There is no window - so why destroy one?
   if (winState.appWindow == NULL)
      return;

   DestroyWindow( winState.appWindow );
   winState.appWindow = NULL;
}

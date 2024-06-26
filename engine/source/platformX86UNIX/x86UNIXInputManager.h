//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------



#ifndef _X86UNIXINPUTMANAGER_H_
#define _X86UNIXINPUTMANAGER_H_

#include "core/tVector.h"
#include "platform/platformInput.h"
#include "platformX86UNIX/platformX86UNIX.h"

#include <SDL/SDL_events.h>

#define NUM_KEYS ( KEY_OEM_102 + 1 )
#define KEY_FIRST KEY_ESCAPE

struct AsciiData
{
   struct KeyData
   {
      U16   ascii;
      bool  isDeadChar;
   };

   KeyData upper;
   KeyData lower;
   KeyData goofy;
};

typedef struct _SDL_Joystick;

struct JoystickAxisInfo
{
      S32 type;
      S32 minValue;
      S32 maxValue;
};

//------------------------------------------------------------------------------
class JoystickInputDevice : public InputDevice
{
  public:
    JoystickInputDevice(U8 deviceID);
    ~JoystickInputDevice();

    bool activate();
    bool deactivate();
    bool isActive() { return( mActive ); }

    U8 getDeviceType() { return( JoystickDeviceType ); }
    U8 getDeviceID() { return( mDeviceID ); }
    const char* getName();
    const char* getJoystickAxesString();

    void loadJoystickInfo();
    void loadAxisInfo();
    JoystickAxisInfo& getAxisInfo(int axisNum) { return mAxisList[axisNum]; }

    bool process();
    void reset();

  private:
    bool mActive;
    U8 mDeviceID;
    SDL_Joystick* mStick;
    Vector<JoystickAxisInfo> mAxisList;
    Vector<bool> mButtonState;
    Vector<U8> mHatState;

    S32 mNumAxes;
    S32 mNumButtons;
    S32 mNumHats;
    S32 mNumBalls;
};

//------------------------------------------------------------------------------
class UInputManager : public InputManager
{
   friend bool JoystickInputDevice::process(); // for joystick event funcs
   friend void JoystickInputDevice::reset();

   public:
      UInputManager();

      void init();
      bool enable();
      void disable();
      void activate();
      void deactivate();
      void setWindowLocked(bool locked);
      bool isActive()               { return( mActive ); }

      void onDeleteNotify( SimObject* object );
      bool onAdd();
      void onRemove();

      void process();

      bool enableKeyboard();
      void disableKeyboard();
      bool isKeyboardEnabled()      { return( mKeyboardEnabled ); }
      bool activateKeyboard();
      void deactivateKeyboard();
      bool isKeyboardActive()       { return( mKeyboardActive ); }

      bool enableMouse();
      void disableMouse();
      bool isMouseEnabled()         { return( mMouseEnabled ); }
      bool activateMouse();
      void deactivateMouse();
      bool isMouseActive()          { return( mMouseActive ); }

      bool enableJoystick();
      void disableJoystick();
      bool isJoystickEnabled()      { return( mJoystickEnabled ); }
      bool activateJoystick();
      void deactivateJoystick();
      bool isJoystickActive()       { return( mJoystickActive ); }

      void setLocking(bool enabled);
      bool getLocking() { return mLocking; }

      const char* getJoystickAxesString( U32 deviceID );
      bool joystickDetected()       { return mJoystickList.size() > 0; }
   private:
      typedef SimGroup Parent;
      // the following vector is just for quick access during event processing.
      // it does not manage the cleanup of the JoystickInputDevice objects
      Vector<JoystickInputDevice*> mJoystickList;

      bool mKeyboardEnabled;
      bool mMouseEnabled;
      bool mJoystickEnabled;

      bool mKeyboardActive;
      bool mMouseActive;
      bool mJoystickActive;

      bool mActive;

      // Device state variables
      S32 mModifierKeys;
      bool mKeyboardState[256];
      bool mMouseButtonState[3];

      // last mousex and y are maintained when window is unlocked
      S32 mLastMouseX;
      S32 mLastMouseY;

      void initJoystick();

      void resetKeyboardState();
      void resetMouseState();
      void resetInputState();

      void lockInput();
      void unlockInput();
      bool mLocking;

      void joyHatEvent(U8 deviceID, U8 hatNum,
          U8 prevHatState, U8 currHatState);
      void joyButtonEvent(U8 deviceID, U8 buttonNum, bool pressed);
      void joyButtonEvent(const SDL_Event& event);
      void joyAxisEvent(const SDL_Event& event);
      void joyAxisEvent(U8 deviceID, U8 axisNum, S16 axisValue);
      void mouseButtonEvent(const SDL_Event& event);
      void mouseMotionEvent(const SDL_Event& event);
      void keyEvent(const SDL_Event& event);
      bool processKeyEvent(InputEvent &event);
};

#endif  // _H_X86UNIXINPUTMANAGER_

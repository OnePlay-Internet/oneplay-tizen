#include "moonlight_wasm.hpp"
#include <cmath>
//#include <boost/lexical_cast.hpp>
#include <Limelight.h>

#define KEY_PREFIX 0x80

extern short rightStickXemu;
extern short rightStickYemu;
extern int A_pressed;
extern int B_pressed;
int RightButtonDown = 0;
int LeftButtonDown = 0;

static int ConvertButtonToLiButton(unsigned short button) {
  switch (button) {
    case 0:
      return BUTTON_LEFT;
    case 1:
      return BUTTON_MIDDLE;
    case 2:
      return BUTTON_RIGHT;
    default:
      return 0;
  }
}

static char GetModifierFlags(const EmscriptenKeyboardEvent &event) {
  char flags = 0;

  if (event.ctrlKey == true) {
    flags |= MODIFIER_CTRL;
  }
  if (event.shiftKey == true) {
    flags |= MODIFIER_SHIFT;
  }
  if (event.altKey) {
    flags |= MODIFIER_ALT;
  }

  return flags;
}

EM_BOOL MoonlightInstance::HandleMouseDown(const EmscriptenMouseEvent &event) {
  if (!m_MouseLocked) {
    LockMouse();
    m_MouseLastPosX = event.screenX;
    m_MouseLastPosY = event.screenY;
    return EM_TRUE;
  }
  LiSendMouseButtonEvent(BUTTON_ACTION_PRESS,
                         ConvertButtonToLiButton(event.button));
  return EM_TRUE;
}

EM_BOOL MoonlightInstance::HandleMouseMove(const EmscriptenMouseEvent &event) {
  if (!m_MouseLocked) {
    return EM_FALSE;
  }

  m_MouseDeltaX += event.movementX;
  m_MouseDeltaY += event.movementY;

  m_MouseLastPosX = event.screenX;
  m_MouseLastPosY = event.screenY;

  return EM_TRUE;
}

EM_BOOL MoonlightInstance::HandleMouseUp(const EmscriptenMouseEvent &event) {
  if (!m_MouseLocked) {
    return EM_FALSE;
  }

  LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE,
                         ConvertButtonToLiButton(event.button));
  return EM_TRUE;
}

EM_BOOL MoonlightInstance::HandleWheel(const EmscriptenWheelEvent &event) {
  if (!m_MouseLocked) {
    return EM_FALSE;
  }

  m_AccumulatedTicks += event.deltaY;
  return EM_TRUE;
}

EM_BOOL MoonlightInstance::HandleKeyDown(const EmscriptenKeyboardEvent &event) {
  if (!m_MouseLocked) {
    return EM_FALSE;
  }

  char modifiers = GetModifierFlags(event);
  uint32_t keyCode = event.keyCode;

  if (modifiers == (MODIFIER_ALT | MODIFIER_CTRL | MODIFIER_SHIFT)) {
    if (keyCode == 0x51) {  // Q key
      // Terminate the connection
      StopConnection();
      return EM_TRUE;
    } else {
      // Wait until these keys come up to unlock the mouse
      m_WaitingForAllModifiersUp = true;
    }
  }

  LiSendKeyboardEvent(KEY_PREFIX << 8 | keyCode, KEY_ACTION_DOWN, modifiers);
  return EM_TRUE;
}

EM_BOOL MoonlightInstance::HandleKeyUp(const EmscriptenKeyboardEvent &event) {
  if (!m_MouseLocked) {
    return EM_FALSE;
  }

  char modifiers = GetModifierFlags(event);
  uint32_t keyCode = event.keyCode;

  // Check if all modifiers are up now
  if (m_WaitingForAllModifiersUp && modifiers == 0) {
    UnlockMouse();
    m_WaitingForAllModifiersUp = false;
  }

  LiSendKeyboardEvent(KEY_PREFIX << 8 | keyCode, KEY_ACTION_UP, modifiers);
  return EM_TRUE;
}

EM_BOOL handleKeyDown(int eventType, const EmscriptenKeyboardEvent *event,
                      void *userData) {
  return g_Instance->HandleKeyDown(*event);
}

EM_BOOL handleKeyUp(int eventType, const EmscriptenKeyboardEvent *event,
                    void *userData) {
  return g_Instance->HandleKeyUp(*event);
}

EM_BOOL handleMouseMove(int eventType, const EmscriptenMouseEvent *event,
                        void *userData) {
  return g_Instance->HandleMouseMove(*event);
}

EM_BOOL handleMouseUp(int eventType, const EmscriptenMouseEvent *event,
                      void *userData) {
  return g_Instance->HandleMouseUp(*event);
}

EM_BOOL handleMouseDown(int eventType, const EmscriptenMouseEvent *event,
                        void *userData) {
  return g_Instance->HandleMouseDown(*event);
}

EM_BOOL handleWheel(int eventType, const EmscriptenWheelEvent *event,
                    void *userData) {
  return g_Instance->HandleWheel(*event);
}

EM_BOOL handlePointerLockChange(
    int eventType,
    const EmscriptenPointerlockChangeEvent *pointerlockChangeEvent,
    void *userData) {
  if (!pointerlockChangeEvent)
    return false;

  if (pointerlockChangeEvent->isActive) {
    g_Instance->DidLockMouse(0);
  } else {
    g_Instance->MouseLockLost();
  }
  return true;
}

EM_BOOL handlePointerLockError(int eventType,
                               const void *reserved,
                               void *userData) {
  g_Instance->DidLockMouse(eventType);
  return true;
}

void MoonlightInstance::ReportMouseMovement() {
  if (m_MouseDeltaX != 0 || m_MouseDeltaY != 0) {
    LiSendMouseMoveEvent(m_MouseDeltaX, m_MouseDeltaY);
    m_MouseDeltaX = m_MouseDeltaY = 0;
  }
  if (m_AccumulatedTicks != 0) {
    // We can have fractional ticks here, so multiply by WHEEL_DELTA
    // to get actual scroll distance and use the high-res variant.
    LiSendHighResScrollEvent(m_AccumulatedTicks * 120);
    m_AccumulatedTicks = 0;
  }
}

void MoonlightInstance::sendEmulatedMouseEvent() {

  double vector[2];
  double magnitude = 0;
  
  vector[0] = (double)rightStickXemu;
  vector[1] = (double)rightStickYemu;
  
  vector[0] = vector[0]*(1 / 32766.0f);
  vector[1] = vector[1]*(1 / 32766.0f);
  vector[0] = vector[0]*4;
  vector[1] = vector[1]*4;
  
  magnitude = sqrt((vector[0]*vector[0])+(vector[1]*vector[1]));
	
  if ( magnitude > 0) {
     // Move faster as the stick is pressed further from center
     vector[0] = vector[0]*pow(magnitude,1);
     vector[1] = vector[1]*pow(magnitude,1);
     if (magnitude >= 1) {
       LiSendMouseMoveEvent((short)vector[0], (short)-vector[1]);
     }
  }
}

void MoonlightInstance::sendEmulatedRightCLickMouseEvent(){

  if(B_pressed){
    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS,BUTTON_RIGHT);
    RightButtonDown = 1;
  }
  else if(RightButtonDown && !B_pressed){
    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE,BUTTON_RIGHT);
    RightButtonDown = 0;
  }
}

void MoonlightInstance::sendEmulatedLeftCLickMouseEvent(){

  if(A_pressed){
    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS,BUTTON_LEFT);
    LeftButtonDown = 1;
  }
  else if(LeftButtonDown && !A_pressed){
    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE,BUTTON_LEFT);
    LeftButtonDown = 0;
  }
}

void MoonlightInstance::sendKeycode(std::string stringkeycode, std::string stringmodifiers){
  
  uint32_t keyCode = static_cast<uint32_t>(std::stoul(stringkeycode.c_str(), NULL, 16));
  char modifiers = static_cast<char>(std::stoul(stringmodifiers.c_str(), NULL, 16));
  
  LiSendKeyboardEvent(KEY_PREFIX << 8 | keyCode, KEY_ACTION_DOWN,modifiers);
  LiSendKeyboardEvent(KEY_PREFIX << 8 | keyCode, KEY_ACTION_UP,modifiers);
}

void MoonlightInstance::LockMouse() {
  emscripten_request_pointerlock(kCanvasName, false);
}

void MoonlightInstance::UnlockMouse() {
  emscripten_exit_pointerlock();
}

void MoonlightInstance::DidLockMouse(int32_t result) {
  if (result != 0) {
    ClLogMessage("Error locking mouse, event type: %d\n", result);
  }
  m_MouseLocked = (result == 0);
}

void MoonlightInstance::MouseLockLost() {
    m_MouseLocked = false;
}

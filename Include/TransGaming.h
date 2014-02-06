#pragma once
#ifndef TransGaming_H
#define TransGaming_H

#ifdef _WIN32

/* enum TG_ACTIVATION_STATE: state values passed in to a TGNOTIFY_ACTIVATION
     callback through the <state> parameter.  This is used to identify what
     type of activation event occurred. */
typedef enum
{
    TGAS_NONE,
    TGAS_GAINFOCUS,
    TGAS_LOSEFOCUS,
    TGAS_MINIMIZE,
    TGAS_TOGGLE_WINDOWED,
    TGAS_TOGGLE_FULLSCREEN,
} TG_ACTIVATION_STATE;

/* enum TG_NOTIFY_TYPE: available types of notification callbacks. */
typedef enum
{
    TGNOTIFY_ACTIVATION,

    TGNOTIFY_COUNT,
} TG_NOTIFY_TYPE;

/* type TGNotifyCallback_Func: prototype for the notification callback
     function.  The type of notification (ie: TGNOTIFY_ACTIVATION) is
     passed in through the <type> parameter.  The state of the event 
     (which is dependent on the callback type) is passed in through the
     <state> parameter.  State specific data is passed in through the
     <data> parameter.  An unmodified user context value is passed in
     through the <context> parameter. */
typedef void (WINAPI *TGNotifyCallback_Func)(TG_NOTIFY_TYPE type, DWORD state, int data, void *context);

/* type TGRegisterNotificationCallback_Func: prototype definition for
     the TGRegisterNotificationCallback() function. */
typedef BOOL (WINAPI *TGRegisterNotificationCallback_Func)(TG_NOTIFY_TYPE type, TGNotifyCallback_Func callback, void *context);

// Register for TransGaming event notifications
BLUEIMPORT BOOL TGRegisterForNotifications( TGNotifyCallback_Func, void* context );

// Check if we're running with transgaming
BLUEIMPORT bool IsTransgaming();

#else

#define IsTransgaming() false

#endif

#endif
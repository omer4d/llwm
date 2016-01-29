#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include <malloc.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef struct {
     int type;
     int mods;
     int keycode;
}KeyTrigger;

typedef union {
     int type;
     KeyTrigger key;
}Trigger;

typedef struct CallbackNode_t {
     struct CallbackNode_t* next;
     lua_State* luaState;
     int cbRegIndex;
     Trigger trigger;
}CallbackNode;

typedef struct {
     CallbackNode* first;
}CallbackList;

void die(const char *errstr, ...)
{
     va_list ap;
  
     va_start(ap, errstr);
     vfprintf(stderr, errstr, ap);
     va_end(ap);
     exit(EXIT_FAILURE);
}

int triggersEqual(Trigger* a, Trigger* b)
{
     if(a->type == b->type)
     {
	  switch(a->type)
	  {
	  case KeyPress:
	       return a->key.mods == b->key.mods && a->key.keycode == b->key.keycode;
	  default:
	       return 1;
	  }
     }

     return 0;
}

CallbackNode* createCallbackNode(CallbackNode* next, lua_State* luaState, int cbRegIndex, Trigger trigger)
{
     CallbackNode* node = (CallbackNode*)malloc(sizeof(CallbackNode));
     node->next = next;
     node->luaState = luaState;
     node->cbRegIndex = cbRegIndex;
     node->trigger = trigger;
     return node;
}

void destroyCallbackNode(CallbackNode* node)
{
     luaL_unref(node->luaState, LUA_REGISTRYINDEX, node->cbRegIndex);
     free(node);
}

CallbackList* createCallbackList()
{
     CallbackList* list = (CallbackList*)malloc(sizeof(CallbackList));
     list->first = NULL;
     return list;
}

void destroyCallbackList(CallbackList* list)
{
     CallbackNode* node = list->first;
     
     while(node != NULL)
     {
	  CallbackNode* tmp = node->next;
	  destroyCallbackNode(node);
	  node = tmp;
     }

     free(list);
}

void removeCallback(CallbackList* list, Trigger trigger)
{
     if(list->first != NULL)
     {
	  if(triggersEqual(&list->first->trigger, &trigger))
	  {
	       CallbackNode* n = list->first;
	       list->first = n->next;
	       destroyCallbackNode(n);
	  }
	  
	  else
	  {
	       CallbackNode *prev, *n;
	       for(prev = list->first, n = list->first->next; n != NULL; prev = n, n = n->next)
		    if(triggersEqual(&n->trigger, &trigger))
		    {
			 prev->next = n->next;
			 destroyCallbackNode(n);
			 break;
		    }
	  }
     }
}

void addCallback(CallbackList* list, lua_State* luaState, int regIndex, Trigger trigger)
{
     removeCallback(list, trigger);
     list->first = createCallbackNode(list->first, luaState, regIndex, trigger);
}

void triggerCallback(CallbackList* list, Trigger trigger, Window win)
{
     CallbackNode* node;

     for(node = list->first; node != NULL; node = node->next)
	  if(triggersEqual(&node->trigger, &trigger))
	  {
	       lua_rawgeti(node->luaState, LUA_REGISTRYINDEX, node->cbRegIndex);
	       lua_pcall(node->luaState, 0, 0, 0);
	       break;
	  }
}

int xErrorHandler(Display* disp, XErrorEvent* err)
{
     char buff[512];

     XGetErrorText(disp, err->error_code, buff, 512);
     printf("X error on request type %d: %s", err->request_code, buff);
     
     return 1;
}

Display* dpy;
Window root;
CallbackList* callbacks = NULL;
int quitFlag = 0;

int quit(lua_State* luaState)
{
     quitFlag = 1;
     return 0;
}

int bind(lua_State *luaState)
{
     int mods = luaL_checkinteger(luaState, 1);
     char const* keyname = luaL_checkstring(luaState, 2);
     int keycode = XKeysymToKeycode(dpy, XStringToKeysym(keyname));

     printf("Registering binding for %d+%s (%d)\n", mods, keyname, keycode);
     
     XGrabKey(dpy, keycode, mods, root, True, GrabModeAsync, GrabModeAsync);
     
     Trigger trigger;
     trigger.type = KeyPress;
     trigger.key.mods = mods;
     trigger.key.keycode = keycode;
     
     addCallback(callbacks, luaState, luaL_ref(luaState, LUA_REGISTRYINDEX), trigger);
     return 0;
}

int unbind(lua_State *luaState)
{
     int mods = luaL_checkinteger(luaState, 1);
     char const* keyname = luaL_checkstring(luaState, 2);
     int keycode = XKeysymToKeycode(dpy, XStringToKeysym(keyname));
     
     printf("Unbinding %d+%s (%d)\n", mods, keyname, keycode);
     
     XUngrabKey(dpy, keycode, mods, root);
     
     Trigger trigger;
     trigger.type = KeyPress;
     trigger.key.mods = mods;
     trigger.key.keycode = keycode;
     
     removeCallback(callbacks, trigger);
     return 0;
}

int unbindall(lua_State *luaState)
{
     XUngrabKey(dpy, AnyKey, AnyModifier, root);
     XSync(dpy, False);
     return 0;
}

int subs(lua_State* luaState)
{
     printf("Registering listener...\n");
     
     Trigger trigger;
     trigger.type = luaL_checkinteger(luaState, 1);
     
     addCallback(callbacks, luaState, luaL_ref(luaState, LUA_REGISTRYINDEX), trigger);
     return 0;
}

int unsubs(lua_State* luaState)
{
     printf("Unregistering listener...\n");
     
     Trigger trigger;
     trigger.type = luaL_checkinteger(luaState, 1);
     
     removeCallback(callbacks, trigger);
     return 0;
}

const struct luaL_Reg wmlib [] = {
     {"quit", quit},
     {"subs", subs},
     {"unsubs", unsubs},
     {"bind", bind},
     {"unbind", unbind},
     {"unbindall", unbindall},
     {NULL, NULL}
};

void pushFunc(lua_State* luaState, char const* key, lua_CFunction func)
{
     lua_pushstring(luaState, key);
     lua_pushcfunction(luaState, func);
     lua_settable(luaState, -3);
}

void initLuaAPI(lua_State* luaState)
{
     lua_newtable(luaState);
     luaL_setfuncs(luaState, wmlib, 0);
     lua_setglobal(luaState, "wm");
}

void reloadScript(lua_State* luaState)
{
     char path[512];
     int n;
     
     strcpy(path, getenv("HOME"));
     n = strlen(path);
     strcpy(&path[path[n - 1] == '/' ? (n - 1) : n], "/.config/llwm/script.lua");
     
     if(luaL_dofile(luaState, path))
	  printf("%s\n", lua_tostring(luaState, -1));
     else
	  printf("Script loaded successfully!\n");
}

int main()
{
     int keystate[256];
     XSetWindowAttributes setAttribs;
     XEvent ev;
     Bool dap = False;
     
     callbacks = createCallbackList();
     
     //XWindowAttributes attr;
     //XButtonEvent start;

  
     if(!(dpy = XOpenDisplay(":0")))
	  die("Failed top open display.");

     int i;
     for(i = 0; i < 256; ++i)
	  keystate[i] = 0;

     XSetErrorHandler(xErrorHandler);
     root = XDefaultRootWindow(dpy);
     setAttribs.event_mask = SubstructureNotifyMask;
     XChangeWindowAttributes(dpy, root, CWEventMask, &setAttribs);
     XkbSetDetectableAutoRepeat(dpy, True, &dap);
     XSync(dpy, True);
     
     lua_State *luaState = luaL_newstate();
     luaL_openlibs(luaState);
     initLuaAPI(luaState);

     reloadScript(luaState);

     while(!quitFlag)
     {
	  XNextEvent(dpy, &ev);
	  Trigger trigger;
	  trigger.type = KeyPress;
	  
	  printf("Evt: %d, %d\n", ev.type, dap);

	  if(ev.type == KeyPress)
	  {
	       if(!keystate[ev.xkey.keycode])
	       {
		    keystate[ev.xkey.keycode] = 1;
		    trigger.key.keycode = ev.xkey.keycode;
		    trigger.key.mods = ev.xkey.state;
		    triggerCallback(callbacks, trigger, ev.xany.window);
	       }
	  }

	  else if(ev.type == KeyRelease)
	  {
	       keystate[ev.xkey.keycode] = 0;
	  }

	  else
	       triggerCallback(callbacks, trigger, ev.xany.window);
	  
	  /*
	  if(ev.type == KeyPress && ev.xkey.subwindow != None)
	       XRaiseWindow(dpy, ev.xkey.subwindow);
	  else if(ev.type == ButtonPress && ev.xbutton.subwindow != None)
	  {
	       XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
	       start = ev.xbutton;
	  }
	  else if(ev.type == MotionNotify && start.subwindow != None)
	  {
	       int xdiff = ev.xbutton.x_root - start.x_root;
	       int ydiff = ev.xbutton.y_root - start.y_root;
	       XMoveResizeWindow(dpy, start.subwindow,
				 attr.x + (start.button==1 ? xdiff : 0),
				 attr.y + (start.button==1 ? ydiff : 0),
				 MAX(1, attr.width + (start.button==3 ? xdiff : 0)),
				 MAX(1, attr.height + (start.button==3 ? ydiff : 0)));
	  }
	  else if(ev.type == ButtonRelease)
	  start.subwindow = None;*/
     }

     XUngrabKey(dpy, AnyKey, AnyModifier, root);
     XSync(dpy, False);
     destroyCallbackList(callbacks);
     lua_close(luaState);
     
     return 0;
}

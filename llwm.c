#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include <malloc.h>
#include <string.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

void die(const char *errstr, ...)
{
     va_list ap;
  
     va_start(ap, errstr);
     vfprintf(stderr, errstr, ap);
     va_end(ap);
     exit(EXIT_FAILURE);
}

void debuglog(const char *fmt, ...)
{
     va_list ap;
     
     va_start(ap, fmt);
     vfprintf(stdout, fmt, ap);
     va_end(ap);
}

int xErrorHandler(Display* disp, XErrorEvent* err)
{
     char buff[512];

     XGetErrorText(disp, err->error_code, buff, 512);
     debuglog("X error on request type %d: %s", err->request_code, buff);
     
     return 1;
}

int keyDownCb = -1;
int keyUpCb = -1;
int windowCreatedCb = -1;
int windowDestroyedCb = -1;

Display* dpy;
Window root;
int quitFlag = 0;

void setCallback(int* cb, lua_State* luaState)
{
     if(*cb >= 0)
	  luaL_unref(luaState, LUA_REGISTRYINDEX, *cb);
     *cb = lua_type(luaState, 1) == LUA_TNIL ? -1 : luaL_ref(luaState, LUA_REGISTRYINDEX);
}

static int msghandler(lua_State *L) // Shamelessly stolen from the lua interpreter! :)
{
     const char *msg = lua_tostring(L, 1);
     
     if (msg == NULL)
     {  /* is error object not a string? */
	  if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
	      lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
	       return 1;  /* that is the message */
	  else
	       msg = lua_pushfstring(L, "(error object is a %s value)",
				     luaL_typename(L, 1));
     }
     
     luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
     
     return 1;  /* return the traceback */
}

static int docall(lua_State *L, int narg, int nres) // Shamelessly stolen from the lua interpreter! :)
{
     int status;
     int base = lua_gettop(L) - narg;  /* function index */
     
     lua_pushcfunction(L, msghandler);  /* push message handler */
     lua_insert(L, base);  /* put it under function and args */
     status = lua_pcall(L, narg, nres, base);
     lua_remove(L, base);  /* remove message handler from the stack */
     
     return status;
}

int wmQuit(lua_State* luaState)
{
     quitFlag = 1;
     return 0;
}

int wmGrabKey(lua_State* luaState)
{
     int mods = luaL_checkinteger(luaState, 1);
     char const* keyname = luaL_checkstring(luaState, 2);
     int keycode = XKeysymToKeycode(dpy, XStringToKeysym(keyname));
     
     debuglog("Grabbing %d+%s (%d)\n", mods, keyname, keycode);
     
     XGrabKey(dpy, keycode, mods, root, True, GrabModeAsync, GrabModeAsync);
     
     return 0;
}

int wmReleaseKey(lua_State* luaState)
{
     int mods = luaL_checkinteger(luaState, 1);
     char const* keyname = luaL_checkstring(luaState, 2);
     int keycode = XKeysymToKeycode(dpy, XStringToKeysym(keyname));
     
     debuglog("Releasing %d+%s (%d)\n", mods, keyname, keycode);
     
     XUngrabKey(dpy, keycode, mods, root);

     return 0;
}

int wmReleaseAllKeys(lua_State *luaState)
{
     XUngrabKey(dpy, AnyKey, AnyModifier, root);
     XSync(dpy, False);
     return 0;
}

int wmGrabKeyboard(lua_State* luaState)
{     
     XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);    
     return 0;
}

int wmReleaseKeyboard(lua_State* luaState)
{
     XUngrabKeyboard(dpy, CurrentTime);
     return 0;
}


int wmOnKeyDown(lua_State* luaState)
{
     setCallback(&keyDownCb, luaState);
     return 0;
}

int wmOnKeyUp(lua_State* luaState)
{
     setCallback(&keyUpCb, luaState);
     return 0;
}

int wmOnWindowCreated(lua_State* luaState)
{
     setCallback(&windowCreatedCb, luaState);
     return 0;
}

int wmOnWindowDestroyed(lua_State* luaState)
{
     setCallback(&windowDestroyedCb, luaState);
     return 0;
}

int wmSetWindowFrame(lua_State* luaState)
{
     int mods = luaL_checkinteger(luaState, 1);
     
     XMoveResizeWindow(dpy, luaL_checkinteger(luaState, 1),
		       luaL_checkinteger(luaState, 2),
		       luaL_checkinteger(luaState, 3),
		       luaL_checkinteger(luaState, 4),
		       luaL_checkinteger(luaState, 5));
     
     return 0;
}

const struct luaL_Reg wmlib [] = {
     {"quit", wmQuit},
     {"grabKey", wmGrabKey},
     {"releaseKey", wmReleaseKey},
     {"releaseAllKeys", wmReleaseAllKeys},
     {"grabKeyboard", wmGrabKeyboard},
     {"releaseKeyboard", wmReleaseKeyboard},
     {"onKeyDown", wmOnKeyDown},
     {"onKeyUp", wmOnKeyUp},
     {"onWindowCreated", wmOnWindowCreated},
     {"onWindowDestroyed", wmOnWindowDestroyed},
     {"setWindowFrame", wmSetWindowFrame},
     {NULL, NULL}
};

void initLuaAPI(lua_State* luaState)
{
     lua_newtable(luaState);
     luaL_setfuncs(luaState, wmlib, 0);
     lua_setglobal(luaState, "wm");
}

void reloadScript(lua_State* luaState)
{
     /*
     char path[512];
     int n;
     
     strcpy(path, getenv("HOME"));
     n = strlen(path);
     strcpy(&path[path[n - 1] == '/' ? (n - 1) : n], "/.config/llwm/script.lua");*/

     char const* path = "default.lua";
     
     if(luaL_dofile(luaState, path))
	  printf("%s\n", lua_tostring(luaState, -1));
     else
	  printf("Script loaded successfully!\n");
}

void cleanupX()
{
     XUngrabKey(dpy, AnyKey, AnyModifier, root);
     XSync(dpy, False);
}

int main()
{
     XSetWindowAttributes setAttribs;
     XEvent ev;
  
     if(!(dpy = XOpenDisplay(":1")))
	  die("Failed top open display.");
     
     XSetErrorHandler(xErrorHandler);
     root = XDefaultRootWindow(dpy);
     setAttribs.event_mask = SubstructureNotifyMask | SubstructureRedirectMask;
     XChangeWindowAttributes(dpy, root, CWEventMask, &setAttribs);
     XkbSetDetectableAutoRepeat(dpy, True, None);
     XSync(dpy, True);
     
     lua_State *luaState = luaL_newstate();
     luaL_openlibs(luaState);
     initLuaAPI(luaState);
     
     reloadScript(luaState);
     
     while(!quitFlag)
     {
	  XNextEvent(dpy, &ev);
	  
	  //debuglog("Evt: %d\n", ev.type);

	  switch(ev.type)
	  {
	  case MapRequest:
	       XMapWindow(dpy, ev.xmaprequest.window);
	       break;
	       
	  case KeyPress:
	       if(keyDownCb >= 0)
	       {
		    lua_rawgeti(luaState, LUA_REGISTRYINDEX, keyDownCb);
		    lua_pushnumber(luaState, ev.xany.window);
		    lua_pushnumber(luaState, ev.xkey.state);
		    lua_pushstring(luaState, XKeysymToString(XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0)));	       
		    if(lua_pcall(luaState, 3, 0, 0))
			 printf("%s\n", lua_tostring(luaState, -1));
	       }
	       break;

	  case KeyRelease:
	       if(keyUpCb >= 0)
	       {
		    lua_rawgeti(luaState, LUA_REGISTRYINDEX, keyUpCb);
		    lua_pushnumber(luaState, ev.xany.window);
		    lua_pushnumber(luaState, ev.xkey.state);
		    lua_pushstring(luaState, XKeysymToString(XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0)));	       
		    if(lua_pcall(luaState, 3, 0, 0))
			 printf("%s\n", lua_tostring(luaState, -1));
	       }
	       break;
	       
	  case CreateNotify:
	       if(windowCreatedCb >= 0)
	       {
		    lua_rawgeti(luaState, LUA_REGISTRYINDEX, windowCreatedCb);
		    lua_pushnumber(luaState, ev.xcreatewindow.window);
		    if(docall(luaState, 1, 0))
			 printf("%s\n", lua_tostring(luaState, -1));
	       }
	       break;

	  case DestroyNotify:
	       if(windowDestroyedCb >= 0)
	       {
		    lua_rawgeti(luaState, LUA_REGISTRYINDEX, windowDestroyedCb);
		    lua_pushnumber(luaState, ev.xdestroywindow.window);	       
		    if(lua_pcall(luaState, 1, 0, 0))
			 printf("%s\n", lua_tostring(luaState, -1));
	       }
	       break;
	  default:
	       break;
	  }
     }
     
     cleanupX();
     lua_close(luaState);
     
     return 0;
}

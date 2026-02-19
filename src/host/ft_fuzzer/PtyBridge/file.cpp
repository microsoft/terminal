#include "PtyBridge.h"
int WINAPI PtyRead(int arg0, void * arg1, int arg2) { return -1; }
int WINAPI PtyWrite(int arg0, void * arg1, int arg2) { return -1; }
int WINAPI OpenPty(int* pmaster, int* pslave, int width, int height) { return -1; }
int WINAPI ResizePty(int pmaster, int width, int height) { return -1; }
int WINAPI SpawnChild(int pmaster, int pslave, const char* child) { return -1; }

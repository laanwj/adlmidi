#ifndef H_UI
#define H_UI

#include "config.hh"

class UI
{
public:
  #ifdef __WIN32__
    void* handle;
  #endif
    int x, y, color, txtline, maxy;
    char background[MaxWidth][MaxHeight];
    char slots[MaxWidth][MaxHeight];
    unsigned char slotcolors[MaxWidth][MaxHeight];
    short curpatch[MaxHeight];
    short curins[MaxHeight];
    bool cursor_visible;
public:
    UI();
    void HideCursor();
    void ShowCursor();
    void PrintLn(const char* fmt, ...) __attribute__((format(printf,2,3)));
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend);
    void Draw(int notex,int notey, int color, char ch);
    void IllustrateVolumes(double left, double right);
    void IllustratePatchChange(int MidCh, int patch, int adlinsid);
    // Move tty cursor to the indicated position.
    // Movements will be done in relative terms
    // to the current cursor position only.
    void GotoXY(int newx, int newy);
    // Set color (4-bit). Bits: 1=blue, 2=green, 4=red, 8=+intensity
    void Color(int newcolor);
    // Choose a permanent color for given instrument
    int AllocateColor(int ins);
    // Cleanup before exit
    void Cleanup();
};

extern class UI UI;

#endif


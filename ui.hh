#ifndef H_UI
#define H_UI

#include "config.hh"

/* Console backend */
class ConsoleInterface
{
public:
    virtual ~ConsoleInterface();
    /* Print status message (use only before calling Draw) */
    virtual void InitMessage(int color, const char *message, int nchars) = 0;
    /* Draw an arbitrary character in an arbitrary position */
    virtual void Draw(int notex, int notey, int color, char ch) = 0;
    /* Flush output after draws */
    virtual void Flush() = 0;
};

class UI
{
private:
    ConsoleInterface *console;
public:
    int txtline;
    char background[MaxWidth][MaxHeight];
    char foreground[MaxWidth][MaxHeight];
    short curpatch[MaxHeight];
    short curins[MaxHeight];
public:
    UI();
    ~UI();
    void HideCursor();
    void ShowCursor();
    void PrintLn(const char* fmt, ...) __attribute__((format(printf,2,3)));
    void InitMessage(int color, const char *fmt, ...) __attribute__((format(printf,3,4)));
    void InitDone();
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend);
    void IllustrateVolumes(double left, double right);
    void IllustratePatchChange(int MidCh, int patch, int adlinsid);
    // Move tty cursor to the indicated position.
    // Movements will be done in relative terms
    // to the current cursor position only.
    void GotoXY(int newx, int newy);
    // Choose a permanent color for given instrument
    int AllocateColor(int ins);
    // Cleanup before exit
    void Cleanup();
};

extern class UI UI;

#endif


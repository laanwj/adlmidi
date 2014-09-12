#ifndef H_UI
#define H_UI

#include "config.hh"
#include "uiinterface.hh"

/* Console backend */
class ConsoleInterface
{
public:
    virtual ~ConsoleInterface() = 0;
    /* Create the grid. Requests a certain width and height, and returns
     * the actual width and height.
     */
    virtual void CreateGrid(int width, int height, int *out_width, int *out_height) = 0;
    /* Draw an arbitrary character in an arbitrary position */
    virtual void Draw(int notex, int notey, int color, char ch) = 0;
    /* Flush output after draws */
    virtual void Flush() = 0;
};

class UI: public UIInterface
{
private:
    ConsoleInterface *console;

    int width, height;
    int txtline;
    char background[MaxWidth][MaxHeight];
    char foreground[MaxWidth][MaxHeight];
    short curpatch[MaxHeight];
    short curins[MaxHeight];

    // Choose a permanent color for given instrument
    int AllocateColor(int ins);
public:
    UI();
    ~UI();
    void PrintLn(const char* fmt, ...) __attribute__((format(printf,2,3)));
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend);
    void IllustrateVolumes(double left, double right);
    void IllustratePatchChange(int MidCh, int patch, int adlinsid);
};

void InitMessage(int color, const char *fmt, ...) __attribute__((format(printf,2,3)));

#endif


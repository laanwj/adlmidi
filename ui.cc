#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <stdarg.h>
#include <cstdio>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil

#include <assert.h>

#if !defined(__WIN32__) || defined(__CYGWIN__)
# include <termio.h>
# include <fcntl.h>
# include <sys/ioctl.h>
#endif

#include <deque>
#include <algorithm>

#include <signal.h>

#include "ui.hh"

static unsigned WinHeight()
{
    if(AdlPercussionMode)
        return std::min(2u, NumCards) * 23;
    return std::min(3u, NumCards) * 18;
}

#include "adldata.hh"

static const char MIDIsymbols[256+1] =
"PPPPPPhcckmvmxbd"  // Ins  0-15
"oooooahoGGGGGGGG"  // Ins 16-31
"BBBBBBBBVVVVVHHM"  // Ins 32-47
"SSSSOOOcTTTTTTTT"  // Ins 48-63
"XXXXTTTFFFFFFFFF"  // Ins 64-79
"LLLLLLLLpppppppp"  // Ins 80-95
"XXXXXXXXGGGGGTSS"  // Ins 96-111
"bbbbMMMcGXXXXXXX"  // Ins 112-127
"????????????????"  // Prc 0-15
"????????????????"  // Prc 16-31
"???DDshMhhhCCCbM"  // Prc 32-47
"CBDMMDDDMMDDDDDD"  // Prc 48-63
"DDDDDDDDDDDDDDDD"  // Prc 64-79
"DD??????????????"  // Prc 80-95
"????????????????"  // Prc 96-111
"????????????????"; // Prc 112-127

UI::UI(): x(0), y(0), color(-1), txtline(1),
      maxy(0), cursor_visible(true)
{
  #ifdef __WIN32__
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GotoXY(41,13);
    CONSOLE_SCREEN_BUFFER_INFO tmp;
    GetConsoleScreenBufferInfo(handle,&tmp);
    if(tmp.dwCursorPosition.X != 41)
    {
        // Console is not obeying controls! Probably cygwin xterm.
        handle = 0;
    }
    else
    {
        //COORD size = { 80, 23*NumCards+5 };
        //SetConsoleScreenBufferSize(handle,size);
    }
  #endif
    std::memset(slots, '.',      sizeof(slots));
    std::memset(background, '.', sizeof(background));
    std::fputc('\r', stderr); // Ensure cursor is at x=0
    GotoXY(0,0); Color(15);
    prn("Hit Ctrl-C to quit\r");
}
void UI::HideCursor()
{
    if(!cursor_visible) return;
    cursor_visible = false;
  #ifdef __WIN32__
    if(handle)
    {
      const CONSOLE_CURSOR_INFO info = {100,false};
      SetConsoleCursorInfo(handle,&info);
      return;
    }
  #endif
    prn("\33[?25l"); // hide cursor
}
void UI::ShowCursor()
{
    if(cursor_visible) return;
    cursor_visible = true;
    GotoXY(0,maxy); Color(7);
  #ifdef __WIN32__
    if(handle)
    {
      const CONSOLE_CURSOR_INFO info = {100,true};
      SetConsoleCursorInfo(handle,&info);
      return;
    }
  #endif
    prn("\33[?25h"); // show cursor
    std::fflush(stderr);
}
void UI::PrintLn(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char Line[512];
  #ifndef __CYGWIN__
    int nchars = vsnprintf(Line, sizeof(Line), fmt, ap);
  #else
    int nchars = std::vsprintf(Line, fmt, ap); /* SECURITY: POSSIBLE BUFFER OVERFLOW */
  #endif
    va_end(ap);

    if(nchars == 0) return;

    const int beginx = 2;

    HideCursor();
    GotoXY(beginx,txtline);
    for(x=beginx; x-beginx<nchars && x < 80; ++x)
    {
        if(Line[x-beginx] == '\n') break;
        Color(Line[x-beginx] == '.' ? 1 : 8);
        std::fputc( background[x][txtline] = Line[x-beginx], stderr);
    }
    for(int tx=x; tx<80; ++tx)
    {
        if(background[tx][txtline]!='.' && slots[tx][txtline]=='.')
        {
            GotoXY(tx,txtline);
            Color(1);
            std::fputc(background[tx][txtline] = '.', stderr);
            ++x;
        }
    }
    std::fflush(stderr);

    txtline=(1 + txtline) % WinHeight();
}
void UI::IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
{
    HideCursor();
    int notex = 2 + (note+55)%77;
    int notey = 1 + adlchn % WinHeight();
    char illustrate_char = background[notex][notey];
    if(pressure > 0)
    {
        illustrate_char = MIDIsymbols[ins];
        if(bend < 0) illustrate_char = '<';
        if(bend > 0) illustrate_char = '>';
    }
    else if(pressure < 0)
    {
        illustrate_char = '%';
    }
    Draw(notex,notey,
        pressure?AllocateColor(ins):
        (illustrate_char=='.'?1:
         illustrate_char=='&'?1: 8),
        illustrate_char);
}

void UI::Draw(int notex,int notey, int color, char ch)
{
    if(slots[notex][notey] != ch
    || slotcolors[notex][notey] != color)
    {
        slots[notex][notey] = ch;
        slotcolors[notex][notey] = color;
        GotoXY(notex, notey);
        Color(color);
    #ifdef __WIN32__
        if(handle) WriteConsole(handle,&ch,1, 0,0);
        else
    #endif
        {
          std::fputc(ch, stderr);
          std::fflush(stderr);
        }
        ++x;
    }
}

void UI::IllustrateVolumes(double left, double right)
{
    const unsigned maxy = WinHeight();
    const unsigned white_threshold  = maxy/23;
    const unsigned red_threshold    = maxy*4/23;
    const unsigned yellow_threshold = maxy*8/23;

    double amp[2] = {left*maxy, right*maxy};
    for(unsigned y=0; y<maxy; ++y)
        for(unsigned w=0; w<2; ++w)
        {
            char c = amp[w] > (maxy-1)-y ? '|' : background[w][y+1];
            Draw(w,y+1,
                 c=='|' ? y<white_threshold ? 15
                        : y<red_threshold ? 12
                        : y<yellow_threshold ? 14
                        : 10 : (c=='.' ? 1 : 8),
                 c);
        }
}

// Move tty cursor to the indicated position.
// Movements will be done in relative terms
// to the current cursor position only.
void UI::GotoXY(int newx, int newy)
{
    if(newy > maxy) maxy = newy;
    while(newy > y)
    {
        std::fputc('\n', stderr); y+=1; x=0;
    }
  #ifdef __WIN32__
    if(handle)
    {
      CONSOLE_SCREEN_BUFFER_INFO tmp;
      GetConsoleScreenBufferInfo(handle, &tmp);
      COORD tmp2 = { x = newx, tmp.dwCursorPosition.Y } ;
      if(newy < y) { tmp2.Y -= (y-newy); y = newy; }
      SetConsoleCursorPosition(handle, tmp2);
    }
  #endif
    if(newy < y) { prn("\33[%dA", y-newy); y = newy; }
    if(newx != x)
    {
        if(newx == 0 || (newx<10 && std::abs(newx-x)>=10))
            { std::fputc('\r', stderr); x = 0; }
        if(newx < x) prn("\33[%dD", x-newx);
        if(newx > x) prn("\33[%dC", newx-x);
        x = newx;
    }
}
// Set color (4-bit). Bits: 1=blue, 2=green, 4=red, 8=+intensity
void UI::Color(int newcolor)
{
    if(color != newcolor)
    {
      #ifdef __WIN32__
        if(handle)
          SetConsoleTextAttribute(handle, newcolor);
        else
      #endif
        {
          static const char map[8+1] = "04261537";
          prn("\33[0;%s3%c",
              (newcolor&8) ? "1;" : "", map[newcolor&7]);
          // If xterm-256color is used, try using improved colors:
          //        Translate 8 (dark gray) into #003366 (bluish dark cyan)
          //        Translate 1 (dark blue) into #000033 (darker blue)
          if(newcolor==8) prn(";38;5;24;25");
          if(newcolor==1) prn(";38;5;17;25");
          std::fputc('m', stderr);
        }
        color=newcolor;
    }
}
// Choose a permanent color for given instrument
int UI::AllocateColor(int ins)
{
    static char ins_colors[256] = { 0 }, ins_color_counter = 0;
    if(ins_colors[ins])
        return ins_colors[ins];
    if(ins & 0x80)
    {
        static const char shuffle[] = {2,3,4,5,6,7};
        return ins_colors[ins] = shuffle[ins_color_counter++ % 6];
    }
    else
    {
        static const char shuffle[] = {10,11,12,13,14,15};
        return ins_colors[ins] = shuffle[ins_color_counter++ % 6];
    }
}

void UI::prn(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void UI::Cleanup()
{
    ShowCursor();
#ifdef __WIN32__
    Color(7);
#else
    prn("\33[0m");
#endif
    std::fflush(stderr);
}

class UI UI;


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

#include <QApplication>
#include <QWidget>

#include "ui.hh"

static QApplication *app;

class QtConsoleInterface: public ConsoleInterface
{
public:
    QtConsoleInterface():
      x(0), y(0), color(-1),
      maxy(0), cursor_visible(true),
      screen_mode(false)
    {
        std::memset(buffer, '.',    sizeof(buffer));
        std::memset(colors, 0,      sizeof(colors));

        int argc=0;
        app = new QApplication(argc, 0);
    }

    ~QtConsoleInterface()
    {
        ShowCursor();
        std::fputs("\33[0m", stderr);
        std::fflush(stderr);
    }

    void InitMessage(int color, const char *message, int nchars)
    {
        if(screen_mode)
            return;
        if(color != -1)
            Color(color);
        std::fwrite(message, nchars, 1, stderr);
        std::fputs("\33[0m", stderr);
        std::fflush(stderr);
    }

    void CreateGrid(int width, int height, int *out_width, int *out_height)
    {
        screen_mode = true;

        std::fputc('\r', stderr); // Ensure cursor is at x=0
        GotoXY(0,0); Color(15);
        std::fputs("Hit Ctrl-C to quit\r", stderr);
        HideCursor();

        *out_width = width;
        *out_height = height;
    }

    void Draw(int notex,int notey, int color, char ch)
    {
        if(buffer[notex][notey] != ch
        || colors[notex][notey] != color)
        {
            buffer[notex][notey] = ch;
            colors[notex][notey] = color;
            GotoXY(notex, notey);
            Color(color);
            std::fputc(ch, stderr);
            ++x;
        }
    }

    void Flush()
    {
        std::fflush(stderr);
    }

private:
    int x, y, color, maxy;
    char buffer[MaxWidth][MaxHeight];
    unsigned char colors[MaxWidth][MaxHeight];
    bool cursor_visible;
    bool screen_mode;

    void HideCursor()
    {
        if(!cursor_visible) return;
        cursor_visible = false;
        std::fputs("\33[?25l", stderr); // hide cursor
    }

    void ShowCursor()
    {
        if(cursor_visible) return;
        cursor_visible = true;
        GotoXY(0,maxy); Color(7);
        std::fputs("\33[?25h", stderr); // show cursor
        std::fflush(stderr);
    }

    // Move tty cursor to the indicated position.
    // Movements will be done in relative terms
    // to the current cursor position only.
    void GotoXY(int newx, int newy)
    {
        if(newy > maxy || (newy > y && (newy-y)<4 && newx == 0))
        {
            while(newy > y)
            {
                std::fputc('\n', stderr); y+=1; x=0;
            }
        }
        if(newy > maxy)
            maxy = newy;
        if(newy < y) { std::fprintf(stderr, "\33[%dA", y-newy); y = newy; }
        if(newy > y) { std::fprintf(stderr, "\33[%dB", newy-y); y = newy; }
        if(newx != x)
        {
            if(newx == 0 || (newx<10 && std::abs(newx-x)>=10))
                { std::fputc('\r', stderr); x = 0; }
            if(newx < x) std::fprintf(stderr, "\33[%dD", x-newx);
            if(newx > x) std::fprintf(stderr, "\33[%dC", newx-x);
            x = newx;
        }
    }

    // Set color (4-bit or 8-bit). Bits: 1=blue, 2=green, 4=red, 8=+intensity
    void Color(int newcolor)
    {
        if(color != newcolor)
        {
            if(newcolor<16)
            {
                static const char map[16] = {0,17,2,6,1,5,3,7,24,12,10,14,9,13,11,15};
                std::fprintf(stderr, "\33[0;38;5;%im", map[newcolor]);
            }
            else
            {
                std::fprintf(stderr, "\33[0;38;5;%im", newcolor);
            }
            color = newcolor;
        }
    }
};

ConsoleInterface *CreateQtConsole()
{
    return new QtConsoleInterface();
}


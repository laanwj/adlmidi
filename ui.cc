#include "ui.hh"

#include <cstring>
#include <stdarg.h>
#include <string>
#include <vector>

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

#include "midi_symbols_256.hh"

UIInterface::~UIInterface() { }

class UnixTerminalConsoleInterface: public ConsoleInterface
{
public:
    UnixTerminalConsoleInterface():
      x(0), y(0), color(-1),
      maxy(0), cursor_visible(true)
    {
        std::memset(slots, '.',      sizeof(slots));
    }

    ~UnixTerminalConsoleInterface()
    {
        ShowCursor();
        std::fputs("\33[0m", stderr);
        std::fflush(stderr);
    }

    void CreateGrid(int width, int height, int *out_width, int *out_height)
    {
        std::fputc('\r', stderr); // Ensure cursor is at x=0
        GotoXY(0,0); Color(15);
        std::fputs("Hit Ctrl-C to quit\r", stderr);
        HideCursor();

        *out_width = width;
        *out_height = height;
    }

    void Draw(int notex,int notey, int color, char ch)
    {
        if(slots[notex][notey] != ch
        || slotcolors[notex][notey] != color)
        {
            slots[notex][notey] = ch;
            slotcolors[notex][notey] = color;
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
    char slots[MaxWidth][MaxHeight];
    unsigned char slotcolors[MaxWidth][MaxHeight];
    bool cursor_visible;

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

ConsoleInterface::~ConsoleInterface() {}

UI::UI():
    width(0), height(0),
    txtline(1)
{
    std::memset(background, '.', sizeof(background));
    std::memset(foreground, '.', sizeof(foreground));
    console = new UnixTerminalConsoleInterface();
    for(unsigned ch=0; ch<MaxHeight; ++ch)
    {
        curpatch[ch] = -1;
        curins[ch] = -1;
    }

    int req_lines;
    if(AdlPercussionMode)
        req_lines = std::min(2u, NumCards) * 23;
    else
        req_lines = std::min(3u, NumCards) * 18;
    console->CreateGrid(MaxWidth, req_lines, &width, &height);
}

UI::~UI()
{
    delete console;
}
void UI::PrintLn(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char Line[512];
    int nchars = vsnprintf(Line, sizeof(Line), fmt, ap);
    va_end(ap);

    if(nchars <= 0) return;
    if((size_t)nchars > (sizeof(Line)-1))
        nchars = sizeof(Line)-1;

    const int beginx = 2;
    int x;
    for(x=beginx; x-beginx<nchars && x < 80; ++x)
    {
        if(Line[x-beginx] == '\n') break;
        console->Draw(x, txtline, Line[x-beginx] == '.' ? 1 : 8, background[x][txtline] = Line[x-beginx]);
    }
    for(int tx=x; tx<80; ++tx)
    {
        if(background[tx][txtline]!='.' && foreground[tx][txtline]=='.')
        {
            console->Draw(tx, txtline, 1, background[tx][txtline] = '.');
            ++x;
        }
    }

    txtline=(1 + txtline) % height;
}

void InitMessage(int color, const char *fmt, ...)
{
    va_list ap;
    if(color != -1)
    {
        static const char map[16] = {0,17,2,6,1,5,3,7,24,12,10,14,9,13,11,15};
        std::fprintf(stderr, "\33[0;38;5;%im", map[color]);
    }
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fputs("\33[0m", stderr);
    std::fflush(stderr);
}

void UI::IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
{
    // If not in percussion mode the lower 5 channels are not use, so use 18 lines per chip instead of 23
    if(!AdlPercussionMode)
        adlchn = (adlchn / 23) * 18 + (adlchn % 23);
    int notex = 2 + (note+55)%77;
    int notey = 1 + adlchn % height;
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
    console->Draw(notex,notey,
        pressure?AllocateColor(ins):
        (illustrate_char=='.'?1:
         illustrate_char=='&'?1: 8),
        illustrate_char);
    console->Flush();
    foreground[notex][notey] = illustrate_char;
}

void UI::IllustrateVolumes(double left, double right)
{
    const unsigned maxy = height;
    const unsigned white_threshold  = maxy/23;
    const unsigned red_threshold    = maxy*4/23;
    const unsigned yellow_threshold = maxy*8/23;

    double amp[2] = {left*maxy, right*maxy};
    for(unsigned y=0; y<maxy; ++y)
        for(unsigned w=0; w<2; ++w)
        {
            char c = amp[w] > (maxy-1)-y ? '|' : background[w][y+1];
            console->Draw(w,y+1,
                 c=='|' ? y<white_threshold ? 15
                        : y<red_threshold ? 12
                        : y<yellow_threshold ? 14
                        : 10 : (c=='.' ? 1 : 8),
                 c);
        }
    console->Flush();
}

void UI::IllustratePatchChange(int MidCh, int patch, int adlinsid)
{
    if(MidCh < 0 || MidCh >= height || (patch != -1 && curpatch[MidCh] == patch && curins[MidCh] == adlinsid))
        return;
    curpatch[MidCh] = patch;
    curins[MidCh] = adlinsid;
    // 8 or 9 or 11
    console->Draw(81,MidCh+1, 8, '0' + (MidCh / 10));
    console->Draw(82,MidCh+1, 8, '0' + (MidCh % 10));
    const int name_column = 92;
    if(patch == -1)
    {
        console->Draw(84,MidCh+1, 1, '-');
        console->Draw(85,MidCh+1, 1, '-');
        console->Draw(86,MidCh+1, 1, '-');
        console->Draw(88,MidCh+1, 1, '-');
        console->Draw(89,MidCh+1, 1, '-');
        console->Draw(90,MidCh+1, 1, '-');
        for(unsigned x=name_column; x<MaxWidth; ++x)
            console->Draw(x,MidCh+1, 1, ' ');
    }
    else
    {
        console->Draw(84,MidCh+1, 8, '0' + ((patch/100) % 10));
        console->Draw(85,MidCh+1, 8, '0' + ((patch/10) % 10));
        console->Draw(86,MidCh+1, 8, '0' + ((patch) % 10));
        console->Draw(88,MidCh+1, 1, '[');
        console->Draw(89,MidCh+1, AllocateColor(patch), MIDIsymbols[patch]);
        console->Draw(90,MidCh+1, 1, ']');
        std::string name = "[unnamed]";
        int color = 1;
        if(adlinsid >= 0)
        {
            if(adlins[adlinsid].name)
                name = adlins[adlinsid].name;
            /// Set color based on flags
            if(!(adlins[adlinsid].flags & adlinsdata::Flag_NoSound))
            {
                if(adlins[adlinsid].adlno1 != adlins[adlinsid].adlno2)
                    if(adlins[adlinsid].flags & adlinsdata::Flag_Pseudo4op)
                        color = 9; // pseudo-4-op
                    else
                        color = 11; // 4-op
                else
                    color = 8; // 2-op
            }
        }
        for(unsigned x=name_column; x<MaxWidth-1; ++x)
        {
            if((x-name_column) < name.size())
                console->Draw(x,MidCh+1, color, name[x-name_column]);
            else
                console->Draw(x,MidCh+1, color, ' ');
        }
    }
    console->Flush();
}

// Choose a permanent color for given instrument
int UI::AllocateColor(int ins)
{
    return MIDIcolors256[ins];
#if 0
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
#endif
}


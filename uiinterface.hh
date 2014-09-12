#ifndef H_UIINTERFACE
#define H_UIINTERFACE

/* UI interface */
class UIInterface
{
public:
    virtual ~UIInterface() = 0;

    virtual void PrintLn(const char* fmt, ...) __attribute__((format(printf,2,3))) = 0;
    virtual void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend) = 0;
    virtual void IllustrateVolumes(double left, double right) = 0;
    virtual void IllustratePatchChange(int MidCh, int patch, int adlinsid) = 0;
};

#endif

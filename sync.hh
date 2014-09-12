#ifndef H_SYNC
#define H_SYNC

# include <SDL.h>
class MutexType
{
    SDL_mutex* mut;
public:
    MutexType() : mut(SDL_CreateMutex()) { }
    ~MutexType() { SDL_DestroyMutex(mut); }
    void Lock() { SDL_mutexP(mut); }
    void Unlock() { SDL_mutexV(mut); }

    friend class CondType;
};
class CondType
{
    SDL_cond* cond;
public:
    CondType() : cond(SDL_CreateCond()) { }
    ~CondType() { SDL_DestroyCond(cond); }
    void Signal() { SDL_CondSignal(cond); }
    void Wait(MutexType &mutex) { SDL_CondWait(cond, mutex.mut); }
};

#endif

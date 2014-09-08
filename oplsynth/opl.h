#ifndef OPL_H
#define OPL_H

// Abstract base class for OPL emulators

class OPLEmul
{
public:
	OPLEmul() {}
	virtual ~OPLEmul() {}

	virtual void Reset() = 0;
	virtual void WriteReg(int reg, int v) = 0;
	virtual void Update(float *buffer, int length) = 0;
	virtual void SetPanning(int c, float left, float right) = 0;
};

OPLEmul *DBOPLCreate(bool stereo);
OPLEmul *JavaOPLCreate(bool stereo);
OPLEmul *DBOPLv2Create(bool fullpan);
OPLEmul *YMF262Create(bool fullpan);

#define OPL_SAMPLE_RATE			((double)PCM_RATE)
#define CENTER_PANNING_POWER	0.70710678118	/* [RH] volume at center for EQP */

#endif

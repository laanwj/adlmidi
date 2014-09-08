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

OPLEmul *DBOPLCreate(unsigned int sample_rate, bool stereo);
OPLEmul *JavaOPLCreate(unsigned int sample_rate, bool stereo);
OPLEmul *DBOPLv2Create(unsigned int sample_rate, bool fullpan);
OPLEmul *YMF262Create(unsigned int sample_rate, bool fullpan);

#define CENTER_PANNING_POWER	0.70710678118	/* [RH] volume at center for EQP */

#endif

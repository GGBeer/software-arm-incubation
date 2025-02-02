/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#ifndef DUMMYPIN_H_
#define DUMMYPIN_H_

#include <GenericPin.h>

class DummyPin : public GenericPin
{
public:
	DummyPin() : GenericPin(NULL, 0) {};
	virtual int ConfigLength() { return 0; };
	virtual int ComObjCount() { return 0; };
};

#endif /* DUMMYPIN_H_ */

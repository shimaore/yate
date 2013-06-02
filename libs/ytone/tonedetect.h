/**
 * tonedetect.h
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * Detectors for various tones
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2004-2006 Null Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef __TONEDETECT_H
#define __TONEDETECT_H

#include <yatephone.h>

#include <math.h>

using namespace TelEngine;

// remember the values below are squares, we compute in power, not amplitude

// how much we keep from old value when averaging, must be below 1
#define MOVING_AVG_KEEP     0.97
// minimum square of signal energy to even consider detecting
#define THRESHOLD2_ABS     1e+06
// relative square of spectral power from total signal power
#define THRESHOLD2_REL_FAX  0.95
// same for continuity test tones
#define THRESHOLD2_REL_COT  0.90
// sum of tones (low+high) from total
#define THRESHOLD2_REL_ALL  0.60
// each tone from threshold from total
#define THRESHOLD2_REL_DTMF 0.33
// hysteresis after tone detection
#define THRESHOLD2_REL_HIST 0.75

// minimum DTMF detect time
#define DETECT_DTMF_MSEC 32

// 2-pole filter parameters
typedef struct
{
    double gain;
    double y0;
    double y1;
} Params2Pole;

// Half 2-pole filter - the other part is common to all filters
class Tone2PoleFilter
{
public:
    inline Tone2PoleFilter()
	: m_mult(0.0), m_y0(0.0), m_y1(0.0)
	{ }
    inline Tone2PoleFilter(double gain, double y0, double y1)
	: m_mult(1.0/gain), m_y0(y0), m_y1(y1)
	{ init(); }
    inline Tone2PoleFilter(const Params2Pole& params)
	: m_mult(1.0/params.gain), m_y0(params.y0), m_y1(params.y1)
	{ init(); }
    inline void assign(const Params2Pole& params)
	{ m_mult = 1.0/params.gain; m_y0 = params.y0; m_y1 = params.y1; init(); }
    inline void init()
	{ m_val = m_y[1] = m_y[2] = 0.0; }
    inline double value() const
	{ return m_val; }
    void update(double xd);
private:
    double m_mult;
    double m_y0;
    double m_y1;
    double m_val;
    double m_y[3];
};

class ToneConsumer : public DataConsumer
{
    YCLASS(ToneConsumer,DataConsumer)
public:
    enum Mode {
	Mono = 0,
	Left,
	Right,
	Mixed
    };
    ToneConsumer(const String& id, const String& name);
    virtual ~ToneConsumer(); 
    virtual unsigned long Consume(const DataBlock& data, unsigned long tStamp, unsigned long flags);
    virtual const String& toString() const
	{ return m_name; }
    inline const String& id() const
	{ return m_id; }
    void setFaxDivert(const Message& msg);
    void init();
private:
    void checkDtmf();
    void checkFax();
    void checkCont();
    String m_id;
    String m_name;
    String m_faxDivert;
    String m_faxCaller;
    String m_faxCalled;
    String m_target;
    String m_dnis;
    Mode m_mode;
    bool m_detFax;
    bool m_detCont;
    bool m_detDtmf;
    bool m_detDnis;
    char m_dtmfTone;
    int m_dtmfCount;
    double m_xv[3];
    double m_pwr;
    Tone2PoleFilter m_fax;
    Tone2PoleFilter m_cont;
    Tone2PoleFilter m_dtmfL[4];
    Tone2PoleFilter m_dtmfH[4];
};

#endif

/* vi: set ts=8 sw=4 sts=4 noet: */

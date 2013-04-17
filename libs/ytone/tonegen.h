/**
 * tonegen.h
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * Tones generator
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
#ifndef __TONEGEN_H
#define __TONEGEN_H

#include <yatephone.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 40ms silence, 120ms tone, 40ms silence, total 200ms - slow but safe
#define DTMF_LEN 960
#define DTMF_GAP 320

using namespace TelEngine;

static ObjList tones;
static ObjList datas;

typedef struct {
    int nsamples;
    const short* data;
    bool repeat;
} Tone;

class ToneDesc : public String
{
public:
    ToneDesc(const Tone* tone, const String& name,
	const String& prefix = String::empty());
    ~ToneDesc();
    inline const Tone* tones() const
	{ return m_tones; }
    inline bool repeatAll() const
	{ return m_repeatAll; }
    // Init this tone description from comma separated list of tone data
    bool setTones(const String& desc);
    // Tone name/alias match.
    // Set name to this object's name if true is returned and alias matches
    bool isName(String& name) const;
    // Build tones from a list
    static void buildTones(const String& name, const NamedList& list);
private:
    inline void clearTones() {
	    if (m_tones && m_ownTones)
		delete[] m_tones;
	    m_tones = 0;
	    m_ownTones = true;
	    toneListChanged();
	}
    // Called when tones list changed to update data
    void toneListChanged();
    String m_alias;                      // Tone name alias
    Tone* m_tones;                       // Tones array. Ends with an invalid one (zero)
    bool m_ownTones;                     // Clear tones when reset/destroyed
    bool m_repeatAll;                    // True if all tones repeated
};

class ToneData : public GenObject
{
public:
    ToneData(const char* desc);
    inline ToneData(int f1, int f2 = 0, bool modulated = false)
	: m_f1(f1), m_f2(f2), m_mod(modulated), m_data(0)
	{ }
    inline ToneData(const ToneData& original)
	: GenObject(),
	  m_f1(original.f1()), m_f2(original.f2()),
	  m_mod(original.modulated()), m_data(0)
	{ }
    virtual ~ToneData();
    inline int f1() const
	{ return m_f1; }
    inline int f2() const
	{ return m_f2; }
    inline bool modulated() const
	{ return m_mod; }
    inline bool valid() const
	{ return m_f1 != 0; }
    inline bool equals(int f1, int f2) const
	{ return (m_f1 == f1) && (m_f2 == f2); }
    inline bool equals(const ToneData& other) const
	{ return (m_f1 == other.f1()) && (m_f2 == other.f2()); }
    const short* data();
    static ToneData* getData(const char* desc);
    // Decode a tone description from [!]desc[/duration]
    // Build a tone data if needded
    // Return true on success
    static bool decode(const String& desc, int& samples, const short*& data,
	bool& repeat);
private:
    bool parse(const char* desc);
    int m_f1;
    int m_f2;
    bool m_mod;
    const short* m_data;
};

class ToneSource : public ThreadedSource
{
public:
    virtual void destroyed();
    virtual void run();
    inline const String& name()
	{ return m_name; }
    bool startup();
    static ToneSource* getTone(String& tone, const String& prefix);
    static const ToneDesc* getBlock(String& tone, const String& prefix, bool oneShot = false);
    static Tone* buildCadence(const String& desc);
    static Tone* buildDtmf(const String& dtmf, int len = DTMF_LEN, int gap = DTMF_GAP);
protected:
    ToneSource(const ToneDesc* tone = 0);
    virtual bool noChan() const
	{ return false; }
    virtual void cleanup();
    void advanceTone(const Tone*& tone);
    static const ToneDesc* getBlock(String& tone, const ToneDesc* table);
    static const ToneDesc* findToneDesc(String& tone, const String& prefix);
    String m_name;
    const Tone* m_tone;
    int m_repeat;
    bool m_firstPass;
private:
    DataBlock m_data;
    unsigned m_brate;
    unsigned m_total;
    u_int64_t m_time;
};

class TempSource : public ToneSource
{
public:
    TempSource(String& desc, const String& prefix, DataBlock* rawdata);
    virtual ~TempSource();
protected:
    virtual bool noChan() const
	{ return true; }
private:
    Tone* m_single;
    DataBlock* m_rawdata;                // Raw linear data to be sent
};

#endif
/* vi: set ts=8 sw=4 sts=4 noet: */

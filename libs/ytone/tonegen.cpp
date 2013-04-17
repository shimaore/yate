/**
 * tonegen.cpp
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

#include "tonegen.h"

#define __module "tonegen"

using namespace TelEngine;

static ObjList s_toneDesc;               // List of configured tones
static ObjList s_defToneDesc;            // List of default tones
static bool s_defToneDescReady = false;
static String s_defLang;                 // Default tone language
static const String s_default = "itu";

// 421.052Hz (19 samples @ 8kHz) sine wave, pretty close to standard 425Hz
static const short tone421hz[] = {
    19,
    3246, 6142, 8371, 9694, 9965, 9157, 7357, 4759, 1645,
    -1645, -4759, -7357, -9157, -9965, -9694, -8371, -6142, -3246,
    0 };

// 1000Hz (8 samples @ 8kHz) standard digital milliwatt
static const short tone1000hz[] = {
    8,
    8828, 20860, 20860, 8828,
    -8828, -20860, -20860, -8828
    };

// 941.176Hz (2*8.5 samples @ 8kHz) sine wave, approximates 950Hz
static const short tone941hz[] = {
    17,
    6736, 9957, 7980, 1838, -5623, -9617, -8952, -3614,
    3614, 8952, 9617, 5623, -1838, -7980, -9957, -6736,
    0 };

// 1454.545Hz (2*5.5 samples @ 8kHz) sine wave, approximates 1400Hz
static const short tone1454hz[] = {
    11,
    9096, 7557, -2816, -9898, -5407,
    5407, 9898, 2816, -7557, -9096,
    0 };

// 1777.777Hz (2*4.5 samples @ 8kHz) sine wave, approximates 1800Hz
static const short tone1777hz[] = {
    9,
    9848, 3420, -8659, -6429,
    6429, 8659, -3420, -9848,
    0 };

static const Tone t_dial[] = { { 8000, tone421hz, true }, { 0, 0 } };

static const Tone t_busy[] = { { 4000, tone421hz, true }, { 4000, 0, true }, { 0, 0 } };

static const Tone t_specdial[] = { { 7600, tone421hz, true }, { 400, 0, true }, { 0, 0 } };

static const Tone t_ring[] = { { 8000, tone421hz, true }, { 32000, 0, true }, { 0, 0 } };

static const Tone t_congestion[] = { { 2000, tone421hz, true }, { 2000, 0, true }, { 0, 0 } };

static const Tone t_outoforder[] = {
    { 800, tone421hz, true }, { 800, 0, true },
    { 800, tone421hz, true }, { 800, 0, true },
    { 800, tone421hz, true }, { 800, 0, true },
    { 1600, tone421hz, true }, { 1600, 0, true },
    { 0, 0 } };

static const Tone t_callwait[] = {
    { 160, 0, true },
    { 800, tone421hz, true }, { 800, 0, true }, { 800, tone421hz, true },
    { 160, 0, true },
    { 0, 0 } };

static const Tone t_info[] = {
    { 2640, tone941hz, true }, { 240, 0, true },
    { 2640, tone1454hz, true }, { 240, 0, true },
    { 2640, tone1777hz, true }, { 8000, 0, true },
    { 0, 0 } };

static const Tone t_mwatt[] = { { 8000, tone1000hz, true }, { 0, 0 } };

static const Tone t_silence[] = { { 8000, 0, true }, { 0, 0 } };

static const Tone t_noise[] = { { 2000, ToneData::getData("noise")->data(), true }, { 0, 0 } };

#define MAKE_DTMF(s) { \
    { DTMF_GAP, 0, true }, \
    { DTMF_LEN, ToneData::getData(s)->data(), true }, \
    { DTMF_GAP, 0, true }, \
    { 0, 0 } \
}
static const Tone t_dtmf[][4] = {
    MAKE_DTMF("1336+941"),
    MAKE_DTMF("1209+697"),
    MAKE_DTMF("1336+697"),
    MAKE_DTMF("1477+697"),
    MAKE_DTMF("1209+770"),
    MAKE_DTMF("1336+770"),
    MAKE_DTMF("1477+770"),
    MAKE_DTMF("1209+852"),
    MAKE_DTMF("1336+852"),
    MAKE_DTMF("1477+852"),
    MAKE_DTMF("1209+941"),
    MAKE_DTMF("1477+941"),
    MAKE_DTMF("1633+697"),
    MAKE_DTMF("1633+770"),
    MAKE_DTMF("1633+852"),
    MAKE_DTMF("1633+941")
};
#undef MAKE_DTMF

#define MAKE_PROBE(s) { \
    { 8000, ToneData::getData(s)->data(), true }, \
    { 0, 0 } \
}
static const Tone t_probes[][2] = {
    MAKE_PROBE("2000+125"),
    MAKE_PROBE("2000*125"),
    MAKE_PROBE("2000*1000"),
    MAKE_PROBE("2010"),
    MAKE_PROBE("1780"),
};
#undef MAKE_PROBE

static const ToneDesc s_descOne[] = {
    ToneDesc(t_callwait,"callwaiting"),
    ToneDesc(t_dtmf[0],"dtmf/0"),
    ToneDesc(t_dtmf[1],"dtmf/1"),
    ToneDesc(t_dtmf[2],"dtmf/2"),
    ToneDesc(t_dtmf[3],"dtmf/3"),
    ToneDesc(t_dtmf[4],"dtmf/4"),
    ToneDesc(t_dtmf[5],"dtmf/5"),
    ToneDesc(t_dtmf[6],"dtmf/6"),
    ToneDesc(t_dtmf[7],"dtmf/7"),
    ToneDesc(t_dtmf[8],"dtmf/8"),
    ToneDesc(t_dtmf[9],"dtmf/9"),
    ToneDesc(t_dtmf[10],"dtmf/*"),
    ToneDesc(t_dtmf[11],"dtmf/#"),
    ToneDesc(t_dtmf[12],"dtmf/a"),
    ToneDesc(t_dtmf[13],"dtmf/b"),
    ToneDesc(t_dtmf[14],"dtmf/c"),
    ToneDesc(t_dtmf[15],"dtmf/d"),

    ToneDesc(t_dial,"dial"),
    ToneDesc(t_busy,"busy"),
    ToneDesc(t_ring,"ring"),
    ToneDesc(t_specdial,"specdial"),
    ToneDesc(t_congestion,"congestion"),
    ToneDesc(t_outoforder,"outoforder"),
    ToneDesc(t_info,"info"),
    ToneDesc(t_mwatt,"milliwatt"),
    ToneDesc(t_silence,"silence"),
    ToneDesc(t_noise,"noise"),
    ToneDesc(t_probes[0],"probe/0"),
    ToneDesc(t_probes[1],"probe/1"),
    ToneDesc(t_probes[2],"probe/2"),
    ToneDesc(t_probes[3],"cotv"),
    ToneDesc(t_probes[4],"cots"),

    ToneDesc((Tone*)0,"")
};


// This function is here mainly to avoid 64bit gcc b0rking optimizations
static unsigned int byteRate(u_int64_t time, unsigned int bytes)
{
    if (!(time && bytes))
	return 0;
    time = Time::now() - time;
    if (!time)
	return 0;
    return (unsigned int)((bytes*(u_int64_t)1000000 + time/2) / time);
}

// Retrieve the alias associated with a given name
static const char* getAlias(const String& name)
{
#define TONE_GETALIAS(n,a) { if (name == n) return a; }
    if (!name)
	return 0;
    TONE_GETALIAS("dial","dt");
    TONE_GETALIAS("busy","bs");
    TONE_GETALIAS("ring","rt");
    TONE_GETALIAS("specdial","sd");
    TONE_GETALIAS("congestion","cg");
    TONE_GETALIAS("outoforder","oo");
    TONE_GETALIAS("info","in");
    TONE_GETALIAS("milliwatt","mw");
    TONE_GETALIAS("silence",0);
    TONE_GETALIAS("noise","cn");
    TONE_GETALIAS("probe/0","probe");
    TONE_GETALIAS("probe/1",0);
    TONE_GETALIAS("probe/2",0);
    TONE_GETALIAS("cotv","co1");
    TONE_GETALIAS("cots","co2");
    TONE_GETALIAS("callwaiting","cw");
    TONE_GETALIAS("dtmf/0","0");
    TONE_GETALIAS("dtmf/1","1");
    TONE_GETALIAS("dtmf/2","2");
    TONE_GETALIAS("dtmf/3","3");
    TONE_GETALIAS("dtmf/4","4");
    TONE_GETALIAS("dtmf/5","5");
    TONE_GETALIAS("dtmf/6","6");
    TONE_GETALIAS("dtmf/7","7");
    TONE_GETALIAS("dtmf/8","8");
    TONE_GETALIAS("dtmf/9","9");
    TONE_GETALIAS("dtmf/*","*");
    TONE_GETALIAS("dtmf/#","#");
    TONE_GETALIAS("dtmf/a","a");
    TONE_GETALIAS("dtmf/b","b");
    TONE_GETALIAS("dtmf/c","c");
    TONE_GETALIAS("dtmf/d","d");
    return 0;
#undef TONE_GETALIAS
}

ToneDesc::ToneDesc(const Tone* tone, const String& name, const String& prefix)
    : String(prefix + name),
    m_tones((Tone*)tone),
    m_ownTones(false),
    m_repeatAll(true)
{
    const char* alias = getAlias(name);
    if (alias)
	m_alias = prefix + alias;
    toneListChanged();
    XDebug(__module,DebugAll,"ToneDesc(%s) [%p]",c_str(),this);
}

ToneDesc::~ToneDesc()
{
    clearTones();
}

// Init this tone description from comma separated list if tone data
bool ToneDesc::setTones(const String& desc)
{
    Debug(__module,DebugAll,"ToneDesc(%s) initializing from '%s' [%p]",
	c_str(),desc.c_str(),this);
    clearTones();
    ObjList* list = desc.split(',',false);
    m_tones = new Tone[list->count() + 1];
    m_ownTones = true;
    int n = 0;
    for (ObjList* o = list->skipNull(); o; o = o->skipNext(), n++) {
	const String& s = o->get()->toString();
	if (ToneData::decode(s,m_tones[n].nsamples,m_tones[n].data,m_tones[n].repeat))
	    DDebug(__module,DebugAll,
		"ToneDesc(%s) added tone '%s' samples=%d data=%p repeat=%d [%p]",
		c_str(),s.c_str(),m_tones[n].nsamples,m_tones[n].data,
		m_tones[n].repeat,this);
	else {
	    Debug(__module,DebugNote,"ToneDesc(%s) invalid tone description '%s' [%p]",
		c_str(),s.c_str(),this);
	    n = -1;
	    break;
	}
    }
    TelEngine::destruct(list);
    if (n > 0)
	// Invalidate the last tone in the list
	::memset(m_tones + n,0,sizeof(Tone));
    else
	clearTones();
    toneListChanged();
    return n != 0;
}

// Tone name/alias equality operator. Set name if true is returned
bool ToneDesc::isName(String& name) const
{
    if (name == *this)
	return true;
    if (!m_alias || m_alias != name)
	return false;
    name = *this;
    return true;
}

// Build tone descriptions from a list
void ToneDesc::buildTones(const String& name, const NamedList& list)
{
    DDebug(__module,DebugAll,"Building tones lang=%s from list=%s",
	name.c_str(),list.c_str());
    String prefix;
    ObjList* target = &s_defToneDesc;
    if (name && name != s_default) {
	prefix << name << "/";
	target = &s_toneDesc;
    }
    unsigned int n = list.length();
    for (unsigned int i = 0; i < n; i++) {
	NamedString* ns = list.getParam(i);
	if (TelEngine::null(ns))
	    continue;
	ToneDesc* d = new ToneDesc(0,ns->name(),prefix);
	if (d->setTones(*ns)) {
	    ObjList* o = target->find(d->toString());
	    if (!o)
		target->append(d);
	    else {
		Debug(__module,DebugInfo,"Replacing tone '%s' (from list '%s')",
		    d->toString().c_str(),list.c_str());
		o->set(d);
	    }
	}
	else
	    TelEngine::destruct(d);
    }
}

// Called when tones list changed to update data
void ToneDesc::toneListChanged()
{
    m_repeatAll = true;
    if (!m_tones)
	return;
    for (Tone* tone = m_tones; tone->nsamples; tone++)
	if (!tone->repeat) {
	    m_repeatAll = false;
	    break;
	}
}


ToneData::ToneData(const char* desc)
    : m_f1(0), m_f2(0), m_mod(false), m_data(0)
{
    if (!parse(desc)) {
	Debug(__module,DebugWarn,"Invalid tone description '%s'",desc);
	m_f1 = m_f2 = 0;
	m_mod = false;
    }
}

ToneData::~ToneData()
{
    if (m_data) {
	::free((void*)m_data);
	m_data = 0;
    }
}

// a tone data description is something like "425" or "350+440" or "15*2100"
bool ToneData::parse(const char* desc)
{
    if (!desc)
	return false;
    String tmp(desc);
    if (tmp == "noise") {
	m_f1 = -10;
	return true;
    }
    tmp >> m_f1;
    if (!m_f1)
	return false;
    if (m_f1 < -15)
	m_f1 = -15;
    if (tmp) {
	char sep;
	tmp >> sep;
	switch (sep) {
	    case '+':
		break;
	    case '*':
		m_mod = true;
		break;
	    default:
		return false;
	}
	tmp >> m_f2;
	if (!m_f2)
	    return false;
	// order components so we can compare correctly
	if (m_f1 < m_f2) {
	    int t = m_f1;
	    m_f1 = m_f2;
	    m_f2 = t;
	}
    }
    return true;
}

const short* ToneData::data()
{
    if (m_f1 && !m_data) {
	// generate the data on first call
	short len = 8000;
	if (m_f1 < 0) {
	    Debug(__module,DebugAll,"Building comfort noise at level %d",m_f1);
	    // we don't need much memory for noise...
	    len /= 8;
	}
	else if (m_f2)
	    Debug(__module,DebugAll,"Building tone of %d %s %d Hz",
		m_f1,(m_mod ? "modulated by" : "+"),m_f2);
	else {
	    Debug(__module,DebugAll,"Building tone of %d Hz",m_f1);
	    // half the buffer for even frequencies
	    if ((m_f1 & 1) == 0)
		len /= 2;
	}
	short* dat = (short*)::malloc((len+1)*sizeof(short));
	if (!dat) {
	    Debug(__module,DebugGoOn,"ToneData::data() cold not allocate memory for %d elements",len);
	    return 0;
	}
	short* tmp = dat;
	*tmp++ = len;
	if (m_f1 < 0) {
	    int ofs = 65535 >> (-m_f1);
	    int max = 2 * ofs + 1;
	    for (int x = 0; x < len; x++)
		*tmp++ = (short)((Random::random() % max) - ofs);
	}
	else {
	    double samp = 2*M_PI/8000;
	    for (int x = 0; x < len; x++) {
		double y = ::sin(x*samp*m_f1);
		if (m_f2) {
		    double z = ::sin(x*samp*m_f2);
		    if (m_mod)
			y *= (1+0.5*z);
		    else
			y += z;
		}
		*tmp++ = (short)(y*5000);
	    }
	}
	m_data = dat;
    }
    return m_data;
}

ToneData* ToneData::getData(const char* desc)
{
    ToneData td(desc);
    if (!td.valid())
	return 0;
    ObjList* l = &datas;
    for (; l; l = l->next()) {
	ToneData* d = static_cast<ToneData*>(l->get());
	if (d && d->equals(td))
	    return d;
    }
    ToneData* d = new ToneData(td);
    datas.append(d);
    return d;
}

// Decode a tone description from [!]desc[/duration]
// Build a tone data if needded
// Return true on success
bool ToneData::decode(const String& desc, int& samples, const short*& data, bool& repeat)
{
    if (!desc)
	return false;
    samples = 8000;
    data = 0;
    repeat = (desc[0] != '!');
    int start = repeat ? 0 : 1;
    int pos = desc.find('/',start);
    String freq;
    if (pos > 0) {
	String dur = desc.substr(pos + 1);
	int duration = dur.toInteger();
	if (duration > 0) {
	    // Round up to a multiple of 20
	    duration += 19;
	    samples = duration / 20 * 160;
	}
	freq = desc.substr(start,pos - start);
    }
    else
	freq = desc.substr(start);
    // Silence ?
    if (freq.toInteger(-1) == 0)
	return true;
    ToneData* td = ToneData::getData(freq);
    if (td)
	data = td->data();
    return td != 0;
}

ToneSource::ToneSource(const ToneDesc* tone)
    : m_tone(0), m_repeat(tone == 0), m_firstPass(true),
      m_data(0,320), m_brate(16000), m_total(0), m_time(0)
{
    if (tone) {
	m_tone = tone->tones();
	m_name = *tone;
    }
    Debug(__module,DebugAll,"ToneSource::ToneSource(%p) '%s' [%p]",
	tone,m_name.c_str(),this);
}

void ToneSource::destroyed()
{
    Debug(__module,DebugAll,"ToneSource::destroyed() '%s' [%p] total=%u stamp=%lu",
	m_name.c_str(),this,m_total,timeStamp());
    ThreadedSource::destroyed();
    if (m_time)
	Debug(__module,DebugInfo,"ToneSource rate=%u b/s",byteRate(m_time,m_total));
}

bool ToneSource::startup()
{
    DDebug(__module,DebugAll,"ToneSource::startup(\"%s\") tone=%p",m_name.c_str(),m_tone);
    return m_tone && start("Tone Source");
}

void ToneSource::cleanup()
{
    Debug(__module,DebugAll,"ToneSource::cleanup() '%s' [%p]",m_name.c_str(),this);
    tones.remove(this,false);
    ThreadedSource::cleanup();
}

void ToneSource::advanceTone(const Tone*& tone)
{
    if (!tone)
	return;
    const Tone* start = tone;
    tone++;
    while (tone && tone != start) {
	if (!tone->nsamples) {
	    if ((m_repeat > 0) && !(--m_repeat))
		m_tone = 0;
	    tone = m_tone;
	    m_firstPass = false;
	    continue;
	}
	if (m_firstPass || tone->repeat)
	    break;
	tone++;
    }
    if (tone == start && !m_firstPass && !tone->repeat) {
	m_tone = 0;
	tone = 0;
    }
}

const ToneDesc* ToneSource::getBlock(String& tone, const ToneDesc* table)
{
    for (; table->tones(); table++) {
	if (table->isName(tone))
	    return table;
    }
    return 0;
}

const ToneDesc* ToneSource::findToneDesc(String& tone, const String& prefix)
{
    XDebug(__module,DebugAll,"ToneSource::findToneDesc(%s,%s)",
	tone.c_str(),prefix.c_str());
    ObjList* target = &s_defToneDesc;
    if (prefix) {
	tone = prefix + "/" + tone;
	target = &s_toneDesc;
    }
    for (ObjList* o = target->skipNull(); o; o = o->skipNext()) {
	const ToneDesc* d = static_cast<ToneDesc*>(o->get());
	if (d->isName(tone))
	    return d;
    }
    if (prefix)
	tone.startSkip(prefix + "/",false);
    return 0;
}

const ToneDesc* ToneSource::getBlock(String& tone, const String& prefix, bool oneShot)
{
    if (tone.trimBlanks().toLower().null())
	return 0;
    if (!s_defToneDescReady) {
	s_defToneDesc.append(new ToneDesc(t_dial,"dial"));
	s_defToneDesc.append(new ToneDesc(t_busy,"busy"));
	s_defToneDesc.append(new ToneDesc(t_ring,"ring"));
	s_defToneDesc.append(new ToneDesc(t_specdial,"specdial"));
	s_defToneDesc.append(new ToneDesc(t_congestion,"congestion"));
	s_defToneDesc.append(new ToneDesc(t_outoforder,"outoforder"));
	s_defToneDesc.append(new ToneDesc(t_info,"info"));
	s_defToneDesc.append(new ToneDesc(t_mwatt,"milliwatt"));
	s_defToneDesc.append(new ToneDesc(t_silence,"silence"));
	s_defToneDesc.append(new ToneDesc(t_noise,"noise"));
	s_defToneDesc.append(new ToneDesc(t_probes[0],"probe/0"));
	s_defToneDesc.append(new ToneDesc(t_probes[1],"probe/1"));
	s_defToneDesc.append(new ToneDesc(t_probes[2],"probe/2"));
	s_defToneDesc.append(new ToneDesc(t_probes[3],"cotv"));
	s_defToneDesc.append(new ToneDesc(t_probes[4],"cots"));
	s_defToneDescReady = true;
    }
    XDebug(__module,DebugAll,"ToneSource::getBlock(%s,%s,%u)",
	tone.c_str(),prefix.c_str(),oneShot);
    const ToneDesc* d = 0;
    if (prefix) {
	if (prefix != s_default)
	    d = findToneDesc(tone,prefix);
	else {
	    // Default tone explicitly required
	    d = findToneDesc(tone,String::empty());
	    if (!d && oneShot)
		d = getBlock(tone,s_descOne);
	    return d;
	}
    }
    if (!d && s_defLang && s_defLang != prefix)
	d = findToneDesc(tone,s_defLang);
    if (!d)
	d = findToneDesc(tone,String::empty());
    if (d)
	return d;
    if (oneShot)
	return getBlock(tone,s_descOne);
    return 0;
}

// Build an user defined cadence
Tone* ToneSource::buildCadence(const String& desc)
{
    // TBD
    return 0;
}

// Build a cadence out of DTMFs
Tone* ToneSource::buildDtmf(const String& dtmf, int len, int gap)
{
    if (dtmf.null())
	return 0;
    Tone* tmp = (Tone*)::malloc(2*sizeof(Tone)*(dtmf.length()+1));
    if (!tmp)
	return 0;
    Tone* t = tmp;

    for (unsigned int i = 0; i < dtmf.length(); i++) {
	t->nsamples = gap;
	t->data = 0;
	t->repeat = true;
	t++;

	int c = dtmf.at(i);
	if ((c >= '0') && (c <= '9'))
	    c -= '0';
	else if (c == '*')
	    c = 10;
	else if (c == '#')
	    c = 11;
	else if ((c >= 'a') && (c <= 'd'))
	    c -= ('a' - 12);
	else c = -1;

	t->nsamples = len;
	t->data = ((c >= 0) && (c < 16)) ? t_dtmf[c][1].data : 0;
	t->repeat = true;
	t++;
    }

    t->nsamples = gap;
    t->data = 0;
    t->repeat = true;
    t++;
    t->nsamples = 0;
    t->data = 0;

    return tmp;
}

ToneSource* ToneSource::getTone(String& tone, const String& prefix)
{
    const ToneDesc* td = ToneSource::getBlock(tone,prefix);
    bool repeat = !td || td->repeatAll();
    XDebug(__module,DebugAll,"ToneSource::getTone(%s,%s) found %p '%s' repeatall=%s",
	tone.c_str(),prefix.c_str(),td,td ? td->c_str() : "",String::boolText(repeat));
    // tone name is now canonical
    // Build a fresh source if the list contains tones not repeated
    ObjList* l = repeat ? &tones : 0;
    for (; l; l = l->next()) {
	ToneSource* t = static_cast<ToneSource*>(l->get());
	if (t && (t->name() == tone) && t->running() && (t->refcount() > 1)) {
	    t->ref();
	    return t;
	}
    }
    if (!td)
	return 0;
    ToneSource* t = new ToneSource(td);
    tones.append(t);
    t->startup();
    return t;
}

void ToneSource::run()
{
    Debug(__module,DebugAll,"ToneSource::run() [%p]",this);
    u_int64_t tpos = Time::now();
    m_time = tpos;
    int samp = 0; // sample number
    int dpos = 1; // position in data
    const Tone* tone = m_tone;
    int nsam = tone->nsamples;
    if (nsam < 0)
	nsam = -nsam;
    while (m_tone && looping(noChan())) {
	Thread::check();
	short *d = (short *) m_data.data();
	for (unsigned int i = m_data.length()/2; i--; samp++,dpos++) {
	    if (samp >= nsam) {
		// go to the start of the next tone
		samp = 0;
		const Tone *otone = tone;
		advanceTone(tone);
		nsam = tone ? tone->nsamples : 32000;
		if (nsam < 0) {
		    nsam = -nsam;
		    // reset repeat point here
		    m_tone = tone;
		}
		if (tone != otone)
		    dpos = 1;
	    }
	    if (tone && tone->data) {
		if (dpos > tone->data[0])
		    dpos = 1;
		*d++ = tone->data[dpos];
	    }
	    else
		*d++ = 0;
	}
	int64_t dly = tpos - Time::now();
	if (dly > 0) {
	    XDebug(__module,DebugAll,"ToneSource sleeping for " FMT64 " usec",dly);
	    Thread::usleep((unsigned long)dly);
	}
	if (!looping(noChan()))
	    break;
	Forward(m_data,m_total/2);
	m_total += m_data.length();
	tpos += (m_data.length()*(u_int64_t)1000000/m_brate);
    }
    Debug(__module,DebugAll,"ToneSource [%p] end, total=%u (%u b/s)",
	this,m_total,byteRate(m_time,m_total));
    m_time = 0;
}


TempSource::TempSource(String& desc, const String& prefix, DataBlock* rawdata)
    : m_single(0), m_rawdata(rawdata)
{
    Debug(__module,DebugAll,"TempSource::TempSource(\"%s\",\"%s\") [%p]",
	desc.c_str(),prefix.safe(),this);
    if (desc.null())
	return;
    m_name = desc;
    if (desc.startSkip("*",false))
	m_repeat = 0;
    // Build a source used to send raw linear data
    if (desc == "rawdata") {
	if (!(m_rawdata && m_rawdata->length() >= sizeof(short))) {
	    Debug(__module,DebugNote,
		"TempSource::TempSource(\"%s\") invalid data size=%u [%p]",
		desc.c_str(),m_rawdata?m_rawdata->length():0,this);
	    return;
	}
	m_tone = m_single = (Tone*)::malloc(2*sizeof(Tone));
	m_single[0].nsamples = m_rawdata->length() / sizeof(short);
	m_single[0].data = (short*)m_rawdata->data();
	m_single[0].repeat = true;
	m_single[1].nsamples = 0;
	m_single[1].data = 0;
	return;
    }
    // try first the named tones
    const ToneDesc* tde = getBlock(desc,prefix,true);
    if (tde) {
	m_tone = tde->tones();
	return;
    }
    // for performance reason accept an entire string of DTMFs
    if (desc.startSkip("dtmfstr/",false)) {
	m_tone = m_single = buildDtmf(desc);
	return;
    }
    // or an entire user defined cadence of tones
    if (desc.startSkip("cadence/",false)) {
	m_tone = m_single = buildCadence(desc);
	return;
    }
    // now try to build a single tone
    int samples = 8000;
    const short* data = 0;
    bool repeat = true;
    if (!ToneData::decode(desc,samples,data,repeat))
	return;
    m_single = (Tone*)::malloc(2*sizeof(Tone));
    m_single[0].nsamples = samples;
    m_single[0].data = data;
    m_single[0].repeat = repeat;
    m_single[1].nsamples = 0;
    m_single[1].data = 0;
    m_tone = m_single;
}

TempSource::~TempSource()
{
    Debug(__module,DebugAll,"TempSource::~TempSource() [%p]",this);
    if (m_single) {
	::free(m_single);
	m_single = 0;
    }
    TelEngine::destruct(m_rawdata);
}

/* vi: set ts=8 sw=4 sts=4 noet: */

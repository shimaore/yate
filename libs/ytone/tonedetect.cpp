/**
 * tonedetect.cpp
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
#include "tonedetect.h"

#define __module "tonedetect"

using namespace TelEngine;

// generated CNG detector (1100Hz) - either of the 2 below:
// mkfilter -Bp -Re 50 -a 0.137500
//  -> 2-pole resonator bandpass, 1100Hz, Q-factor=50
// mkfilter -Bu -Bp -o 1 -a 1.3612500000e-01 1.3887500000e-01
//  -> 2-pole butterworth bandpass, 1100Hz +-11Hz @ -3dB
static Params2Pole s_paramsCNG =
    { 1.167453752e+02, -0.9828688170, 1.2878183436 }; // 1100Hz

// generated CED detector (2100Hz) filter parameters
// mkfilter -Bu -Bp -o 1 -a 2.6062500000e-01 2.6437500000e-01
//  -> 2-pole butterworth bandpass, 2100Hz +-15Hz @ -3dB
static Params2Pole s_paramsCED =
    { 8.587870006e+01, -0.9767113407, -0.1551017476 }; // 2100Hz

// generated continuity test verified detector (2010Hz) filter parameters
// mkfilter -Bu -Bp -o 1 -a 2.5025000000e-01 2.5225000000e-01
//  -> 2-pole butterworth bandpass, 2010Hz +-8Hz @ -3dB
static Params2Pole s_paramsCOTv =
    { 1.601528486e+02, -0.9875119299, -0.0156100298 }; // 2010Hz

// generated continuity test send detector (1780Hz) filter parameters
// mkfilter -Bu -Bp -o 1 -a 2.1875000000e-01 2.2625000000e-01
//  -> 2-pole butterworth bandpass, 1780Hz +-30Hz @ -3dB
static Params2Pole s_paramsCOTs =
    { 4.343337207e+01, -0.9539525559, 0.3360345780 }; // 1780Hz

// generated DTMF component filter parameters
// 2-pole butterworth bandpass, +-1% @ -3dB
static Params2Pole s_paramsDtmfL[] = {
    { 1.836705768e+02, -0.9891110494, 1.6984655220 }, // 697Hz
    { 1.663521771e+02, -0.9879774290, 1.6354206881 }, // 770Hz
    { 1.504376844e+02, -0.9867055777, 1.5582944783 }, // 852Hz
    { 1.363034877e+02, -0.9853269818, 1.4673997821 }, // 941Hz
};
static Params2Pole s_paramsDtmfH[] = {
    { 1.063096655e+02, -0.9811871438, 1.1532059506 }, // 1209Hz
    { 9.629842594e+01, -0.9792313229, 0.9860778489 }, // 1336Hz
    { 8.720029263e+01, -0.9770643703, 0.7895131023 }, // 1477Hz
    { 7.896493565e+01, -0.9746723483, 0.5613790789 }, // 1633Hz
};

// DTMF table using low, high indexes
static char s_tableDtmf[][5] = {
    "123A", "456B", "789C", "*0#D"
};

// Update a moving average with square of value (so we end with ~ power)
static void updatePwr(double& avg, double val)
{
    avg = MOVING_AVG_KEEP*avg + (1-MOVING_AVG_KEEP)*val*val;
}


void Tone2PoleFilter::update(double xd)
{
    m_y[0] = m_y[1]; m_y[1] = m_y[2];
    m_y[2] = (xd * m_mult) +
	(m_y0 * m_y[0]) +
	(m_y1 * m_y[1]);
    updatePwr(m_val,m_y[2]);
}


ToneConsumer::ToneConsumer(const String& id, const String& name)
    : m_id(id), m_name(name), m_mode(Mono),
      m_detFax(true), m_detCont(false), m_detDtmf(true), m_detDnis(false),
      m_fax(s_paramsCNG), m_cont(s_paramsCOTv)
{ 
    Debug(__module,DebugAll,"ToneConsumer::ToneConsumer(%s,'%s') [%p]",
	id.c_str(),name.c_str(),this);
    for (int i = 0; i < 4; i++) {
	m_dtmfL[i].assign(s_paramsDtmfL[i]);
	m_dtmfH[i].assign(s_paramsDtmfH[i]);
    }
    init();
    String tmp = name;
    tmp.startSkip("tone/",false);
    if (tmp.startSkip("mixed/",false))
	m_mode = Mixed;
    else if (tmp.startSkip("left/",false))
	m_mode = Left;
    else if (tmp.startSkip("right/",false))
	m_mode = Right;
    else tmp.startSkip("mono/",false);
    if (m_mode != Mono)
	m_format = "2*slin";
    if (tmp && (tmp != "*")) {
	// individual detection requested
	m_detFax = m_detCont = m_detDtmf = m_detDnis = false;
	ObjList* k = tmp.split(',',false);
	for (ObjList* l = k; l; l = l->next()) {
	    String* s = static_cast<String*>(l->get());
	    if (!s)
		continue;
	    m_detFax = m_detFax || (*s == "fax");
	    m_detCont = m_detCont || (*s == "cotv");
	    m_detDtmf = m_detDtmf || (*s == "dtmf");
	    if (*s == "rfax") {
		// detection of receiving Fax requested
		m_fax.assign(s_paramsCED);
		m_detFax = true;
	    }
	    else if (*s == "cots") {
		// detection of COT Send tone requested
		m_cont.assign(s_paramsCOTs);
		m_detCont = true;
	    }
	    else if (*s == "callsetup") {
		// call setup info in the form *ANI*DNIS*
		m_detDnis = true;
	    }
	}
	TelEngine::destruct(k);
    }
}

ToneConsumer::~ToneConsumer()
{
    Debug(__module,DebugAll,"ToneConsumer::~ToneConsumer [%p]",this);
}

// Re-init filter(s)
void ToneConsumer::init()
{
    m_xv[1] = m_xv[2] = 0.0;
    m_pwr = 0.0;
    m_fax.init();
    m_cont.init();
    for (int i = 0; i < 4; i++) {
	m_dtmfL[i].init();
	m_dtmfH[i].init();
    }
    m_dtmfTone = '\0';
    m_dtmfCount = 0;
}

// Check if we detected a DTMF
void ToneConsumer::checkDtmf()
{
    int i;
    char c = m_dtmfTone;
    m_dtmfTone = '\0';
    int l = 0;
    double maxL = m_dtmfL[0].value();
    for (i = 1; i < 4; i++) {
	if (maxL < m_dtmfL[i].value()) {
	    maxL = m_dtmfL[i].value();
	    l = i;
	}
    }
    int h = 0;
    double maxH = m_dtmfH[0].value();
    for (i = 1; i < 4; i++) {
	if (maxH < m_dtmfH[i].value()) {
	    maxH = m_dtmfH[i].value();
	    h = i;
	}
    }
    double limitAll = m_pwr*THRESHOLD2_REL_ALL;
    double limitOne = limitAll*THRESHOLD2_REL_DTMF;
    if (c) {
	limitAll *= THRESHOLD2_REL_HIST;
	limitOne *= THRESHOLD2_REL_HIST;
    }
    if ((maxL < limitOne) ||
	(maxH < limitOne) ||
	((maxL+maxH) < limitAll)) {
#ifdef DEBUG
	if (c)
	    Debug(__module,DebugInfo,"Giving up DTMF '%c' lo=%0.1f, hi=%0.1f, total=%0.1f",
		c,maxL,maxH,m_pwr);
#endif
	return;
    }
    char buf[2];
    buf[0] = s_tableDtmf[l][h];
    buf[1] = '\0';
    if (buf[0] != c) {
	DDebug(__module,DebugInfo,"DTMF '%s' new candidate on %s, lo=%0.1f, hi=%0.1f, total=%0.1f",
	    buf,m_id.c_str(),maxL,maxH,m_pwr);
	m_dtmfTone = buf[0];
	m_dtmfCount = 1;
	return;
    }
    m_dtmfTone = c;
    XDebug(__module,DebugAll,"DTMF '%s' candidate %d on %s, lo=%0.1f, hi=%0.1f, total=%0.1f",
	buf,m_dtmfCount,m_id.c_str(),maxL,maxH,m_pwr);
    if (m_dtmfCount++ == DETECT_DTMF_MSEC) {
	DDebug(__module,DebugNote,"%sDTMF '%s' detected on %s, lo=%0.1f, hi=%0.1f, total=%0.1f",
	    (m_detDnis ? "DNIS/" : ""),
	    buf,m_id.c_str(),maxL,maxH,m_pwr);
	if (m_detDnis) {
	    static Regexp r("^\\*\\([0-9#]*\\)\\*\\([0-9#]*\\)\\*$");
	    m_dnis += buf;
	    if (m_dnis.matches(r)) {
		m_detDnis = false;
		Message* m = new Message("chan.notify");
		m->addParam("id",m_id);
		if (m_target)
		    m->addParam("targetid",m_target);
		m->addParam("operation","setup");
		m->addParam("caller",m_dnis.matchString(1));
		m->addParam("called",m_dnis.matchString(2));
		Engine::enqueue(m);
	    }
	    return;
	}
	Message *m = new Message("chan.masquerade");
	m->addParam("id",m_id);
	m->addParam("message","chan.dtmf");
	m->addParam("text",buf);
	m->addParam("detected","inband");
	Engine::enqueue(m);
    }
}

// Check if we detected a Fax CNG or CED tone
void ToneConsumer::checkFax()
{
    if (m_fax.value() < m_pwr*THRESHOLD2_REL_FAX)
	return;
    if (m_fax.value() > m_pwr) {
	DDebug(__module,DebugNote,"Overshoot on %s, signal=%0.2f, total=%0.2f",
	    m_id.c_str(),m_fax.value(),m_pwr);
	init();
	return;
    }
    DDebug(__module,DebugInfo,"Fax detected on %s, signal=%0.1f, total=%0.1f",
	m_id.c_str(),m_fax.value(),m_pwr);
    // prepare for new detection
    init();
    m_detFax = false;
    Message* m = new Message("chan.masquerade");
    m->addParam("id",m_id);
    if (m_faxDivert) {
	Debug(__module,DebugCall,"Diverting call %s to: %s",
	    m_id.c_str(),m_faxDivert.c_str());
	m->addParam("message","call.execute");
	m->addParam("callto",m_faxDivert);
	m->addParam("reason","fax");
    }
    else {
	m->addParam("message","call.fax");
	m->addParam("detected","inband");
    }
    m->addParam("caller",m_faxCaller,false);
    m->addParam("called",m_faxCalled,false);
    Engine::enqueue(m);
}

// Check if we detected a Continuity Test tone
void ToneConsumer::checkCont()
{
    if (m_cont.value() < m_pwr*THRESHOLD2_REL_COT)
	return;
    if (m_cont.value() > m_pwr) {
	DDebug(__module,DebugNote,"Overshoot on %s, signal=%0.2f, total=%0.2f",
	    m_id.c_str(),m_cont.value(),m_pwr);
	init();
	return;
    }
    DDebug(__module,DebugInfo,"Continuity detected on %s, signal=%0.1f, total=%0.1f",
	m_id.c_str(),m_cont.value(),m_pwr);
    // prepare for new detection
    init();
    m_detCont = false;
    Message* m = new Message("chan.masquerade");
    m->addParam("id",m_id);
    m->addParam("message","chan.dtmf");
    m->addParam("text","O");
    m->addParam("detected","inband");
    Engine::enqueue(m);
}

// Feed samples to the filter(s)
unsigned long ToneConsumer::Consume(const DataBlock& data, unsigned long tStamp, unsigned long flags)
{
    unsigned int samp = data.length() / 2;
    if (m_mode != Mono)
	samp /= 2;
    if (!samp)
	return 0;
    const int16_t* s = (const int16_t*)data.data();
    if (!s)
	return 0;
    while (samp--) {
	m_xv[0] = m_xv[1]; m_xv[1] = m_xv[2];
	switch (m_mode) {
	    case Left:
		// use 1st sample, skip 2nd
		m_xv[2] = *s++;
		s++;
		break;
	    case Right:
		// skip 1st sample, use 2nd
		s++;
		m_xv[2] = *s++;
		break;
	    case Mixed:
		// add together samples
		m_xv[2] = s[0]+(int)s[1];
		s+=2;
		break;
	    default:
		Debug(__module,DebugAll,"Consume %d",*s);
		m_xv[2] = *s++;
	}
	double dx = m_xv[2] - m_xv[0];
	updatePwr(m_pwr,m_xv[2]);

	// update all active detectors
	if (m_detFax)
	    m_fax.update(dx);
	if (m_detCont)
	    m_cont.update(dx);
	if (m_detDtmf || m_detDnis) {
	    for (int j = 0; j < 4; j++) {
		m_dtmfL[j].update(dx);
		m_dtmfH[j].update(dx);
	    }
	}
	// only do checks every millisecond
	if (samp % 8)
	    continue;
	// is it enough total power to accept a signal?
	if (m_pwr >= THRESHOLD2_ABS) {
	    if (m_detDtmf || m_detDnis)
		checkDtmf();
	    if (m_detFax)
		checkFax();
	    if (m_detCont)
		checkCont();
	}
	else {
	    m_dtmfTone = '\0';
	    m_dtmfCount = 0;
	}
    }
    XDebug(__module,DebugAll,"Fax detector on %s: signal=%0.1f, total=%0.1f",
	m_id.c_str(),m_fax.value(),m_pwr);
    return invalidStamp();
}

// Copy parameters required for automatic fax call diversion
void ToneConsumer::setFaxDivert(const Message& msg)
{
    m_target = msg.getParam("notify");
    if (m_id.null())
	m_id = m_target;
    NamedString* divert = msg.getParam("fax_divert");
    if (!divert)
	return;
    m_detFax = true;
    // if divert is empty or false disable diverting
    if (divert->null() || !divert->toBoolean(true))
	m_faxDivert.clear();
    else {
	m_faxDivert = *divert;
	m_faxCaller = msg.getValue("fax_caller",msg.getValue("caller",m_faxCaller));
	m_faxCalled = msg.getValue("fax_called",msg.getValue("called",m_faxCalled));
    }
}

/* vi: set ts=8 sw=4 sts=4 noet: */

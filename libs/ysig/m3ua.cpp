/* m3ua.cpp */

#include "yatesig.h"

// class SS7M3UA : public SS7Layer3, public SIGAdaptUser
// Note: SIGAdaptClient != SIGAdaptUser
// And I probably need to write SS7M3UAGateway, actually.

/* This implements only the Signalling Gateway part of RFC4666, in the section 1.5.1 (SGP with NIF) scenario,
 * with a view to supporting only A1 (GSM BSC - MSC) links, typically with very limited link indication and
 * no routing. Therefor we do not have routing contexts / routing keys. */

/* ASP side: we don't need it at this time, so it's left empty (no processing) */
virtual bool SS7M3UAClient::processMSG(unsigned char msgVersion, unsigned char msgClass,
    unsigned char msgType, const DataBlock& msg, int streamId) {
    // assert(msgVersion == 1);
    switch(msgClass) {
	case MGMT:
	    switch(msgType) {
		case 0: // Error (ERR)
		case 1: // Notify (NTFY)
		default:
	    }
	    break;
	case TRAN:
	    switch(msgType) {
		case 1: // Payload Data (DATA)
		    // push it up towards the ASP
		    break;
	    }
	    break;
	case SSNM:
	    break;
	case ASPTM:
	    break;
	case RKM:
	    break;
	default:
	    break;
    }
    Debug(this,DebugStub,"Unhandled M3UA message type %u",msgType);
    return false;
}

SS7M3UA::SS7M3UA(const NamedList& params)
    : SignallingComponent(params.safe(name ? name : "SS7M3UA"),&params,"ss7-m3ua"),
      m_streamId(1)
{
    DDebug(DebugInfo,"Creating SS7M3UA [%p]",this);
}

/* SS7Layer3 */

// Copied over from SS7M2UA, mostly.
virtual bool SS7M3UA::initialize(const NamedList* config) {
    if (config && !adaptation()) {
	NamedList params("");
	if (resolveConfig(YSTRING("client"),params,config) ||
	    resolveConfig(YSTRING("basename"),params,config)) {
	    DDebug(this,DebugInfo,"Creating adaptation '%s' for SS7 M3UA [%p]",
		    params.c_str(),this);
	    params.addParam("basename",params);
	    SS7M3UAClient* client =
                YOBJECT(SS7M3UAClient,engine()->build("SS7M3UAClient",params,false));
            if (!client)
                return false;
            adaptation(client);
            client->initialize(&params);
            TelEngine::destruct(client);
	}
	// FIXME is this required? is this the right way to do it?
	if (resolveConfig(YSTRING("server"),params,config)) {
	    DDebug(this,DebugInfo,"Creating adaptation '%s' for SS7 M3UA [%p]",
		    params.c_str(),this);
	    // params.addParam("basename",params);
	    SS7M3UAServer* server =
                YOBJECT(SS7M3UAServer,engine()->build("SS7M3UAServer",params,false));
            if (!server)
                return false;
            adaptation(server); // FIXME
            server->initialize(&params);
            TelEngine::destruct(server);
	}
    }
    return transport() && control(Resume,const_cast<NamedList*>(config));
}

virtual int SS7M3UA::transmitMSU(const SS7MSU& msu, const SS7Label& label, int sls = -1) {
    if(!transport())
	return false;
    if(!operational(sls))
	return false;
    // RFC 4666 section 3.3.1
    u_int8_t head[8];

    // Network Appearance -- optional
    if (m_networkAppearance) {
	head[0] = 0x02; head[1] = 0x00;
	head[2] = 0x00; head[3] = 0x08;
	head[4] = (m_networkAppearance >> 24) & 0xff;
	head[5] = (m_networkAppearance >> 16) & 0xff;
	head[6] = (m_networkAppearance >>  8) & 0xff;
	head[7] = (m_networkAppearance >>  0) & 0xff;
	packet.append(head,8);
    }

    // Routing Context -- conditional
    if (m_routingContext) {
	head[0] = 0x00; head[1] = 0x06;
	head[2] = 0x00; head[3] = 0x08;
	head[4] = (m_routingContext >> 24) & 0xff;
	head[5] = (m_routingContext >> 16) & 0xff;
	head[6] = (m_routingContext >>  8) & 0xff;
	head[7] = (m_routingContext >>  0) & 0xff;
	packet.append(head,8);
    }

    // Protocol Data - mandatory
    head[0] = 0x02; head[1] = 0x10;
    head[2] = (msu.length() >> 16) & 0xff;
    head[3] = (msu.length() >>  0) & 0xff;
    packet.append(head,4);
    packet.append(msu);

    // Correlation Id
    if (m_correlationId) {
	head[0] = 0x00; head[1] = 0x13;
	head[2] = 0x00; head[3] = 0x08;
	head[4] = (m_correlationId >> 24) & 0xff;
	head[5] = (m_correlationId >> 16) & 0xff;
	head[6] = (m_correlationId >>  8) & 0xff;
	head[7] = (m_correlationId >>  0) & 0xff;
	packet.append(head,8);
    }

    // streamId is the identifier of the stream the message was received on
    transmitMSG(1,TRAN,1,msu,m_streamId);
}

virtual bool SS7M3UA::operational(int sls = -1) const {
    // only one link/linkset
    return m_active;
}

/* SIGAdaptUser */

/**
 * Traffic activity state change notification
* @param active True if the ASP is active and traffic is allowed
*/
virtual void SS7M3UA::activeChange(bool active) {
    // only one link/linkset
    m_active = active;
}

/* vi: set ts=8 sw=4 sts=4 noet: */

#ifndef SOCKET_SEMANTICS_H
#define SOCKET_SEMANTICS_H
/*************************************************************************

socket_semantics.h

Author:    Curt Hartung
Created:   Jun 2010
OS:        Win32
Project:   Stackless Socket

Description: define which socket semantics to compile in

(c) CCP 2010

***************************************************************************/

// Stackless Socket semantics, this implements overlapped IO and
// stackless-aware scheduling
#define SLSOCKET

////////////////////////////////////////////////////////////////////////////////
// CarbonIO-specific

// how much data is called for in single reads, increasing this will
// make large-packet transfers occur quicker at the expense of memory.
// 4096 is the minimum that the windows kernel can lock so smaller values
// are pointless. NOTE: this RAM is allocated statically PER CONNECTION
const unsigned int c_systemPageSize = 4096; // large enough to comfortably handle any normal packet

// how many bytes buffers are truncated to after they're used, a good
// value here is one slightly larger than most of the average packets,
// to minimize the re-allocing of memory
const unsigned int c_minPacketBufferSize = 5000;

// number of threads the system will spawn on our behalf to handle
// completion notifications, a setting of '0' allows the kernel to pick
// an optimal value (in theory)
const unsigned int c_concurrentCioThreads = 0;

// if a recvPacket call is made, but no 'setMaxpacketSize' call has
// been issued yet, this is the size used
const unsigned int c_defaultMaxPacketSize = 1000000;

// how many simultaneous packets can be transmitted through the
// scatter/gather mechanism of WSASend() it will be rare that this
// needs to be larger than 2, but the cost is minimal, and there is a
// potential gain for long-latency pipes. maybe.
const unsigned int c_wsaBuffersToPersist = 8;

// in bytes '0' means disabled
const unsigned int c_outgoingQueueWarnLevel = 2000000; // level at which a warning will be logged
const unsigned int c_outgoingQueueHaltLevel = 0; // level at which the tasklet will be halted (until it drains)
const unsigned int c_outgoingQueueDropLevel = 0; // level at which the connection will be dropped
const unsigned int c_incomingQueueWarnLevel = 1000000; // level at which a warning will be logged
// note that the incoming queue halt does NOT suspend tasklets, instead
// it suspends CarbonIO constantly asking for data, and is therefore in no
// danger of causing a deadlock
const unsigned int c_incomingQueueHaltLevel = 0; // backup at which reads will stop being issued (until its cleared)

// what sized packets will be compressed
const unsigned int c_defaultCompressionThreshold = -1; // 0:compress everything -1:compress nothing
const unsigned int c_defaultMinRatio = 90; // what percent the compressed data must be to the original to be used
const unsigned int c_defaultCompressionLevel = 6; // zlib compression level to use

// how long an SSL handshake is allowed to remain outstanding. Note
// that all connections are checked on an accept operation (the only
// time we really need to recover stale descriptors), so it is
// possible that a failed SSL negotiation can remain connected [much]
// longer than this interval on an unbusy system
const unsigned int c_defaultSSLHandshakeNegotiationTimeSeconds = 15;

// largest a single SSL record can be, this is used to size buffers for
// single-pass-expecting loops
const unsigned int c_largestSSLRecordSize = 16384;

// maximum size that will be accepted for compression
const unsigned int c_maxCompressibleSize = 20000000;

// the most number of 'active' threads that are allowed to be spawned,
// threads blocked on accept/connect etc don't count toward this total
const long c_dynamicWorkerThreadCap = 8;

////////////////////////////////////////////////////////////////////////////////


#endif

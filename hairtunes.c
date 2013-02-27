/*
 * HairTunes - RAOP packet handler and slave-clocked replay engine
 * Copyright (c) James Laird 2011
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#ifdef _WIN32

#include "stdint_win.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <mmsystem.h>
#include <mmreg.h>
#include "ao/ao.h"
#include "RAOPDefs.h"

#ifndef socklen_t
	typedef int socklen_t;
#endif

#ifndef ssize_t
	typedef int ssize_t;
#endif

inline int close(int sd)
{
	return closesocket(sd);
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <openssl/aes.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <ao/ao.h>

#endif

#include <assert.h>
int debug = 0;

#include "alac.h"

// default buffer - about half a second
//Changed these values to make sound synchronized with airport express during multi-room broadcast.


#define MAX_PACKET      2048

typedef unsigned short seq_t;

// global options (constant after init)
unsigned char aeskey[16], aesiv[16];
AES_KEY aes;
char *rtphost = 0;
int dataport = 0, controlport = 0, timingport = 0;
int fmtp[12];
int sampling_rate;
int frame_size;

static ao_device *libao_dev = NULL;

#ifdef _WIN32
char *libao_driver	= "wmm";
char *libao_file	= NULL;
HANDLE libao_hfile	= INVALID_HANDLE_VALUE;
#else
char *libao_driver = NULL;
#endif

const char *libao_devicename	= NULL;
const char *libao_deviceid		= NULL; // ao_options expects "char*"

static int buffer_start_fill = START_FILL;

// FIFO name and file handle
#ifndef _WIN32
char *pipename = 0;
int pipe_handle = -1;
#endif

#define FRAME_BYTES (4*frame_size)
// maximal resampling shift - conservative
#define OUTFRAME_BYTES (4*(frame_size+3))

alac_file *decoder_info = NULL;

int  init_rtp(CRaopContext* pContext);
void init_buffer(void);
int  init_output(void);
void  deinit_rtp(void);

void rtp_request_resend(seq_t first, seq_t last);
void ab_resync(void);

// interthread variables
  // stdin->decoder
volatile double volume = 1.0;
volatile long fix_volume = 0x10000;

typedef struct audio_buffer_entry {   // decoded audio packets
    int ready;
    signed short *data;
} abuf_t;
volatile abuf_t audio_buffer[BUFFER_FRAMES] = { 0 };
bool audio_buffer_init = true;
#define BUFIDX(seqno) ((seq_t)(seqno) % BUFFER_FRAMES)

// mutex-protected variables
volatile seq_t ab_read, ab_write;
int ab_buffering = 1, ab_synced = 0;

#ifdef _WIN32

CMyMutex	ab_mutex;
CEvent		ab_buffer_ready(FALSE, FALSE);

void die(char *why) 
{
	char buf[512];

	sprintf_s(buf, 512, "FATAL: %s\n", why);
    OutputDebugStringA(buf);
	Log(buf);
}

typedef HANDLE pthread_t;

int pthread_create(pthread_t* tid, void* attr, unsigned (__stdcall * start) (void *), void* arg)
{
	unsigned dummy = 0;

	*tid = (pthread_t) _beginthreadex(NULL, 0, start, arg, 0, &dummy);

	return 0;
}


#else
pthread_mutex_t ab_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ab_buffer_ready = PTHREAD_COND_INITIALIZER;

void die(char *why) {
    ATLTRACE( "FATAL: %s\n", why);
    exit(1);
}

#endif

static pthread_t rtp_thread		= NULL;
static pthread_t audio_thread	= NULL;

static int rtp_sockets[2];  // data, control


int hex2bin(unsigned char *buf, char *hex) {
    int i, j;
    if (strlen(hex) != 0x20)
        return 1;
    for (i=0; i<0x10; i++) {
        if (!sscanf(hex, "%2X", &j))
           return 1;
        hex += 2;
        *buf++ = j;
    }
    return 0;
}

int init_decoder(void) {

	if (decoder_info != NULL)
		return 0;

    alac_file *alac;

    frame_size = fmtp[1]; // stereo samples
    sampling_rate = fmtp[11];

    int sample_size = fmtp[3];
    if (sample_size != 16)
        die("only 16-bit samples supported!");

    alac = create_alac(sample_size, 2);
    if (!alac)
        return 1;
    decoder_info = alac;

    alac->setinfo_max_samples_per_frame = frame_size;
    alac->setinfo_7a =      fmtp[2];
    alac->setinfo_sample_size = sample_size;
    alac->setinfo_rice_historymult = fmtp[4];
    alac->setinfo_rice_initialhistory = fmtp[5];
    alac->setinfo_rice_kmodifier = fmtp[6];
    alac->setinfo_7f =      fmtp[7];
    alac->setinfo_80 =      fmtp[8];
    alac->setinfo_82 =      fmtp[9];
    alac->setinfo_86 =      fmtp[10];
    alac->setinfo_8a_rate = fmtp[11];
    allocate_buffers(alac);
    return 0;
}

void deinit_decoder(void) {

	if (decoder_info != NULL)
	{
		destroy_alac(decoder_info);
		decoder_info = NULL;
	}
}

#ifdef _WIN32

long StartDecoder(CRaopContext* pContext)
{
	int nResult = 0;

	controlport = atoi(pContext->m_strCport.c_str());
	timingport	= atoi(pContext->m_strTport.c_str());
	dataport	= atoi(pContext->m_strSport.c_str());

	memcpy(aesiv, pContext->m_Aesiv, sizeof(aesiv));
	ATLASSERT(sizeof(aesiv) == pContext->m_sizeAesiv);

	memcpy(aeskey, pContext->m_Rsaaeskey, sizeof(aeskey));
	ATLASSERT(sizeof(aeskey) == pContext->m_sizeRsaaeskey);

    AES_set_decrypt_key(aeskey, 128, &aes);

	memset(fmtp, 0, sizeof(fmtp));

	char fmtpstr[256];
	strcpy(fmtpstr, pContext->m_strFmtp.c_str());

	char*	arg = strtok(fmtpstr, " \t");
	int		i	= 0;

	while (arg != NULL)
	{
		fmtp[i++] = atoi(arg);
		arg = strtok(NULL, " \t");
	}
	ATLASSERT(i == (sizeof(fmtp)/sizeof(fmtp[0])));

	init_decoder();
	init_buffer();
	nResult = init_rtp(pContext);      // open a UDP listen port and start a listener; decode into ring buffer
	
	if (nResult > 0)
		init_output();              // resample and output from ring buffer

	return nResult;
}

void FlushDecoder()
{
    ab_mutex.Lock();
    ab_resync();
    ab_mutex.Unlock();
}

void VolumeDecoder(double lfVolume)
{
	volume = pow(10.0,0.05*lfVolume);
	fix_volume = (long)(65536.0 * volume);
}

void StopDecoder()
{
	deinit_rtp();
	deinit_decoder();
}

int	GetStartFill()
{
	return buffer_start_fill;
}

void SetStartFill(int nValue)
{
	buffer_start_fill = nValue;
}

#else

int main(int argc, char **argv) {
    char *hexaeskey = 0, *hexaesiv = 0;
    char *fmtpstr = 0;
    char *arg;
    int i;
    assert(RAND_MAX >= 0x10000);    // XXX move this to compile time
    while ( (arg = *++argv) ) {
        if (!strcasecmp(arg, "iv")) {
            hexaesiv = *++argv;
            argc--;
        } else
        if (!strcasecmp(arg, "key")) {
            hexaeskey = *++argv;
            argc--;
        } else
        if (!strcasecmp(arg, "fmtp")) {
            fmtpstr = *++argv;
        } else
        if (!strcasecmp(arg, "cport")) {
            controlport = atoi(*++argv);
        } else
        if (!strcasecmp(arg, "tport")) {
            timingport = atoi(*++argv);
        } else
        if (!strcasecmp(arg, "dport")) {
            dataport = atoi(*++argv);
        } else
        if (!strcasecmp(arg, "host")) {
            rtphost = *++argv;
        } else
        if (!strcasecmp(arg, "pipe")) {
            pipename = *++argv;
        }
    }

    if (!hexaeskey || !hexaesiv)
        die("Must supply AES key and IV!");

    if (hex2bin(aesiv, hexaesiv))
        die("can't understand IV");
    if (hex2bin(aeskey, hexaeskey))
        die("can't understand key");
    AES_set_decrypt_key(aeskey, 128, &aes);

    memset(fmtp, 0, sizeof(fmtp));
    i = 0;
    while ( (arg = strsep(&fmtpstr, " \t")) )
        fmtp[i++] = atoi(arg);

    init_decoder();
    init_buffer();
    init_rtp();      // open a UDP listen port and start a listener; decode into ring buffer
    fflush(stdout);
    init_output();              // resample and output from ring buffer

    char line[128];
    int in_line = 0;
    int n;
    double f;
    while (fgets(line + in_line, sizeof(line) - in_line, stdin)) {
        n = strlen(line);
        if (line[n-1] != '\n') {
            in_line = strlen(line) - 1;
            if (n == sizeof(line)-1)
                in_line = 0;
            continue;
        }
        if (sscanf(line, "vol: %lf\n", &f)) {
            assert(f<=0);
            if (debug)
                ATLTRACE( "VOL: %lf\n", f);
            volume = pow(10.0,0.05*f);
            fix_volume = 65536.0 * volume;
            continue;
        }
        if (!strcmp(line, "exit\n")) {
            exit(0);
        }
        if (!strcmp(line, "flush\n")) {
            pthread_mutex_lock(&ab_mutex);
            ab_resync();
            pthread_mutex_unlock(&ab_mutex);
            if (debug)
                ATLTRACE( "FLUSH\n");
        }
    }
    ATLTRACE( "bye!\n");
    fflush(stderr);

    return EXIT_SUCCESS;
}

#endif

void init_buffer(void) {
	if (audio_buffer_init)
	{
		int i;
		for (i=0; i<BUFFER_FRAMES; i++)
		{
			audio_buffer[i].data = (short *) malloc(OUTFRAME_BYTES);
		}
		audio_buffer_init = false;
	}
	ab_resync();
}

void ab_resync(void) {
    int i;
    for (i=0; i<BUFFER_FRAMES; i++)
        audio_buffer[i].ready = 0;
    ab_synced = 0;
}

// the sequence numbers will wrap pretty often.
// this returns true if the second arg is after the first
static inline int seq_order(seq_t a, seq_t b) {
    signed short d = b - a;
    return d > 0;
}

void alac_decode(short *dest, char *buf, int len) {
    unsigned char packet[MAX_PACKET];
    assert(len<=MAX_PACKET);

    unsigned char iv[16];
    int i;
    memcpy(iv, aesiv, sizeof(iv));
    for (i=0; i+16<=len; i += 16)
        AES_cbc_encrypt((unsigned char*)buf+i, packet+i, 0x10, &aes, iv, AES_DECRYPT);
    if (len & 0xf)
        memcpy(packet+i, buf+i, len & 0xf);

    int outsize;

    decode_frame(decoder_info, packet, dest, &outsize);

    assert(outsize == FRAME_BYTES);
}

void buffer_put_packet(seq_t seqno, char *data, int len) {
    volatile abuf_t *abuf = 0;
    short read;
    short buf_fill;

#ifdef _WIN32
	ab_mutex.Lock();
#else
    pthread_mutex_lock(&ab_mutex);
#endif
    if (!ab_synced) {
        ab_write = seqno;
        ab_read = seqno-1;
        ab_synced = 1;
    }
    if (seqno == ab_write+1) {                  // expected packet
        abuf = audio_buffer + BUFIDX(seqno);
        ab_write = seqno;
    } else if (seq_order(ab_write, seqno)) {    // newer than expected
        rtp_request_resend(ab_write, seqno-1);
        abuf = audio_buffer + BUFIDX(seqno);
        ab_write = seqno;
    } else if (seq_order(ab_read, seqno)) {     // late but not yet played
        abuf = audio_buffer + BUFIDX(seqno);
    } else {    // too late.
        ATLTRACE( "\nlate packet %04X (%04X:%04X)\n", seqno, ab_read, ab_write);
    }
    buf_fill = ab_write - ab_read;
#ifdef _WIN32
	ab_mutex.Unlock();
#else
    pthread_mutex_unlock(&ab_mutex);
#endif
    if (abuf) {
        alac_decode(abuf->data, data, len);
        abuf->ready = 1;
    }

    if (ab_buffering && buf_fill >= buffer_start_fill)
#ifdef _WIN32
		ab_buffer_ready.Set();
#else
        pthread_cond_signal(&ab_buffer_ready);
#endif
	if (!ab_buffering) {
        // check if the t+10th packet has arrived... last-chance resend
        read = ab_read + 10;
        abuf = audio_buffer + BUFIDX(read);
        if (!abuf->ready)
            rtp_request_resend(read, read);
    }
}


struct sockaddr_in6 rtp_client_v6;
struct sockaddr_in	rtp_client_v4;
struct sockaddr_in* rtp_client	= NULL;
int					rtp_size	= 0;

#ifdef _WIN32
unsigned int __stdcall rtp_thread_func(void *arg) {
#else
void *rtp_thread_func(void *arg) {
#endif

    socklen_t si_len = rtp_size;
    char packet[MAX_PACKET];
    char *pktp;
    seq_t seqno;
    ssize_t plen;
    int sock = rtp_sockets[0], csock = rtp_sockets[1];
    int readsock;
    char type;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    FD_SET(csock, &fds);

    while (select(csock>sock ? csock+1 : sock+1, &fds, 0, 0, 0)!=-1) {
        if (FD_ISSET(sock, &fds)) {
            readsock = sock;
        } else {
            readsock = csock;
        }
        FD_SET(sock, &fds);
        FD_SET(csock, &fds);

        plen = recvfrom(readsock, packet, sizeof(packet), 0, (struct sockaddr*)rtp_client, &si_len);
        if (plen < 0)
            continue;
        assert(plen<=MAX_PACKET);

		DataPacketHeader* pHeader = (DataPacketHeader*)packet;
        
		type = pHeader->getPayloadType();

		if (type == 0x60 || type == 0x56) {   // audio data / resend
            pktp = packet;
            if (type==0x56) {
                pktp += 4;
                plen -= 4;
            }
            seqno = ntohs(*(unsigned short *)(pktp+2));
            buffer_put_packet(seqno, pktp+12, plen-12);
        }
    }

    return 0;
}

void rtp_request_resend(seq_t first, seq_t last) {
    if (seq_order(last, first))
        return;

    ATLTRACE( "requesting resend on %d packets (port %d)\n", last-first+1, controlport);

    unsigned char req[8];    // *not* a standard RTCP NACK
    req[0] = 0x80;
    req[1] = 0x55|0x80;  // Apple 'resend'
    *(unsigned short *)(req+2) = htons(1);  // our seqnum
    *(unsigned short *)(req+4) = htons(first);  // missed seqnum
    *(unsigned short *)(req+6) = htons(last-first+1);  // count

    rtp_client->sin_port = htons(controlport);
    sendto(rtp_sockets[1], (const char *)req, sizeof(req), 0, (struct sockaddr *)rtp_client, rtp_size);
}


int init_rtp(CRaopContext* pContext) 
{
    struct sockaddr_in si;
    memset(&si, 0, sizeof(si));
    
	struct sockaddr_in6 si6;
    memset(&si6, 0, sizeof(si6));

    si.sin_family		= AF_INET;
    si.sin_addr.s_addr	= htonl(INADDR_ANY);

	si6.sin6_family		= AF_INET6;
    si6.sin6_addr		= in6addr_any;
    si6.sin6_flowinfo	= 0;

    int sock = -1, csock = -1;    // data and control (we treat the streams the same here)

    unsigned short port = 6000;
    
	USHORT type = pContext->m_bV4 ? AF_INET : AF_INET6;

	while(1) 
	{
        if(sock < 0)
            sock = socket(type, SOCK_DGRAM, IPPROTO_UDP);

		if (sock==-1)
		{
            die("Can't create data socket!");
			return -1;
		}

        if(csock < 0)
            csock = socket(type, SOCK_DGRAM, IPPROTO_UDP);
        if (csock==-1)
		{
            die("Can't create control socket!");
			return -1;
		}
		int bind1;
		int bind2;

		if (type == AF_INET)
		{
			rtp_size	= sizeof(rtp_client_v4);
			rtp_client	= &rtp_client_v4;

			si.sin_port = htons(port);
			bind1 = bind(sock, (const sockaddr*)&si, sizeof(si));
			si.sin_port = htons(port + 1);
			bind2 = bind(csock, (const sockaddr*)&si, sizeof(si));
		}
		else
		{
			rtp_size	= sizeof(rtp_client_v6);
			rtp_client	= (sockaddr_in*)&rtp_client_v6;

			si6.sin6_port = htons(port);
			bind1 = bind(sock, (const sockaddr*)&si6, sizeof(si6));
			si6.sin6_port = htons(port + 1);
			bind2 = bind(csock, (const sockaddr*)&si6, sizeof(si6));
		}

        if(bind1 != -1 && bind2 != -1) break;
        if(bind1 != -1) { close(sock); sock = -1; }
        if(bind2 != -1) { close(csock); csock = -1; }

        port += 3;
    }
#ifndef _WIN32
    printf("port: %d\n", port); // let our handler know where we end up listening
    printf("cport: %d\n", port+1);
#endif
    rtp_sockets[0] = sock;
    rtp_sockets[1] = csock;
    pthread_create(&rtp_thread, NULL, rtp_thread_func, (void *)rtp_sockets);

    return port;
}

void deinit_rtp(void) {
	if (rtp_thread != NULL)
	{
		close(rtp_sockets[0]);
	    close(rtp_sockets[1]);

#ifdef _WIN32
		WaitForSingleObject((HANDLE)rtp_thread, INFINITE);
		CloseHandle(rtp_thread);
		rtp_thread = NULL;
#endif
	}
}

static short lcg_rand(void) 
{ 
	static unsigned long lcg_prev = 12345; 

	lcg_prev = lcg_prev * 69069 + 3; 
	return lcg_prev & 0xffff; 
} 

static inline short dithered_vol(short sample) {
    static short rand_a, rand_b;
    long out;

    out = (long)sample * fix_volume;
    if (fix_volume < 0x10000) {
		rand_b = rand_a;
		rand_a = lcg_rand();
        out += rand_a;
        out -= rand_b;
    }
    return out>>16;
}

typedef struct {
    double hist[2];
    double a[2];
    double b[3];
} biquad_t;

static void biquad_init(biquad_t *bq, double a[], double b[]) {
    bq->hist[0] = bq->hist[1] = 0.0;
    memcpy(bq->a, a, 2*sizeof(double));
    memcpy(bq->b, b, 3*sizeof(double));
}

static void biquad_lpf(biquad_t *bq, double freq, double Q) {
    double w0 = 2*M_PI*freq/((float)sampling_rate/(float)frame_size);
    double alpha = sin(w0)/(2.0*Q);

    double a_0 = 1.0 + alpha;
    double b[3], a[2];
    b[0] = (1.0-cos(w0))/(2.0*a_0);
    b[1] = (1.0-cos(w0))/a_0;
    b[2] = b[0];
    a[0] = -2.0*cos(w0)/a_0;
    a[1] = (1-alpha)/a_0;

    biquad_init(bq, a, b);
}

static double biquad_filt(biquad_t *bq, double in) {
    double w = in - bq->a[0]*bq->hist[0] - bq->a[1]*bq->hist[1];
	double out = bq->b[1]*bq->hist[0] + bq->b[2]*bq->hist[1] + bq->b[0]*w;
    bq->hist[1] = bq->hist[0];
    bq->hist[0] = w;

    return out;
}

double bf_playback_rate = 1.0;

static double bf_est_drift = 0.0;   // local clock is slower by
static biquad_t bf_drift_lpf;
static double bf_est_err = 0.0, bf_last_err;
static biquad_t bf_err_lpf, bf_err_deriv_lpf;
static double desired_fill;
static int fill_count;

void bf_est_reset(short fill) {
    biquad_lpf(&bf_drift_lpf, 1.0/180.0, 0.3);
    biquad_lpf(&bf_err_lpf, 1.0/10.0, 0.25);
    biquad_lpf(&bf_err_deriv_lpf, 1.0/2.0, 0.2);
    fill_count = 0;
    bf_playback_rate = 1.0;
    bf_est_err = bf_last_err = 0;
    desired_fill = fill_count = 0;
}
void bf_est_update(short fill) {
    if (fill_count < 1000) {
        desired_fill += (double)fill/1000.0;
        fill_count++;
        return;
    }

#define CONTROL_A   (1e-4)
#define CONTROL_B   (1e-1)

    double buf_delta = fill - desired_fill;
    bf_est_err = biquad_filt(&bf_err_lpf, buf_delta);
    double err_deriv = biquad_filt(&bf_err_deriv_lpf, bf_est_err - bf_last_err);

    bf_est_drift = biquad_filt(&bf_drift_lpf, CONTROL_B*(bf_est_err*CONTROL_A + err_deriv) + bf_est_drift);

    if (debug)
        ATLTRACE( "bf %d err %f drift %f desiring %f ed %f estd %f\r", fill, bf_est_err, bf_est_drift, desired_fill, err_deriv, err_deriv + CONTROL_A*bf_est_err);
    bf_playback_rate = 1.0 + CONTROL_A*bf_est_err + bf_est_drift;

    bf_last_err = bf_est_err;
}

// get the next frame, when available. return 0 if underrun/stream reset.
short *buffer_get_frame(void) {
    short buf_fill;
    seq_t read;

#ifdef _WIN32
	ab_mutex.Lock();
#else
    pthread_mutex_lock(&ab_mutex);
#endif
    buf_fill = ab_write - ab_read;
    if (buf_fill < 1 || !ab_synced) {    // init or underrun. stop and wait
        if (ab_synced)
            ATLTRACE( "\nunderrun.\n");

        ab_buffering = 1;
#ifdef _WIN32
		ab_mutex.Unlock();
		WaitForSingleObject(ab_buffer_ready, INFINITE);
		ab_mutex.Lock();
#else
        pthread_cond_wait(&ab_buffer_ready, &ab_mutex);
#endif
		ab_read++;
        buf_fill = ab_write - ab_read;
#ifdef _WIN32
		ab_mutex.Unlock();
#else
        pthread_mutex_unlock(&ab_mutex);
#endif
        bf_est_reset(buf_fill);
        return 0;
    }
    if (buf_fill >= BUFFER_FRAMES) {   // overrunning! uh-oh. restart at a sane distance
        ATLTRACE( "\noverrun.\n");
        ab_read = ab_write - buffer_start_fill;
    }
    read = ab_read;
    ab_read++;
#ifdef _WIN32
	ab_mutex.Unlock();
#else
    pthread_mutex_unlock(&ab_mutex);
#endif
    buf_fill = ab_write - ab_read;
    bf_est_update(buf_fill);

    volatile abuf_t *curframe = audio_buffer + BUFIDX(read);
    if (!curframe->ready) {
        ATLTRACE( "\nmissing frame.\n");
        memset(curframe->data, 0, FRAME_BYTES);
    }
    curframe->ready = 0;
    return curframe->data;
}

int stuff_buffer(double playback_rate, short *inptr, short *outptr) {
    int i;
    int stuffsamp = frame_size;
    int stuff = 0;
    double p_stuff;

    p_stuff = 1.0 - pow(1.0 - fabs(playback_rate-1.0), frame_size);

    if (rand() < (p_stuff*RAND_MAX)) {
        stuff = playback_rate > 1.0 ? -1 : 1;
        stuffsamp = rand() % (frame_size - 1);
    }

    for (i=0; i<stuffsamp; i++) {   // the whole frame, if no stuffing
        *outptr++ = dithered_vol(*inptr++);
        *outptr++ = dithered_vol(*inptr++);
    };
    if (stuff) {
        if (stuff==1) {
            if (debug)
                ATLTRACE( "+++++++++\n");
            // interpolate one sample
            *outptr++ = dithered_vol(((long)inptr[-2] + (long)inptr[0]) >> 1);
            *outptr++ = dithered_vol(((long)inptr[-1] + (long)inptr[1]) >> 1);
        } else if (stuff==-1) {
            if (debug)
                ATLTRACE( "---------\n");
            inptr++;
            inptr++;
        }
        for (i=stuffsamp; i<frame_size + stuff; i++) {
            *outptr++ = dithered_vol(*inptr++);
            *outptr++ = dithered_vol(*inptr++);
        }
    }

    return frame_size + stuff;
}

#ifdef _WIN32
unsigned int __stdcall audio_thread_func(void *arg) 
#else
void *audio_thread_func(void *arg) 
#endif
{
    int play_samples;

#ifndef _WIN32
    signed short buf_fill __attribute__((unused));
#endif
	signed short *inbuf, *outbuf;
    outbuf = (short*)malloc(OUTFRAME_BYTES);

    while (1) {
        do {
            inbuf = buffer_get_frame();
        } while (!inbuf);

        play_samples = stuff_buffer(bf_playback_rate, inbuf, outbuf);

#ifndef _WIN32
        if (pipename) {
            if (pipe_handle == -1) {
                // attempt to open pipe - block if there are no readers
                pipe_handle = open(pipename, O_WRONLY);
            }

            // only write if pipe open (there's a reader)
            if (pipe_handle != -1) {
                 if (write(pipe_handle, outbuf, play_samples*4) == -1) {
                    // write failed - do anything here?
                    // SIGPIPE is handled elsewhere...
                 }
            }
        } else 
#endif		
		{
            ao_play(libao_dev, (char *)outbuf, play_samples*4);
        }
    }

    return 0;
}

#define NUM_CHANNELS 2

#ifndef _WIN32

void handle_broken_fifo() {
    close(pipe_handle);
    pipe_handle = -1;
}

void init_pipe(char* pipe) {
    // make the FIFO and catch the broken pipe signal
    mknod(pipe, S_IFIFO | 0644, 0);
    signal(SIGPIPE, handle_broken_fifo);
}

#endif

void deinit_ao()
{
	if (libao_dev != NULL)
	{
		ao_close(libao_dev);
		libao_dev = NULL;
	}
}

void init_ao() 
{
	if (libao_dev == NULL)
	{
		ao_initialize();

		int driver;

		if (libao_driver) 
		{
			// if a libao driver is specified on the command line, use that
			driver = ao_driver_id(libao_file == NULL && libao_hfile == INVALID_HANDLE_VALUE ? libao_driver : libao_hfile != INVALID_HANDLE_VALUE ? "raw" : "au");
			if (driver == -1) {
				die("Could not find requested ao driver");
			}
		} 
		else {
			// otherwise choose the default
			driver = ao_default_driver_id();
		}

		ao_sample_format fmt;
		memset(&fmt, 0, sizeof(fmt));
	
		fmt.bits = 16;
		fmt.rate = sampling_rate;
		fmt.channels = NUM_CHANNELS;
		fmt.byte_format = AO_FMT_NATIVE;
	
		ao_option *ao_opts = NULL;
		if(libao_deviceid) {
			ao_append_option(&ao_opts, "id", libao_deviceid);
		} else if(libao_devicename){
			ao_append_option(&ao_opts, "dev", libao_devicename);
		}

		if (libao_file == NULL && libao_hfile == INVALID_HANDLE_VALUE)
		{
			libao_dev = ao_open_live(driver, &fmt, ao_opts);
			if (libao_dev == NULL) {
				die("Could not open ao device");
			}
		}
		else
		{
			if (libao_hfile != INVALID_HANDLE_VALUE)
			{
				libao_dev = ao_open_file2(driver, libao_hfile, &fmt, ao_opts);
				if (libao_dev == NULL) {
					die("Could not open ao stdout device");
				}
			}
			else
			{
				libao_dev = ao_open_file(driver, libao_file, TRUE, &fmt, ao_opts);
				if (libao_dev == NULL) {
					die("Could not open ao file device");
				}
			}
		}
	}
}

int init_output(void) 
{
	init_ao();

	if (audio_thread != NULL)
		return 0;

    pthread_create(&audio_thread, NULL, audio_thread_func, NULL);

    return 0;
}


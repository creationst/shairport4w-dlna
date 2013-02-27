/*
 *
 *  ao_wmm.c
 *
 *      Original Copyright (C) Benjamin Gerard - March 2007
 *      Modifications Copyright (C) Frank Friemel - April 2011
 *
 *  This file is originally part of libao, a cross-platform library.  See
 *  README for a history of this source code.
 *
 *  libao is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  libao is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ********************************************************************

 last mod: $Id: ao_wmm.c 17629 2010-11-18 12:04:46Z xiphmont $

 ********************************************************************/

//#define PREPARE_EACH
#define _CRT_SECURE_NO_DEPRECATE
#define AO_BUILDING_LIBAO
#define HAVE_WMM

#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <Ks.h>
#include <ksmedia.h>

#include <stdlib.h>
#include <string.h>

#include <stdarg.h>
#include <stdio.h>
#include <io.h>
 
#include <fcntl.h>
 

#ifndef KSDATAFORMAT_SUBTYPE_PCM
#define KSDATAFORMAT_SUBTYPE_PCM (GUID) {0x00000001,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}
#endif


#include "ao/ao.h"
/* #include "ao/plugin.h" */

#define GALLOC_WVHD_TYPE (GHND)
#define GALLOC_DATA_TYPE (GHND)

static const char * mmerror(MMRESULT mmrError)
{
  static char mmbuffer[1024];
  int len;
  sprintf(mmbuffer,"mm:%d ",(int)mmrError);
  len = (int)strlen(mmbuffer);
  waveOutGetErrorTextA(mmrError, mmbuffer+len, sizeof(mmbuffer)-len);
  mmbuffer[sizeof(mmbuffer)-1] = 0;
  return mmbuffer;
}

static char * ao_wmm_options[] = {"dev", "id", "matrix","verbose","quiet","debug"};
static ao_info ao_wmm_info =
  {
    /* type             */ AO_TYPE_LIVE,
    /* name             */ "WMM audio driver output ",
    /* short-name       */ "wmm",
    /* author           */ "Benjamin Gerard <benjihan@users.sourceforge.net>",
    /* comment          */ "Outputs audio to the Windows MultiMedia driver.",
    /* prefered format  */ AO_FMT_LITTLE,
    /* priority         */ 20,
    /* options          */ ao_wmm_options,
    /* # of options     */ sizeof(ao_wmm_options)/sizeof(*ao_wmm_options)
  };

typedef struct {
  WAVEHDR wh;          /* waveheader                        */
  char *  data;        /* sample data ptr                   */
  int     idx;         /* index of this header              */
  int     count;       /* current byte count                */
  int     length;      /* size of data                      */
  int     sent;        /* set when header is sent to device */
} myWH_t;

typedef struct ao_wmm_internal {
  UINT  id;             /* device id                       */
  HWAVEOUT hwo;         /* waveout handler                 */
  WAVEOUTCAPSA caps;     /* device caps                     */
  WAVEFORMATEXTENSIBLE wavefmt; /* sample format           */

  int opened;           /* device has been opened          */
  int prepared;         /* waveheaders have been prepared  */
  int blocks;           /* number of blocks (wave headers) */
  int splPerBlock;      /* sample per blocks.              */
  int msPerBlock;       /* millisecond per block (approx.) */

  void * bigbuffer;     /* Allocated buffer for waveheaders and sound data */
  myWH_t * wh;          /* Pointer to waveheaders in bigbuffer             */
  BYTE * spl;           /* Pointer to sound data in bigbuffer              */

  int sent_blocks;      /* Number of waveheader sent (not ack).        */
  int full_blocks;      /* Number of waveheader full (ready to send).  */
  int widx;             /* Index to the block being currently filled.  */
  int ridx;             /* Index to the block being sent.              */

} ao_wmm_internal;

int ao_wmm_test(void)
{
  return 1; /* This plugin works in default mode */
}

ao_info *ao_wmm_driver_info(void)
{
  return &ao_wmm_info;
}

int ao_wmm_set_option(ao_device *device,
                      const char *key, const char *value)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;
  int res = 0;

  if (!strcmp(key, "dev")) {
    if (!strcmp(value,"default")) {
      key = "id";
      value = "0";
    } else {
      WAVEOUTCAPSA caps;
      int i, max = waveOutGetNumDevs();

      //adebug("searching for device %s among %d\n", value, max);
      for (i=0; i<max; ++i) {
        MMRESULT mmres = waveOutGetDevCapsA(i, &caps, sizeof(caps));
        if (mmres == MMSYSERR_NOERROR) {
          res = !strcmp(value, caps.szPname);
          //adebug("checking id=%d, name='%s', ver=%d.%d  => [%s]\n",
                //i,caps.szPname,caps.vDriverVersion>>8,caps.vDriverVersion&255,res?"YES":"no");
          if (res) {
            internal->id   = i;
            internal->caps = caps;
            break;
          }
        } else {
          //OutputDebugStringA("waveOutGetDevCaps(%d) => %s",i,mmerror(mmres));
        }
      }
      goto finish;
    }
  }

  if (!strcmp(key,"id")) {
    MMRESULT mmres;
    WAVEOUTCAPSA caps;

    int id  = strtol(value,0,0);
    int max = waveOutGetNumDevs();

    if (id >= 0 &&  id <= max) {
      if (id-- == 0) {
        //adebug("set default wavemapper\n");
        id = WAVE_MAPPER;
      }
      mmres = waveOutGetDevCapsA(id, &caps, sizeof(caps));

      if (mmres == MMSYSERR_NOERROR) {
        res = 1;
        //adebug("checking id=%d, name='%s', ver=%d.%d  => [YES]\n",
              //id,caps.szPname,caps.vDriverVersion>>8,caps.vDriverVersion&255);
        internal->id   = id;
        internal->caps = caps;
      } else {
        //OutputDebugStringA("waveOutGetDevCaps(%d) => %s",id,mmerror(mmres));
      }
    }
  }

 finish:
  return res;
}


int ao_wmm_device_init(ao_device *device)
{
  ao_wmm_internal *internal;
  int res;

  internal = (ao_wmm_internal *) malloc(sizeof(ao_wmm_internal));
  device->internal = internal;
  if (internal != NULL) {
    memset(internal,0,sizeof(ao_wmm_internal));
    internal->id          = WAVE_MAPPER;
    internal->blocks      = 32;
    internal->splPerBlock = 512;
    /* set default device */
    ao_wmm_set_option(device,"id","0");
  }

  res = internal != NULL;

  device->output_matrix = strdup("L,R,C,LFE,BL,BR,CL,CR,BC,SL,SR");
  device->output_matrix_order = AO_OUTPUT_MATRIX_COLLAPSIBLE;

  return res;
}

static int _ao_open_device(ao_device *device)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;
  int res;
  MMRESULT mmres;

  mmres =
    waveOutOpen(&internal->hwo,
		internal->id,
		&internal->wavefmt.Format,
		(DWORD_PTR)0/* waveOutProc */,
		(DWORD_PTR)device,
		CALLBACK_NULL/* |WAVE_FORMAT_DIRECT */|WAVE_ALLOWSYNC);

  if(mmres == MMSYSERR_NOERROR){
    //adebug("waveOutOpen id=%d, channels=%d, bits=%d, rate %d => SUCCESS\n",
          //internal->id,
          //internal->wavefmt.Format.nChannels,
          //internal->wavefmt.Format.wBitsPerSample,
          //internal->wavefmt.Format.nSamplesPerSec);
  }else{
    /*OutputDebugStringA("waveOutOpen id=%d, channels=%d, bits=%d, rate %d => FAILED\n",
          internal->id,
          internal->wavefmt.Format.nChannels,
          internal->wavefmt.Format.wBitsPerSample,
          internal->wavefmt.Format.nSamplesPerSec);*/
  }

  if (mmres == MMSYSERR_NOERROR) {
    UINT id;
    if (MMSYSERR_NOERROR == waveOutGetID(internal->hwo,&id)) {
      internal->id = id;
    }
  }

  res = (mmres == MMSYSERR_NOERROR);
  return res;
}

static int _ao_close_device(ao_device *device)
{
  ao_wmm_internal * internal = (ao_wmm_internal *) device->internal;
  int res;
  MMRESULT mmres;

  mmres = waveOutClose(internal->hwo);
  if(mmres == MMSYSERR_NOERROR) {
    //adebug("waveOutClose(%d)\n => %s\n", internal->id, mmerror(mmres));
  }else{
   // OutputDebugStringA("waveOutClose(%d)\n => %s\n", internal->id, mmerror(mmres));
  }
  res = (mmres == MMSYSERR_NOERROR);

  return res;
}

static int _ao_alloc_wave_headers(ao_device *device)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;
  int bytesPerBlock = internal->wavefmt.Format.nBlockAlign * internal->splPerBlock;
  /*   int bytes = internal->blocks * (sizeof(WAVEHDR) + bytesPerBlock); */
  int bytes = internal->blocks * (sizeof(*internal->wh) + bytesPerBlock);
  int res;
  MMRESULT mmres;

  //adebug("_ao_alloc_wave_headers blocks=%d, bytes/blocks=%d, total=%d\n",
//         internal->blocks,bytesPerBlock,bytes);

  internal->bigbuffer = malloc(bytes);
  if (internal->bigbuffer != NULL) {
    int i;
    BYTE * b;

    memset(internal->bigbuffer,0,bytes);
    internal->wh = (myWH_t *)internal->bigbuffer;
    internal->spl = (LPBYTE) (internal->wh+internal->blocks);
    for (i=0, b=internal->spl; i<internal->blocks; ++i, b+=bytesPerBlock) {
      internal->wh[i].data = (char*)b;
      internal->wh[i].wh.lpData = internal->wh[i].data;
      internal->wh[i].length = bytesPerBlock;
      internal->wh[i].wh.dwBufferLength = internal->wh[i].length;
      internal->wh[i].wh.dwUser = (DWORD_PTR)device;
      mmres = waveOutPrepareHeader(internal->hwo,
				   &internal->wh[i].wh,sizeof(WAVEHDR));
      if (MMSYSERR_NOERROR != mmres) {
        //OutputDebugStringA("waveOutPrepareHeader(%d) => %s\n",i, mmerror(mmres));
        break;
      }
    }
    if (i<internal->blocks) {
      while (--i >= 0) {
        waveOutUnprepareHeader(internal->hwo,
			       &internal->wh[i].wh,sizeof(WAVEHDR));
      }
      free(internal->bigbuffer);
      internal->wh        = 0;
      internal->spl       = 0;
      internal->bigbuffer = 0;
    } else {
      /* all ok ! */
    }
  } else {
    //adebug("malloc() => FAILED\n");
  }

  res = (internal->bigbuffer != NULL);
  if(!res){
    OutputDebugStringA("_ao_alloc_wave_headers() => FAILED\n");
  }else{
    //adebug("_ao_alloc_wave_headers() => success\n");
  }
  return res;
}

static int _ao_get_free_block(ao_device * device);
static int _ao_wait_wave_headers(ao_device *device, int wait_all)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;
  int res = 1;

  //adebug("wait for %d blocks (%swait all)\n",
         //internal->sent_blocks,wait_all?"":"not ");

  while (internal->sent_blocks > 0) {
    int n;
    _ao_get_free_block(device);
    n = internal->sent_blocks;
    if (n > 0) {
      unsigned int ms = (internal->msPerBlock>>1)+1;
      if (wait_all) ms *= n;
      //adebug("sleep for %ums wait on %d blocks\n",ms, internal->sent_blocks);
      Sleep(ms);
    }
  }

  res &= !internal->sent_blocks;
  if(!res){
    OutputDebugStringA("_ao_wait_wave_headers => FAILED\n");
  }else{
    //adebug("_ao_wait_wave_headers => success\n");
  }
  return res;
}

static int _ao_free_wave_headers(ao_device *device)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;
  MMRESULT mmres;
  int res = 1;

  if (internal->wh) {
    int i;

    /* Reset so we dont need to wait ... Just a satefy net
     * since _ao_wait_wave_headers() has been called once before.
     */
    mmres = waveOutReset(internal->hwo);
    //adebug("waveOutReset(%d) => %s\n", internal->id, mmerror(mmres));
    /* Wait again to be sure reseted waveheaders has been released. */
    _ao_wait_wave_headers(device,0);

    for (i=internal->blocks; --i>=0; ) {
      mmres = waveOutUnprepareHeader(internal->hwo,
				     &internal->wh[i].wh,sizeof(WAVEHDR));
      /*if (mmres != MMSYSERR_NOERROR)
        OutputDebugStringA("waveOutUnprepareHeader(%d) => %s\n", i, mmerror(mmres));*/

      res &= mmres == MMSYSERR_NOERROR;
    }
    internal->wh  = 0;
    internal->spl = 0;
  }

  if(!res){
    OutputDebugStringA("_ao_alloc_wave_headers() => FAILED\n");
  }else{
    //adebug("_ao_alloc_wave_headers() => success\n");
  }
  return res;
}


/*
 * open the audio device for writing to
 */
int ao_wmm_open(ao_device * device, ao_sample_format * format)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;
  int res = 0;
  WAVEFORMATEXTENSIBLE wavefmt;

  //adebug("open() channels=%d, bits=%d, rate=%d, format %d(%s)\n",
    //     device->output_channels,format->bits,format->rate,format->byte_format,
    //     format->byte_format==AO_FMT_LITTLE
      //   ?"little"
       //  :(format->byte_format==AO_FMT_NATIVE
        //   ?"native"
         //  :(format->byte_format==AO_FMT_BIG?"big":"unknown")));

  if(internal->opened) {
    OutputDebugStringA("open() => already opened\n");
    goto error_no_close;
  }

  /* Force LITTLE as specified by WIN32 API */
  format->byte_format = AO_FMT_LITTLE;
  device->driver_byte_format = AO_FMT_LITTLE;

  /* $$$ WMM 8 bit samples are unsigned... Not sure for ao ... */
  /* Yes, ao 8 bit PCM is unsigned -- Monty */

  /* Make sample format */
  memset(&wavefmt,0,sizeof(wavefmt));
  wavefmt.Format.wFormatTag          = WAVE_FORMAT_EXTENSIBLE;
  wavefmt.Format.nChannels           = device->output_channels;
  wavefmt.Format.wBitsPerSample      = (((format->bits+7)>>3)<<3);
  wavefmt.Format.nSamplesPerSec      = format->rate;
  wavefmt.Format.nBlockAlign         = (wavefmt.Format.wBitsPerSample>>3)*wavefmt.Format.nChannels;
  wavefmt.Format.nAvgBytesPerSec     = wavefmt.Format.nSamplesPerSec*wavefmt.Format.nBlockAlign;
  wavefmt.Format.cbSize              = 22;
  wavefmt.Samples.wValidBitsPerSample = format->bits;
  wavefmt.SubFormat           = KSDATAFORMAT_SUBTYPE_PCM;
  wavefmt.dwChannelMask       = device->output_mask;

  internal->wavefmt       = wavefmt;

  /* $$$ later this should be optionnal parms */
  internal->blocks      = 64;
  internal->splPerBlock = 512;
  internal->msPerBlock  =
    (internal->splPerBlock * 1000 + format->rate - 1) / format->rate;

  /* Open device */
  if(!_ao_open_device(device))
    goto error;
  internal->opened = 1;

  /* Allocate buffers */
  if (!_ao_alloc_wave_headers(device))
    goto error;
  internal->prepared = 1;

  res = 1;
 error:
  if (!res) {
    if (internal->prepared) {
      _ao_free_wave_headers(device);
      internal->prepared = 0;
    }
    if (internal->opened) {
      _ao_close_device(device);
      internal->opened = 0;
    }
  }

 error_no_close:
  if(res){
    //adebug("open() => success\n");
  }else{
    OutputDebugStringA("open() => FAILED\n");
  }
  return res;
}



/* Send a block to audio hardware */
static int _ao_send_block(ao_device *device, const int idx)
{
  ao_wmm_internal * internal = (ao_wmm_internal *) device->internal;
  MMRESULT mmres;

  /* Satanity checks */
  if (internal->wh[idx].sent) {
    //adebug("block %d marked SENT\n",idx);
    return 0;
  }
  if (!!(internal->wh[idx].wh.dwFlags & WHDR_DONE)) {
    //adebug("block %d marked DONE\n",idx);
    return 0;
  }

  /* count <= 0, just pretend it's been sent */
  if (internal->wh[idx].count <= 0) {
    internal->wh[idx].sent = 2; /* set with 2 so we can track these special cases */
    internal->wh[idx].wh.dwFlags |= WHDR_DONE;
    ++internal->sent_blocks;
    return 1;
  }

  internal->wh[idx].wh.dwBufferLength = internal->wh[idx].count;
  internal->wh[idx].count = 0;
  mmres = waveOutWrite(internal->hwo,
		       &internal->wh[idx].wh, sizeof(WAVEHDR));
  internal->wh[idx].sent = (mmres == MMSYSERR_NOERROR);
  /*&& !(internal->wh[idx].wh.dwFlags & WHDR_DONE);*/
  internal->sent_blocks += internal->wh[idx].sent;
  if (mmres != MMSYSERR_NOERROR) {
    //adebug("waveOutWrite(%d) => %s\n",idx,mmerror(mmres));
  }
  return mmres == MMSYSERR_NOERROR;
}

/* Get idx of next free block. */
static int _ao_get_free_block(ao_device * device)
{
  ao_wmm_internal * internal = (ao_wmm_internal *) device->internal;
  const int idx = internal->widx;
  int ridx = internal->ridx;

  while (internal->wh[ridx].sent && !!(internal->wh[ridx].wh.dwFlags & WHDR_DONE)) {
    /* block successfully sent to hardware, release it */
    /*debug("_ao_get_free_block: release block %d\n",ridx);*/
    internal->wh[ridx].sent = 0;
    internal->wh[ridx].wh.dwFlags &= ~WHDR_DONE;

    --internal->full_blocks;
    if (internal->full_blocks<0) {
      //adebug("internal error with full block counter\n");
      internal->full_blocks = 0;
    }

    --internal->sent_blocks;
    if (internal->sent_blocks<0) {
      //adebug("internal error with sent block counter\n");
      internal->sent_blocks = 0;
    }
    if (++ridx >= internal->blocks) ridx = 0;
  }
  internal->ridx = ridx;

  return internal->wh[idx].sent
    ? -1
    : idx;
}

/*
 * play the sample to the already opened file descriptor
 */
int ao_wmm_play(ao_device *device,
                const char *output_samples, uint_32 num_bytes)
{
  int ret = 1;
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;

  while(ret && num_bytes > 0) {
    int n;
    const int idx = _ao_get_free_block(device);

    if (idx == -1) {
      Sleep(internal->msPerBlock);
      continue;
    }

    /* Get free bytes in the block */
    n = internal->wh[idx].wh.dwBufferLength
      - internal->wh[idx].count;

    /* Get amount to copy */
    if (n > (int)num_bytes) {
      n = num_bytes;
    }

    /* Do copy */
    CopyMemory((char*)internal->wh[idx].wh.lpData
	       + internal->wh[idx].count,
	       output_samples, n);

    /* Updates pointers and counters */
    output_samples += n;
    num_bytes -= n;
    internal->wh[idx].count += n;

    /* Is this block full ? */
    if (internal->wh[idx].count
	== internal->wh[idx].wh.dwBufferLength) {
      ++internal->full_blocks;
      if (++internal->widx == internal->blocks) {
	internal->widx = 0;
      }
      ret = _ao_send_block(device,idx);
    }
  }

  //adebug("ao_wmm_play => %d rem => [%s]\n",num_bytes,ret?"success":"error");
  return ret;

}

int ao_wmm_close(ao_device *device)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;
  int ret = 0;

  if (internal->opened && internal->prepared) {
    _ao_wait_wave_headers(device, 1);
  }

  if (internal->prepared) {
    ret = _ao_free_wave_headers(device);
    internal->prepared = 0;
  }

  if (internal->opened) {
    ret = _ao_close_device(device);
    internal->opened = 0;
  }

  return ret;
}

void ao_wmm_device_clear(ao_device *device)
{
  ao_wmm_internal *internal = (ao_wmm_internal *) device->internal;

  if (internal->bigbuffer) {
    free(internal->bigbuffer); internal->bigbuffer = NULL;
  }
  free(internal);
  device->internal=NULL;
}

ao_functions ao_wmm = {
  ao_wmm_test,
  ao_wmm_driver_info,
  ao_wmm_device_init,
  ao_wmm_set_option,
  ao_wmm_open,
  ao_wmm_play,
  ao_wmm_close,
  ao_wmm_device_clear
};

// audio_out.c
/* --- Other constants --- */
#define DEF_SWAP_BUF_SIZE  1024

/* --- Driver Table --- */

typedef struct driver_list {
	ao_functions *functions;
	void *handle;
	struct driver_list *next;
} driver_list;


extern ao_functions ao_null;
extern ao_functions ao_wav;
extern ao_functions ao_raw;
extern ao_functions ao_au;
#ifdef HAVE_SYS_AUDIO_H
extern ao_functions ao_aixs;
#endif
#ifdef HAVE_WMM
extern ao_functions ao_wmm;
#endif
static ao_functions *static_drivers[] = {
	&ao_null, /* Must have at least one static driver! */
	&ao_wav,
	&ao_raw,
	&ao_au,
#ifdef HAVE_SYS_AUDIO_H
	&ao_aixs,
#endif
#ifdef HAVE_WMM
	&ao_wmm,
#endif

	NULL /* End of list */
};

static driver_list *driver_head = NULL;
static ao_config config = {
	NULL /* default_driver */
};

static ao_info **info_table = NULL;
static int driver_count = 0;

/* uses the device messaging and option infrastructure */
static ao_device *ao_global_dummy;
static ao_device ao_global_dummy_storage;
static ao_option *ao_global_options=NULL;

/* ---------- Helper functions ---------- */

/* Clear out all of the library configuration options and set them to
   defaults.   The defaults should match the initializer above. */
static void _clear_config()
{
        memset(ao_global_dummy,0,sizeof(*ao_global_dummy));
        ao_global_dummy = NULL;
        ao_free_options(ao_global_options);
        ao_global_options = NULL;

	free(config.default_driver);
	config.default_driver = NULL;
}




/* If *name is a valid driver name, return its driver number.
   Otherwise, test all of available live drivers until one works. */
static int _find_default_driver_id (const char *name)
{
	int def_id;
	int id;
	ao_info *info;
	driver_list *dl = driver_head;
        ao_device *device = ao_global_dummy;

        //adebug("Testing drivers to find playback default...\n");
	if ( name == NULL || (def_id = ao_driver_id(name)) < 0 ) {
		/* No default specified. Find one among available drivers. */
		def_id = -1;

		id = 0;
		while (dl != NULL) {

			info = dl->functions->driver_info();
                        //adebug("...testing %s\n",info->short_name);
			if ( info->type == AO_TYPE_LIVE &&
			     info->priority > 0 && /* Skip static drivers */
			     dl->functions->test() ) {
				def_id = id; /* Found a usable driver */
                                //adebug("OK, using driver %s\n",info->short_name);
				break;
			}

			dl = dl->next;
			id++;
		}
	}

	return def_id;
}


/* Convert the static drivers table into a linked list of drivers. */
static driver_list* _load_static_drivers(driver_list **end)
{
        ao_device *device = ao_global_dummy;
	driver_list *head;
	driver_list *driver;
	int i;

	/* insert first driver */
	head = driver = (driver_list *)calloc(1,sizeof(driver_list));
	if (driver != NULL) {
		driver->functions = static_drivers[0];
		driver->handle = NULL;
		driver->next = NULL;
                //adebug("Loaded driver %s (built-in)\n",driver->functions->driver_info()->short_name);

		i = 1;
		while (static_drivers[i] != NULL) {
                  driver->next = (driver_list *)calloc(1,sizeof(driver_list));
			if (driver->next == NULL)
				break;

			driver->next->functions = static_drivers[i];
			driver->next->handle = NULL;
			driver->next->next = NULL;

			driver = driver->next;
                        //adebug("Loaded driver %s (built-in)\n",driver->functions->driver_info()->short_name);
			i++;
		}
	}

	if (end != NULL)
		*end = driver;

	return head;
}


/* Load the dynamic drivers from disk and append them to end of the
   driver list.  end points the driver_list node to append to. */
static void _append_dynamic_drivers(driver_list *end)
{
#ifdef HAVE_DLOPEN
	struct dirent *plugin_dirent;
	char *ext;
	struct stat statbuf;
	DIR *plugindir;
	driver_list *plugin;
	driver_list *driver = end;
        ao_device *device = ao_global_dummy;

	/* now insert any plugins we find */
	plugindir = opendir(AO_PLUGIN_PATH);
        //adebug("Loading driver plugins from %s...\n",AO_PLUGIN_PATH);
	if (plugindir != NULL) {
          while ((plugin_dirent = readdir(plugindir)) != NULL) {
            char fullpath[strlen(AO_PLUGIN_PATH) + 1 + strlen(plugin_dirent->d_name) + 1];
            snprintf(fullpath, sizeof(fullpath), "%s/%s",
                     AO_PLUGIN_PATH, plugin_dirent->d_name);
            if (!stat(fullpath, &statbuf) &&
                S_ISREG(statbuf.st_mode) &&
                (ext = strrchr(plugin_dirent->d_name, '.')) != NULL) {
              if (strcmp(ext, SHARED_LIB_EXT) == 0) {
                plugin = _get_plugin(fullpath);
                if (plugin) {
                  driver->next = plugin;
                  plugin->next = NULL;
                  driver = driver->next;
                }
              }
            }
          }

          closedir(plugindir);
	}
#endif
}


/* Compare two drivers based on priority
   Used as compar function for qsort() in _make_info_table() */
static int _compar_driver_priority (const driver_list **a,
				    const driver_list **b)
{
	return memcmp(&((*b)->functions->driver_info()->priority),
		      &((*a)->functions->driver_info()->priority),
		      sizeof(int));
}


/* Make a table of driver info structures for ao_driver_info_list(). */
static ao_info ** _make_info_table (driver_list **head, int *driver_count)
{
	driver_list *list;
	int i;
	ao_info **table;
	driver_list **drivers_table;

	*driver_count = 0;

	/* Count drivers */
	list = *head;
	i = 0;
	while (list != NULL) {
		i++;
		list = list->next;
	}


	/* Sort driver_list */
	drivers_table = (driver_list **) calloc(i, sizeof(driver_list *));
	if (drivers_table == NULL)
		return (ao_info **) NULL;
	list = *head;
	*driver_count = i;
	for (i = 0; i < *driver_count; i++, list = list->next)
		drivers_table[i] = list;
	qsort(drivers_table, i, sizeof(driver_list *),
			(int(*)(const void *, const void *))
			_compar_driver_priority);
	*head = drivers_table[0];
	for (i = 1; i < *driver_count; i++)
		drivers_table[i-1]->next = drivers_table[i];
	drivers_table[i-1]->next = NULL;


	/* Alloc table */
	table = (ao_info **) calloc(i, sizeof(ao_info *));
	if (table != NULL) {
		for (i = 0; i < *driver_count; i++)
			table[i] = drivers_table[i]->functions->driver_info();
	}

	free(drivers_table);

	return table;
}


/* Return the driver struct corresponding to particular driver id
   number. */
static driver_list *_get_driver(int driver_id) {
	int i = 0;
	driver_list *driver = driver_head;

	if (driver_id < 0) return NULL;

	while (driver && (i < driver_id)) {
		i++;
		driver = driver->next;
	}

	if (i == driver_id)
		return driver;

	return NULL;
}


/* Check if driver_id is a valid id number */
static int _check_driver_id(int driver_id)
{
	int i = 0;
	driver_list *driver = driver_head;

	if (driver_id < 0) return 0;

	while (driver && (i <= driver_id)) {
		driver = driver->next;
		i++;
	}

	if (i == (driver_id + 1))
		return 1;

	return 0;
}


/* helper function to convert a byte_format of AO_FMT_NATIVE to the
   actual byte format of the machine, otherwise just return
   byte_format */
static int _real_byte_format(int byte_format)
{
	if (byte_format == AO_FMT_NATIVE) {
		if (ao_is_big_endian())
			return AO_FMT_BIG;
		else
			return AO_FMT_LITTLE;
	} else
		return byte_format;
}


/* Create a new ao_device structure and populate it with data */
static ao_device* _create_device(int driver_id, driver_list *driver,
				 ao_sample_format *format, FILE *file)
{
	ao_device *device;

	device = (ao_device *)calloc(1,sizeof(ao_device));

	if (device != NULL) {
		device->type = driver->functions->driver_info()->type;
		device->driver_id = driver_id;
		device->funcs = driver->functions;
		device->file = file;
		device->machine_byte_format =
		  ao_is_big_endian() ? AO_FMT_BIG : AO_FMT_LITTLE;
		device->client_byte_format =
		  _real_byte_format(format->byte_format);
		device->swap_buffer = NULL;
		device->swap_buffer_size = 0;
		device->internal = NULL;
                device->output_channels = format->channels;
                device->inter_permute = NULL;
                device->output_matrix = NULL;
	}

	return device;
}


/* Expand the swap buffer in this device if it is smaller than
   min_size. */
static int _realloc_swap_buffer(ao_device *device, int min_size)
{
	void *temp;

	if (min_size > device->swap_buffer_size) {
		temp = realloc(device->swap_buffer, min_size);
		if (temp != NULL) {
			device->swap_buffer = (char*)temp;
			device->swap_buffer_size = min_size;
			return 1; /* Success, realloc worked */
		} else
			return 0; /* Fail to realloc */
	} else
		return 1; /* Success, no need to realloc */
}


static void _buffer_zero(char *target,int och,int bytewidth,int ochannels,int bytes){
  int i = och*bytewidth;
  int stride = bytewidth*ochannels;
  switch(bytewidth){
  case 1:
    while(i<bytes){
      ((unsigned char *)target)[i] = 128; /* 8 bit PCM is unsigned in libao */
      i+=stride;
    }
    break;
  case 2:
    while(i<bytes){
      target[i] = 0;
      target[i+1] = 0;
      i+=stride;
    }
    break;
  case 3:
    while(i<bytes){
      target[i] = 0;
      target[i+1] = 0;
      target[i+2] = 0;
      i+=stride;
    }
    break;
  case 4:
    while(i<bytes){
      target[i] = 0;
      target[i+1] = 0;
      target[i+2] = 0;
      target[i+3] = 0;
      i+=stride;
    }
    break;
  }
}

static void _buffer_permute_swap(char *target,int och,int bytewidth,int ochannels,int bytes,
                                 char *source,int ich, int ichannels){
  int o = och*bytewidth;
  int i = ich*bytewidth;
  int ostride = bytewidth*ochannels;
  int istride = bytewidth*ichannels;
  switch(bytewidth){
  case 2:
    while(o<bytes){
      target[o] = source[i+1];
      target[o+1] = source[i];
      o+=ostride;
      i+=istride;
    }
    break;
  case 3:
    while(o<bytes){
      target[o] = source[i+2];
      target[o+1] = source[i+1];
      target[o+2] = source[i];
      o+=ostride;
      i+=istride;
    }
    break;
  case 4:
    while(o<bytes){
      target[o] = source[i+3];
      target[o+1] = source[i+2];
      target[o+2] = source[i+1];
      target[o+3] = source[i];
      o+=ostride;
      i+=istride;
    }
    break;
  }
}

static void _buffer_permute(char *target,int och,int bytewidth,int ochannels,int bytes,
                            char *source,int ich, int ichannels){
  int o = och*bytewidth;
  int i = ich*bytewidth;
  int ostride = bytewidth*ochannels;
  int istride = bytewidth*ichannels;
  switch(bytewidth){
  case 1:
    while(o<bytes){
      target[o] = source[i];
      o+=ostride;
      i+=istride;
    }
    break;
  case 2:
    while(o<bytes){
      target[o] = source[i];
      target[o+1] = source[i+1];
      o+=ostride;
      i+=istride;
    }
    break;
  case 3:
    while(o<bytes){
      target[o] = source[i];
      target[o+1] = source[i+1];
      target[o+2] = source[i+2];
      o+=ostride;
      i+=istride;
    }
    break;
  case 4:
    while(o<bytes){
      target[o] = source[i];
      target[o+1] = source[i+1];
      target[o+2] = source[i+2];
      target[o+3] = source[i+3];
      o+=ostride;
      i+=istride;
    }
    break;
  }
}


/* Swap and copy the byte order of samples from the source buffer to
   the target buffer. */
static void _swap_samples(char *target_buffer, char* source_buffer,
			  uint_32 num_bytes)
{
	uint_32 i;

	for (i = 0; i < num_bytes; i += 2) {
		target_buffer[i] = source_buffer[i+1];
		target_buffer[i+1] = source_buffer[i];
	}
}

/* the channel locations we know right now. code below assumes U is in slot 0, X in 1, M in 2 */
static char *mnemonics[]={
  "X",
  "M",
  "L","C","R","CL","CR","SL","SR","BL","BC","BR","LFE",
  "A1","A2","A3","A4","A5","A6","A7","A8","A9","A10",
  "A11","A12","A13","A14","A15","A16","A17","A18","A19","A20",
  "A21","A22","A23","A24","A25","A26","A27","A28","A29","A30",
  "A31","A32",NULL
};

/* Check the requested matrix string for syntax and mnemonics */
static char *_sanitize_matrix(int maxchannels, char *matrix, ao_device *device){
  if(matrix){
    char *ret = (char*)calloc(strlen(matrix)+1,1); /* can only get smaller */
    char *p=matrix;
    int count=0;
    while(count<maxchannels){
      char *h,*t;
      int m=0;

      /* trim leading space */
      while(*p && isspace(*p))p++;

      /* search for seperator */
      h=p;
      while(*h && *h!=',')h++;

      /* trim trailing space */
      t=h;
      while(t>p && isspace(*(t-1)))t--;

      while(mnemonics[m]){
        if(t-p && !strncmp(mnemonics[m],p,t-p) &&
           strlen(mnemonics[m])==t-p){
          if(count)strcat(ret,",");
          strcat(ret,mnemonics[m]);
          break;
        }
        m++;
      }
      if(!mnemonics[m]){
        /* unrecognized channel mnemonic */
        {
          int i;
          OutputDebugStringA("Unrecognized channel name \"");
          for(i=0;i<t-p;i++)fputc(p[i],stderr);
          ATLTRACE("\" in channel matrix \"%s\"\n",matrix);
        }
        free(ret);
        return NULL;
      }else
        count++;
      if(!*h)break;
      p=h+1;
    }
    return ret;
  }else
    return NULL;
}

static int _find_channel(int needle, char *haystack){
  char *p=haystack;
  int count=0;

  /* X does not map to anything, including X! */
  if(needle==0) return -1;

  while(1){
    char *h;
    int m=0;

    /* search for seperator */
    h=p;
    while(*h && *h!=',')h++;

    while(mnemonics[m]){
      if(!strncmp(mnemonics[needle],p,h-p) &&
         strlen(mnemonics[needle])==h-p)break;
      m++;
    }
    if(mnemonics[m])
      return count;
    count++;
    if(!*h)break;
    p=h+1;
  }
  return -1;
}

static char **_tokenize_matrix(char *matrix){
  char **ret=NULL;
  char *p=matrix;
  int count=0;
  while(1){
    char *h,*t;

    /* trim leading space */
    while(*p && isspace(*p))p++;

    /* search for seperator */
    h=p;
    while(*h && *h!=',')h++;

    /* trim trailing space */
    t=h;
    while(t>p && isspace(*(t-1)))t--;

    count++;
    if(!*h)break;
    p=h+1;
  }

  ret = (char**)calloc(count+1,sizeof(*ret));

  p=matrix;
  count=0;
  while(1){
    char *h,*t;

    /* trim leading space */
    while(*p && isspace(*p))p++;

    /* search for seperator */
    h=p;
    while(*h && *h!=',')h++;

    /* trim trailing space */
    t=h;
    while(t>p && isspace(*(t-1)))t--;

    ret[count] = (char*) calloc(t-p+1,1);
    memcpy(ret[count],p,t-p);
    count++;
    if(!*h)break;
    p=h+1;
  }

  return ret;

}

static void _free_map(char **m){
  char **in=m;
  while(m && *m){
    free(*m);
    m++;
  }
  if(in)free(in);
}

static unsigned int _matrix_to_channelmask(int ch, char *matrix, char *premap, int **mout){
  unsigned int ret=0;
  char *p=matrix;
  int *perm=(*mout=(int*)malloc(ch*sizeof(*mout)));
  int i;
  char **map = _tokenize_matrix(premap);

  for(i=0;i<ch;i++) perm[i] = -1;
  i=0;

  while(1){
    char *h=p;
    int m=0;

    /* search for seperator */
    while(*h && *h!=',')h++;

    while(map[m]){
      if(h-p && !strncmp(map[m],p,h-p) &&
         strlen(map[m])==h-p)
        break;
      m++;
    }
    /* X is a placeholder, X does not map to X */
    if(map[m] && strcmp(map[m],"X")){
      ret |= (1<<m);
      perm[i] = m;
    }
    if(!*h)break;
    p=h+1;
    i++;
  }

  _free_map(map);
  return ret;
}

static char *_channelmask_to_matrix(unsigned int mask, char *premap){
  int m=0;
  int count=0;
  char buffer[257]={0};
  char **map = _tokenize_matrix(premap);

  while(map[m]){
    if(mask & (1<<m)){
      if(count)
        strcat(buffer,",");
      strcat(buffer,map[m]);
      count++;
    }
    m++;
  }
  _free_map(map);
  return strdup(buffer);
}

static int _channelmask_bits(unsigned int mask){
  int count=0;
  while(mask){
    if(mask&1)count++;
    mask/=2;
  }
  return count;
}

static int _channelmask_maxbit(unsigned int mask){
  int count=0;
  int max=-1;
  while(mask){
    if(mask&1)max=count;
    mask/=2;
    count++;
  }
  return max;
}

static char *_matrix_intersect(char *matrix,char *premap){
  char *p=matrix;
  char buffer[257]={0};
  int count=0;
  char **map = _tokenize_matrix(premap);

  while(1){
    char *h=p;
    int m=0;

    /* search for seperator */
    while(*h && *h!=',')h++;

    while(map[m]){
      if(h-p && !strncmp(map[m],p,h-p) &&
         strlen(map[m])==h-p)
        break;
      m++;
    }
    /* X is a placeholder, X does not map to X */
    if(map[m] && strcmp(map[m],"X")){
      if(count)
        strcat(buffer,",");
      strcat(buffer,map[m]);
      count++;
    }

    if(!*h)break;
    p=h+1;
  }

  _free_map(map);
  return strdup(buffer);
}

static int ao_global_load_options(ao_option *options){
  while (options != NULL) {
    if(!strcmp(options->key,"debug")){
      ao_global_dummy->verbose=2;
    }else if(!strcmp(options->key,"verbose")){
      if(ao_global_dummy->verbose<1)ao_global_dummy->verbose=1;
    }else if(!strcmp(options->key,"quiet")){
      ao_global_dummy->verbose=-1;
    }

    options = options->next;
  }

  return 0;

}

static int ao_device_load_options(ao_device *device, ao_option *options){

  while (options != NULL) {
    if(!strcmp(options->key,"matrix")){
      /* If a driver has a static channel mapping mechanism
         (physically constant channel mapping, or at least an
         unvarying set of constants for mapping channels), the
         output_matrix is already set.  An app/user specified
         output mapping trumps. */
      if(device->output_matrix)
        free(device->output_matrix);
      /* explicitly set the output matrix to the requested
         string; devices must not override. */
      device->output_matrix = _sanitize_matrix(32, options->value, device);
      if(!device->output_matrix){
        OutputDebugStringA("Empty or inavlid output matrix\n");
        return AO_EBADOPTION;
      }
      //adebug("Sanitized device output matrix: %s\n",device->output_matrix);
    }else if(!strcmp(options->key,"debug")){
      device->verbose=2;
    }else if(!strcmp(options->key,"verbose")){
      if(device->verbose<1)device->verbose=1;
    }else if(!strcmp(options->key,"quiet")){
      device->verbose=-1;
    }else{
      if (!device->funcs->set_option(device, options->key, options->value)) {
        /* Problem setting options */
        return AO_EOPENDEVICE;
      }
    }

    options = options->next;
  }

  return 0;
}

/* Open a device.  If this is a live device, file == NULL. */
static ao_device* _open_device(int driver_id, ao_sample_format *format,
			       ao_option *options, FILE *file)
{
	ao_functions *funcs;
	driver_list *driver;
	ao_device *device=NULL;
	int result;
        ao_sample_format sformat=*format;
        sformat.matrix=NULL;

	/* Get driver id */
	if ( (driver = _get_driver(driver_id)) == NULL ) {
		errno = AO_ENODRIVER;
		goto error;
	}

	funcs = driver->functions;

	/* Check the driver type */
	if (file == NULL &&
	    funcs->driver_info()->type != AO_TYPE_LIVE) {

		errno = AO_ENOTLIVE;
		goto error;
	} else if (file != NULL &&
		   funcs->driver_info()->type != AO_TYPE_FILE) {

		errno = AO_ENOTFILE;
		goto error;
	}

	/* Make a new device structure */
	if ( (device = _create_device(driver_id, driver,
				      format, file)) == NULL ) {
		errno = AO_EFAIL;
		goto error;
	}

	/* Initialize the device memory; get static channel mapping */
	if (!funcs->device_init(device)) {
		errno = AO_EFAIL;
		goto error;
	}

	/* Load options */
        errno = ao_device_load_options(device,ao_global_options);
        if(errno) goto error;
        errno = ao_device_load_options(device,options);
        if(errno) goto error;

        /* also sanitize the format input channel matrix */
        if(format->matrix){
          sformat.matrix = _sanitize_matrix(format->channels, format->matrix, device);
          /*if(!sformat.matrix)
            awarn("Input channel matrix invalid; ignoring.\n");*/

          /* special-case handling of 'M'. */
          if(sformat.channels==1 && sformat.matrix && !strcmp(sformat.matrix,"M")){
            free(sformat.matrix);
            sformat.matrix=NULL;
          }
        }

        /* If device init was able to declare a static channel mapping
           mechanism, reconcile it to the input now.  Odd drivers that
           need to communicate with a backend device to determine
           channel mapping strategy can still bypass this mechanism
           entirely.  Also, drivers like ALSA may need to adjust
           strategy depending on what device is successfully opened,
           etc, but this still saves work later. */

        if(device->output_matrix && sformat.matrix){
          switch(device->output_matrix_order){
          case AO_OUTPUT_MATRIX_FIXED:
            /* Backend channel ordering is immutable and unused
               channels must still be sent.  Look for the highest
               channel number we are able to map from the input
               matrix. */
            {
              unsigned int mask = _matrix_to_channelmask(sformat.channels,sformat.matrix,
                                                         device->output_matrix,
                                                         &device->input_map);
              int max = _channelmask_maxbit(mask);
              if(max<0){
                OutputDebugStringA("Unable to map any channels from input matrix to output");
                errno = AO_EBADFORMAT;
                goto error;
              }
              device->output_channels = max+1;
              device->output_mask = mask;
              device->inter_matrix = strdup(device->output_matrix);
            }
            break;

          case AO_OUTPUT_MATRIX_COLLAPSIBLE:
            /* the ordering of channels submitted to the backend is
               fixed, but only the channels in use should be present
               in the audio stream */
            {
              unsigned int mask = _matrix_to_channelmask(sformat.channels,sformat.matrix,
                                                         device->output_matrix,
                                                         &device->input_map);
              int channels = _channelmask_bits(mask);
              if(channels<0){
                OutputDebugStringA("Unable to map any channels from input matrix to output");
                errno = AO_EBADFORMAT;
                goto error;
              }
              device->output_channels = channels;
              device->output_mask = mask;
              device->inter_matrix = _channelmask_to_matrix(mask,device->output_matrix);
            }
            break;

          case AO_OUTPUT_MATRIX_PERMUTABLE:
            /* The ordering of channels is freeform.  Only the
               channels in use should be sent, and they may be sent in
               any order.  If possible, leave the input ordering
               unchanged */
            {
              unsigned int mask = _matrix_to_channelmask(sformat.channels,sformat.matrix,
                                                         device->output_matrix,
                                                         &device->input_map);
              int channels = _channelmask_bits(mask);
              if(channels<0){
                OutputDebugStringA("Unable to map any channels from input matrix to output");
                errno = AO_EBADFORMAT;
                goto error;
              }
              device->output_channels = channels;
              device->output_mask = mask;
              device->inter_matrix = _matrix_intersect(sformat.matrix,device->output_matrix);
            }
            break;

          default:
            OutputDebugStringA("Driver backend failed to set output ordering.\n");
            errno = AO_EFAIL;
            goto error;
          }

        }else{
          device->output_channels = sformat.channels;
        }

        /* other housekeeping */
        device->input_channels = sformat.channels;
        device->bytewidth = (sformat.bits+7)>>3;
        device->rate = sformat.rate;

	/* Open the device */
	result = funcs->open(device, &sformat);
	if (!result) {
          errno = AO_EOPENDEVICE;
          goto error;
	}

        /* set up permutation based on finalized inter_matrix mapping */
        /* The only way device->output_channels could be zero here is
           if the driver has opted to ouput no channels (eg, the null
           driver). */
        if(sformat.matrix){
          if(!device->inter_matrix){
            awarn("Driver %s does not support automatic channel mapping;\n"
                 "\tRouting only L/R channels to output.\n\n",
                 info_table[device->driver_id]->short_name);
            device->inter_matrix=strdup("L,R");
          }
          {

            /* walk thorugh the inter matrix, match channels */
            char *op=device->inter_matrix;
            int count=0;
            device->inter_permute = (int*)calloc(device->output_channels,sizeof(int));

            //adebug("\n");

            while(count<device->output_channels){
              int m=0,mm;
              char *h=op;

              if(*op){
                /* find mnemonic offset of inter channel */
                while(*h && *h!=',')h++;
                while(mnemonics[m]){
                  if(!strncmp(mnemonics[m],op,h-op))
                    break;
                  m++;
                }
                mm=m;

                /* find match in input if any */
                device->inter_permute[count] = _find_channel(m,sformat.matrix);
                if(device->inter_permute[count] == -1 && sformat.channels == 1){
                  device->inter_permute[count] = _find_channel(1,sformat.matrix);
                  mm=1;
                }
              }else
                device->inter_permute[count] = -1;

              /* display resulting mapping for now */
              if(device->inter_permute[count]>=0){
                //adebug("input %d (%s)\t -> backend %d (%s)\n",
                   //    device->inter_permute[count], mnemonics[mm],
                     //  count,mnemonics[m]);
              }else{
                //adebug("             \t    backend %d (%s)\n",
                  //     count,mnemonics[m]);
              }
              count++;
              op=h;
              if(*h)op++;
            }
            {
              char **inch = _tokenize_matrix(sformat.matrix);
              int i,j;
              int unflag=0;
              for(j=0;j<sformat.channels;j++){
                for(i=0;i<device->output_channels;i++)
                  if(device->inter_permute[i]==j)break;
                if(i==device->output_channels){
                  //adebug("input %d (%s)\t -> none\n",
                    //     j,inch[j]);
                  unflag=1;
                }
              }
              _free_map(inch);
              /*if(unflag)
                awarn("Some input channels are unmapped and will not be used.\n");*/
            }
            //averbose("\n");

          }
        }

        /* if there's no actual permutation to do, release the permutation vector */
        if(device->inter_permute && device->output_channels == device->input_channels){
          int i;
          for(i=0;i<device->output_channels;i++)
            if(device->inter_permute[i]!=i)break;
          if(i==device->output_channels){
            free(device->inter_permute);
            device->inter_permute=NULL;
          }
        }

	/* Resolve actual driver byte format */
	device->driver_byte_format =
		_real_byte_format(device->driver_byte_format);

	/* Only create swap buffer if needed */
        if (device->bytewidth>1 &&
            device->client_byte_format != device->driver_byte_format){
          //adebug("swap buffer required:\n");
          //adebug("  machine endianness: %d\n",ao_is_big_endian());
          //adebug("  device->client_byte_format:%d\n",device->client_byte_format);
          //adebug("  device->driver_byte_format:%d\n",device->driver_byte_format);
        }

	if ( (device->bytewidth>1 &&
              device->client_byte_format != device->driver_byte_format) ||
             device->inter_permute){

          result = _realloc_swap_buffer(device, DEF_SWAP_BUF_SIZE);

          if (!result) {

            if(sformat.matrix)free(sformat.matrix);
            device->funcs->close(device);
            device->funcs->device_clear(device);
            free(device);
            errno = AO_EFAIL;
            return NULL; /* Couldn't alloc swap buffer */
          }
	}

	/* If we made it this far, everything is OK. */
        if(sformat.matrix)free(sformat.matrix);
	return device;

 error:
        {
          int errtemp = errno;
          if(sformat.matrix)
            free(sformat.matrix);
          ao_close(device);
          errno=errtemp;
        }
        return NULL;
}


/* ---------- Public Functions ---------- */

/* -- Library Setup/Teardown -- */

static ao_info ao_dummy_info=
  { 0,0,0,0,0,0,0,0,0 };
static ao_info *ao_dummy_driver_info(void){
  return &ao_dummy_info;
}
static ao_functions ao_dummy_funcs=
  { 0, &ao_dummy_driver_info, 0,0,0,0,0,0,0};

void ao_initialize(void)
{
	driver_list *end;
        ao_global_dummy = &ao_global_dummy_storage;
        ao_global_dummy->funcs = &ao_dummy_funcs;

	/* Read config files */
	ao_read_config_files(&config);
        ao_global_load_options(ao_global_options);

	if (driver_head == NULL) {
		driver_head = _load_static_drivers(&end);
		_append_dynamic_drivers(end);
	}

	/* Create the table of driver info structs */
	info_table = _make_info_table(&driver_head, &driver_count);
}

void ao_shutdown(void)
{
	driver_list *driver = driver_head;
	driver_list *next_driver;

	if (!driver_head) return;

	/* unload and free all the drivers */
	while (driver) {
		if (driver->handle) {

		  //dlclose(driver->handle);
		  free(driver->functions); /* DON'T FREE STATIC FUNC TABLES */
		}
		next_driver = driver->next;
		free(driver);
		driver = next_driver;
	}

        _clear_config();
	/* NULL out driver_head or ao_initialize() won't work */
	driver_head = NULL;
}


/* -- Device Setup/Playback/Teardown -- */
int ao_append_option(ao_option **options, const char *key, const char *value)
{
	ao_option *op, *list;

	op = (ao_option *)calloc(1,sizeof(ao_option));
	if (op == NULL) return 0;

	op->key = strdup(key);
	op->value = strdup(value?value:"");
	op->next = NULL;

	if ((list = *options) != NULL) {
		list = *options;
		while (list->next != NULL) list = list->next;
		list->next = op;
	} else {
		*options = op;
	}


	return 1;
}

int ao_append_global_option(const char *key, const char *value)
{
  return ao_append_option(&ao_global_options,key,value);
}

void ao_free_options(ao_option *options)
{
	ao_option *rest;

	while (options != NULL) {
		rest = options->next;
		free(options->key);
		free(options->value);
		free(options);
		options = rest;
	}
}


ao_device *ao_open_live (int driver_id, ao_sample_format *format,
			ao_option *options)
{
	return _open_device(driver_id, format, options, NULL);
}


ao_device *ao_open_file (int driver_id, const char *filename, int overwrite,
			 ao_sample_format *format, ao_option *options)
{
	FILE *file;
	ao_device *device;

	if (strcmp("-", filename) == 0)
		file = stdout;
	else {

		if (!overwrite) {
			/* Test for file existence */
			file = fopen(filename, "r");
			if (file != NULL) {
				fclose(file);
				errno = AO_EFILEEXISTS;
				return NULL;
			}
		}


		file = fopen(filename, "wb");
	}


	if (file == NULL) {
		errno = AO_EOPENFILE;
		return NULL;
	}

	device = _open_device(driver_id, format, options, file);

	if (device == NULL) {
		fclose(file);
		/* errno already set by _open_device() */
		return NULL;
	}

	return device;
}

ao_device *ao_open_file2(int driver_id, HANDLE h,
			 ao_sample_format *format, ao_option *options)
{
	FILE *file = NULL;
	ao_device *device;

	int fd = _open_osfhandle((intptr_t)h, _O_BINARY);

	if (fd != -1)
	{
		file = _fdopen(fd, "wb");
	}

	if (file == NULL) {
		errno = AO_EOPENFILE;
		return NULL;
	}

	device = _open_device(driver_id, format, options, file);

	if (device == NULL) {
		fclose(file);
		/* errno already set by _open_device() */
		return NULL;
	}

	return device;
}


int ao_play(ao_device *device, char* output_samples, uint_32 num_bytes)
{
	char *playback_buffer;

	if (device == NULL)
	  return 0;

	if (device->swap_buffer != NULL) {
          int out_bytes = num_bytes*device->output_channels/device->input_channels;
          if (_realloc_swap_buffer(device, out_bytes)) {
            int i;
            int swap = (device->bytewidth>1 &&
                        device->client_byte_format != device->driver_byte_format);
            for(i=0;i<device->output_channels;i++){
              int ic = device->inter_permute ? device->inter_permute[i] : i;
              if(ic==-1){
                _buffer_zero(device->swap_buffer,i,device->bytewidth,device->output_channels,
                             out_bytes);
              }else if(swap){
                _buffer_permute_swap(device->swap_buffer,i,device->bytewidth,device->output_channels,
                                     out_bytes, output_samples, ic, device->input_channels);
              }else{
                _buffer_permute(device->swap_buffer,i,device->bytewidth,device->output_channels,
                                out_bytes, output_samples, ic, device->input_channels);
              }
            }
            playback_buffer = device->swap_buffer;
            num_bytes = out_bytes;
          } else
            return 0; /* Could not expand swap buffer */
	} else
          playback_buffer = output_samples;

	return device->funcs->play(device, playback_buffer, num_bytes);
}


int ao_close(ao_device *device)
{
	int result;

	if (device == NULL)
		result = 0;
	else {
		result = device->funcs->close(device);
		device->funcs->device_clear(device);

		if (device->file) {
			fclose(device->file);
			device->file = NULL;
		}

		if (device->swap_buffer != NULL)
			free(device->swap_buffer);
                if (device->output_matrix != NULL)
                        free(device->output_matrix);
                if (device->input_map != NULL)
                        free(device->input_map);
                if (device->inter_matrix != NULL)
                        free(device->inter_matrix);
                if (device->inter_permute != NULL)
                        free(device->inter_permute);
		free(device);
	}

	return result;
}


/* -- Driver Information -- */

int ao_driver_id(const char *short_name)
{
	int i;
	driver_list *driver = driver_head;

	i = 0;
	while (driver) {
		if (strcmp(short_name,
			   driver->functions->driver_info()->short_name) == 0)
			return i;
		driver = driver->next;
		i++;
	}

	return -1; /* No driver by that name */
}


int ao_default_driver_id (void)
{
	/* Find the default driver in the list of loaded drivers */

	return _find_default_driver_id(config.default_driver);
}


ao_info *ao_driver_info(int driver_id)
{
	driver_list *driver;

	if ( (driver = _get_driver(driver_id)) )
		return driver->functions->driver_info();
	else
		return NULL;
}


ao_info **ao_driver_info_list(int *count)
{
	*count = driver_count;
	return info_table;
}


#define AUDIO_FILE_MAGIC ((uint_32)0x2e736e64)  /* ".snd" */

#define AUDIO_UNKNOWN_SIZE (~0) /* (unsigned) -1 */

/* Format codes (not comprehensive) */
#define AUDIO_FILE_ENCODING_LINEAR_8 (2) /* 8-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_16 (3) /* 16-bit linear PCM */

#define AU_HEADER_LEN (28)

#define DEFAULT_SWAP_BUFFER_SIZE 2048

/* Write a uint_32 in big-endian order. */
#define WRITE_U32(buf, x) \
	*(buf) = (unsigned char)(((x)>>24)&0xff);\
	*((buf)+1) = (unsigned char)(((x)>>16)&0xff);\
	*((buf)+2) = (unsigned char)(((x)>>8)&0xff);\
	*((buf)+3) = (unsigned char)((x)&0xff);

typedef struct Audio_filehdr {
	uint_32 magic; /* magic number */
	uint_32 hdr_size; /* offset of the data */
	uint_32 data_size; /* length of data (optional) */
	uint_32 encoding; /* data format code */
	uint_32 sample_rate; /* samples per second */
	uint_32 channels; /* number of interleaved channels */
	char info[4]; /* optional text information */
} Audio_filehdr;

static char *ao_au_options[] = {"matrix","verbose","quiet","debug"};
static ao_info ao_au_info =
{
	AO_TYPE_FILE,
	"AU file output",
	"au",
	"Wil Mahan <wtm2@duke.edu>",
	"Sends output to a .au file",
	AO_FMT_BIG,
	0,
	ao_au_options,
        sizeof(ao_au_options)/sizeof(*ao_au_options)
};

typedef struct ao_au_internal
{
	Audio_filehdr au;
} ao_au_internal;


static int ao_au_test(void)
{
	return 1; /* File driver always works */
}


static ao_info *ao_au_driver_info(void)
{
	return &ao_au_info;
}


static int ao_au_device_init(ao_device *device)
{
	ao_au_internal *internal;

	internal = (ao_au_internal *) malloc(sizeof(ao_au_internal));

	if (internal == NULL)
		return 0; /* Could not initialize device memory */

	memset(&(internal->au), 0, sizeof(internal->au));

	device->internal = internal;
        device->output_matrix_order = AO_OUTPUT_MATRIX_FIXED;

	return 1; /* Memory alloc successful */
}


static int ao_au_set_option(ao_device *device, const char *key,
			    const char *value)
{
	return 1; /* No options! */
}

static int ao_au_open(ao_device *device, ao_sample_format *format)
{
	ao_au_internal *internal = (ao_au_internal *) device->internal;
	unsigned char buf[AU_HEADER_LEN];

	/* The AU format is big-endian */
	device->driver_byte_format = AO_FMT_BIG;

	/* Fill out the header */
	internal->au.magic = AUDIO_FILE_MAGIC;
	internal->au.channels = device->output_channels;
	if (format->bits == 8)
		internal->au.encoding = AUDIO_FILE_ENCODING_LINEAR_8;
	else if (format->bits == 16)
		internal->au.encoding = AUDIO_FILE_ENCODING_LINEAR_16;
	else {
		/* Only 8 and 16 bits are supported at present. */
		return 0;
	}
	internal->au.sample_rate = format->rate;
	internal->au.hdr_size = AU_HEADER_LEN;

	/* From the AU specification:  "When audio files are passed
	 * through pipes, the 'data_size' field may not be known in
	 * advance.  In such cases, the 'data_size' should be set
	 * to AUDIO_UNKNOWN_SIZE."
	 */
	internal->au.data_size = AUDIO_UNKNOWN_SIZE;
	/* strncpy(state->au.info, "OGG ", 4); */

	/* Write the header in big-endian order */
	WRITE_U32(buf, internal->au.magic);
	WRITE_U32(buf + 4, internal->au.hdr_size);
	WRITE_U32(buf + 8, internal->au.data_size);
	WRITE_U32(buf + 12, internal->au.encoding);
	WRITE_U32(buf + 16, internal->au.sample_rate);
	WRITE_U32(buf + 20, internal->au.channels);
	strncpy (((char*)buf) + 24, internal->au.info, 4);

	if (fwrite(buf, sizeof(char), AU_HEADER_LEN, device->file)
	    != AU_HEADER_LEN) {
		return 0; /* Error writing header */
	}

        if(!device->inter_matrix){
          /* set up matrix such that users are warned about > stereo playback */
          if(device->output_channels<=2)
            device->inter_matrix=strdup("L,R");
          //else no matrix, which results in a warning
        }


	return 1;
}


/*
 * play the sample to the already opened file descriptor
 */
static int ao_au_play(ao_device *device, const char *output_samples,
		       uint_32 num_bytes)
{
	if (fwrite(output_samples, sizeof(char), num_bytes,
		   device->file) < num_bytes)
		return 0;
	else
		return 1;
}

static int ao_au_close(ao_device *device)
{
	ao_au_internal *internal = (ao_au_internal *) device->internal;

        off_t size;
	unsigned char buf[4];

	/* Try to find the total file length, including header */
	size = ftell(device->file);

	/* It's not a problem if the lseek() fails; the AU
	 * format does not require a file length.  This is
	 * useful for writing to non-seekable files (e.g.
	 * pipes).
	 */
	if (size > 0) {
		internal->au.data_size = size - AU_HEADER_LEN;

		/* Rewind the file */
		if (fseek(device->file, 8 /* offset of data_size */,
					SEEK_SET) < 0)
		{
			return 1; /* Seek failed; that's okay  */
		}

		/* Fill in the file length */
		WRITE_U32 (buf, internal->au.data_size);
		if (fwrite(buf, sizeof(char), 4, device->file) < 4) {
			return 1; /* Header write failed; that's okay */
		}
	}

	return 1;
}


static void ao_au_device_clear(ao_device *device)
{
	ao_au_internal *internal = (ao_au_internal *) device->internal;

	free(internal);
        device->internal=NULL;
}

ao_functions ao_au = {
	ao_au_test,
	ao_au_driver_info,
	ao_au_device_init,
	ao_au_set_option,
	ao_au_open,
	ao_au_play,
	ao_au_close,
	ao_au_device_clear
};


static char *ao_raw_options[] = {"byteorder","matrix","verbose","quiet","debug"};
static ao_info ao_raw_info =
{
	AO_TYPE_FILE,
	"RAW sample output",
	"raw",
	"Stan Seibert <indigo@aztec.asu.edu>",
	"Writes raw audio samples to a file",
	AO_FMT_NATIVE,
	0,
	ao_raw_options,
        sizeof(ao_raw_options)/sizeof(*ao_raw_options)
};

typedef struct ao_raw_internal
{
	int byte_order;
} ao_raw_internal;


static int ao_raw_test(void)
{
	return 1; /* Always works */
}


static ao_info *ao_raw_driver_info(void)
{
	return &ao_raw_info;
}


static int ao_raw_device_init(ao_device *device)
{
	ao_raw_internal *internal;

	internal = (ao_raw_internal *) malloc(sizeof(ao_raw_internal));

	if (internal == NULL)
		return 0; /* Could not initialize device memory */

	internal->byte_order = AO_FMT_NATIVE;

	device->internal = internal;
        device->output_matrix_order = AO_OUTPUT_MATRIX_FIXED;

	return 1; /* Memory alloc successful */
}

static int ao_raw_set_option(ao_device *device, const char *key,
			      const char *value)
{
	ao_raw_internal *internal = (ao_raw_internal *)device->internal;

	if (!strcmp(key, "byteorder")) {
		if (!strcmp(value, "native"))
			internal->byte_order = AO_FMT_NATIVE;
		else if (!strcmp(value, "big"))
			internal->byte_order = AO_FMT_BIG;
		else if (!strcmp(value, "little"))
			internal->byte_order = AO_FMT_LITTLE;
		else
			return 0; /* Bad option value */
	}

	return 1;
}


static int ao_raw_open(ao_device *device, ao_sample_format *format)
{
	ao_raw_internal *internal = (ao_raw_internal *)device->internal;

	device->driver_byte_format = internal->byte_order;

        //if(!device->inter_matrix){
        ///* by default, inter == in */
        //if(format->matrix)
        //  device->inter_matrix = strdup(format->matrix);
        //}

	return 1;
}


/*
 * play the sample to the already opened file descriptor
 */
static int ao_raw_play(ao_device *device, const char *output_samples,
		       uint_32 num_bytes)
{
	if (fwrite(output_samples, sizeof(char), num_bytes,
		   device->file) < num_bytes)
		return 0;
	else
		return 1;
}


static int ao_raw_close(ao_device *device)
{
	/* No closeout needed */
	return 1;
}


static void ao_raw_device_clear(ao_device *device)
{
	ao_raw_internal *internal = (ao_raw_internal *) device->internal;

	free(internal);
        device->internal=NULL;
}


ao_functions ao_raw = {
	ao_raw_test,
	ao_raw_driver_info,
	ao_raw_device_init,
	ao_raw_set_option,
	ao_raw_open,
	ao_raw_play,
	ao_raw_close,
	ao_raw_device_clear
};


#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM         0x0001
#endif
#define FORMAT_MULAW            0x0101
#define IBM_FORMAT_ALAW         0x0102
#define IBM_FORMAT_ADPCM        0x0103
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE  0xfffe
#endif

#define WAV_HEADER_LEN 68

#define WRITE_U16(buf, x) *(buf)     = (unsigned char)(x&0xff);\
						  *((buf)+1) = (unsigned char)((x>>8)&0xff);

#define DEFAULT_SWAP_BUFFER_SIZE 2048

struct riff_struct {
	unsigned char id[4];   /* RIFF */
	unsigned int len;
	unsigned char wave_id[4]; /* WAVE */
};


struct chunk_struct
{
	unsigned char id[4];
	unsigned int len;
};

struct common_struct
{
	unsigned short wFormatTag;
	unsigned short wChannels;
	unsigned int dwSamplesPerSec;
	unsigned int dwAvgBytesPerSec;
	unsigned short wBlockAlign;
	unsigned short wBitsPerSample;
        unsigned short cbSize;
	unsigned short wValidBitsPerSample;
        unsigned int   dwChannelMask;
        unsigned short subFormat;
};

struct wave_header
{
	struct riff_struct   riff;
	struct chunk_struct  format;
	struct common_struct common;
	struct chunk_struct  data;
};


static char *ao_wav_options[] = {"matrix","verbose","quiet","debug"};
static ao_info ao_wav_info =
{
	AO_TYPE_FILE,
	"WAV file output",
	"wav",
	"Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
	"Sends output to a .wav file",
	AO_FMT_LITTLE,
	0,
	ao_wav_options,
        sizeof(ao_wav_options)/sizeof(*ao_wav_options)
};

typedef struct ao_wav_internal
{
	struct wave_header wave;
} ao_wav_internal;


static int ao_wav_test(void)
{
	return 1; /* File driver always works */
}


static ao_info *ao_wav_driver_info(void)
{
	return &ao_wav_info;
}


static int ao_wav_device_init(ao_device *device)
{
	ao_wav_internal *internal;

	internal = (ao_wav_internal *) malloc(sizeof(ao_wav_internal));

	if (internal == NULL)
		return 0; /* Could not initialize device memory */

	memset(&(internal->wave), 0, sizeof(internal->wave));

	device->internal = internal;
        device->output_matrix = strdup("L,R,C,LFE,BL,BR,CL,CR,BC,SL,SR");
        device->output_matrix_order = AO_OUTPUT_MATRIX_COLLAPSIBLE;

	return 1; /* Memory alloc successful */
}


static int ao_wav_set_option(ao_device *device, const char *key,
			     const char *value)
{
	return 1; /* No options! */
}

static int ao_wav_open(ao_device *device, ao_sample_format *format)
{
	ao_wav_internal *internal = (ao_wav_internal *) device->internal;
	unsigned char buf[WAV_HEADER_LEN];
	int size = 0x7fffffff; /* Use a bogus size initially */

	/* Store information */
	internal->wave.common.wChannels = device->output_channels;
	internal->wave.common.wBitsPerSample = ((format->bits+7)>>3)<<3;
	internal->wave.common.wValidBitsPerSample = format->bits;
	internal->wave.common.dwSamplesPerSec = format->rate;

	memset(buf, 0, WAV_HEADER_LEN);

	/* Fill out our wav-header with some information. */
	strncpy((char *)internal->wave.riff.id, "RIFF",4);
	internal->wave.riff.len = size - 8;
	strncpy((char *)internal->wave.riff.wave_id, "WAVE",4);

	strncpy((char *)internal->wave.format.id, "fmt ",4);
	internal->wave.format.len = 40;

	internal->wave.common.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	internal->wave.common.dwAvgBytesPerSec =
		internal->wave.common.wChannels *
		internal->wave.common.dwSamplesPerSec *
		(internal->wave.common.wBitsPerSample >> 3);

	internal->wave.common.wBlockAlign =
		internal->wave.common.wChannels *
		(internal->wave.common.wBitsPerSample >> 3);
	internal->wave.common.cbSize = 22;
	internal->wave.common.subFormat = WAVE_FORMAT_PCM;
        internal->wave.common.dwChannelMask=device->output_mask;

	strncpy((char *)internal->wave.data.id, "data",4);

	internal->wave.data.len = size - WAV_HEADER_LEN;

	strncpy((char *)buf, (const char *)internal->wave.riff.id, 4);
	WRITE_U32(buf+4, internal->wave.riff.len);
	strncpy((char *)buf+8, (const char *)internal->wave.riff.wave_id, 4);
	strncpy((char *)buf+12, (const char *)internal->wave.format.id,4);
	WRITE_U32(buf+16, internal->wave.format.len);
	WRITE_U16(buf+20, internal->wave.common.wFormatTag);
	WRITE_U16(buf+22, internal->wave.common.wChannels);
	WRITE_U32(buf+24, internal->wave.common.dwSamplesPerSec);
	WRITE_U32(buf+28, internal->wave.common.dwAvgBytesPerSec);
	WRITE_U16(buf+32, internal->wave.common.wBlockAlign);
	WRITE_U16(buf+34, internal->wave.common.wBitsPerSample);
	WRITE_U16(buf+36, internal->wave.common.cbSize);
	WRITE_U16(buf+38, internal->wave.common.wValidBitsPerSample);
	WRITE_U32(buf+40, internal->wave.common.dwChannelMask);
	WRITE_U16(buf+44, internal->wave.common.subFormat);
        memcpy(buf+46,"\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71",14);
	strncpy((char *)buf+60, (const char *)internal->wave.data.id, 4);
	WRITE_U32(buf+64, internal->wave.data.len);

	if (fwrite(buf, sizeof(char), WAV_HEADER_LEN, device->file)
	    != WAV_HEADER_LEN) {
		return 0; /* Could not write wav header */
	}

	device->driver_byte_format = AO_FMT_LITTLE;

	return 1;
}


/*
 * play the sample to the already opened file descriptor
 */
static int ao_wav_play(ao_device *device, const char *output_samples,
			uint_32 num_bytes)
{
	if (fwrite(output_samples, sizeof(char), num_bytes,
		   device->file) < num_bytes)
		return 0;
	else
		return 1;

}

static int ao_wav_close(ao_device *device)
{
	ao_wav_internal *internal = (ao_wav_internal *) device->internal;
	unsigned char buf[4];  /* For holding length values */

	long size;

	/* Find how long our file is in total, including header */
	size = ftell(device->file);

	if (size < 0) {
		return 0;  /* Wav header corrupt */
	}

	/* Go back and set correct length info */

	internal->wave.riff.len = size - 8;
	internal->wave.data.len = size - WAV_HEADER_LEN;

	/* Rewind to riff len and write it */
	if (fseek(device->file, 4, SEEK_SET) < 0)
		return 0; /* Wav header corrupt */

	WRITE_U32(buf, internal->wave.riff.len);
	if (fwrite(buf, sizeof(char), 4, device->file) < 4)
	  return 0; /* Wav header corrupt */


	/* Rewind to data len and write it */
	if (fseek(device->file, 64, SEEK_SET) < 0)
		return 0; /* Wav header corrupt */

	WRITE_U32(buf, internal->wave.data.len);
	if (fwrite(buf, sizeof(char), 4, device->file) < 4)
	  return 0; /* Wav header corrupt */


	return 1; /* Wav header correct */
}

static void ao_wav_device_clear(ao_device *device)
{
	ao_wav_internal *internal = (ao_wav_internal *) device->internal;

	free(internal);
        device->internal=NULL;
}


ao_functions ao_wav = {
	ao_wav_test,
	ao_wav_driver_info,
	ao_wav_device_init,
	ao_wav_set_option,
	ao_wav_open,
	ao_wav_play,
	ao_wav_close,
	ao_wav_device_clear
};


static char *ao_null_options[] = {
  "debug","verbose","matrix","quiet"
};
static ao_info ao_null_info = {
	AO_TYPE_LIVE,
	"Null output",
	"null",
	"Stan Seibert <volsung@asu.edu>",
	"This driver does nothing.",
	AO_FMT_NATIVE,
	0,
	ao_null_options,
        sizeof(ao_null_options)/sizeof(*ao_null_options)
};


typedef struct ao_null_internal {
	unsigned long byte_counter;
} ao_null_internal;


static int ao_null_test(void)
{
	return 1; /* Null always works */
}


static ao_info *ao_null_driver_info(void)
{
	return &ao_null_info;
}


static int ao_null_device_init(ao_device *device)
{
	ao_null_internal *internal;

	internal = (ao_null_internal *) malloc(sizeof(ao_null_internal));

	if (internal == NULL)
		return 0; /* Could not initialize device memory */

	internal->byte_counter = 0;

	device->internal = internal;
        device->output_matrix_order = AO_OUTPUT_MATRIX_FIXED;

	return 1; /* Memory alloc successful */
}


static int ao_null_set_option(ao_device *device, const char *key,
			      const char *value)
{
	ao_null_internal *internal = (ao_null_internal *) device->internal;

	return 1;
}



static int ao_null_open(ao_device *device, ao_sample_format *format)
{
	/* Use whatever format the client requested */
	device->driver_byte_format = device->client_byte_format;

        if(!device->inter_matrix){
          /* by default, we want inter == in */
          if(format->matrix)
            device->inter_matrix = strdup(format->matrix);
        }

	return 1;
}


static int ao_null_play(ao_device *device, const char *output_samples,
			uint_32 num_bytes)
{
	ao_null_internal *internal = (ao_null_internal *)device->internal;

	internal->byte_counter += num_bytes;

	return 1;
}


static int ao_null_close(ao_device *device)
{
	ao_null_internal *internal = (ao_null_internal *) device->internal;

	adebug("%ld bytes sent to null device.\n", internal->byte_counter);

	return 1;
}


static void ao_null_device_clear(ao_device *device)
{
	ao_null_internal *internal = (ao_null_internal *) device->internal;

	free(internal);
        device->internal=NULL;
}


ao_functions ao_null = {
	ao_null_test,
	ao_null_driver_info,
	ao_null_device_init,
	ao_null_set_option,
	ao_null_open,
	ao_null_play,
	ao_null_close,
	ao_null_device_clear
};

static int ao_read_config_file(ao_config *config, const char *config_file)
{
	FILE *fp;
	char line[4096];
	int end;

	if ( !(fp = fopen(config_file, "r")) )
		return 0; /* Can't open file */

	while (fgets(line, 4096, fp)) {
		/* All options are key=value */

		if (strncmp(line, "default_driver=", 15) == 0) {
			free(config->default_driver);
			end = strlen(line);
			if (line[end-1] == '\n')
				line[end-1] = 0; /* Remove trailing newline */

			config->default_driver = strdup(line+15);
		}else{
                        /* entries in the config file that don't parse as
                           directives to AO at large are treated as driver
                           options */
						string strkey(line);
						string strvalue;
						string::size_type val;
                        Trim(strkey);
                        if(!strkey.empty()){
                          if((val = strkey.find('=')) != string::npos){
                            strvalue = strkey.substr(val+1);
                            strkey = strkey.substr(0, val);
                          }
                          ao_append_global_option(strkey.c_str(),strvalue.c_str());
                        }
                }
	}

	fclose(fp);

	return 1;
}

void ao_read_config_files (ao_config *config)
{
	char userfile[FILENAME_MAX+1];
	char *homedir = getenv("HOME");

	/* Read the system-wide config file */
	ao_read_config_file(config, AO_SYSTEM_CONFIG);

	/* Read the user config file */
	if ( homedir!=NULL &&
	     strlen(homedir) <= FILENAME_MAX - strlen(AO_USER_CONFIG) )
	{
		strncpy(userfile, homedir, FILENAME_MAX);
		strcat(userfile, AO_USER_CONFIG);
		ao_read_config_file(config, userfile);
	}
}

int ao_is_big_endian(void)
{
	static uint_16 pattern = 0xbabe;
	return 0[(volatile unsigned char *)&pattern] == 0xba;
}

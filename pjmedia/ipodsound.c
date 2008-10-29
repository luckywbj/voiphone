/* 
 * Copyright (C) 2007-2008 Samuel Vinson <samuelv@users.sourceforge.net>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#if PJMEDIA_SOUND_IMPLEMENTATION==PJMEDIA_SOUND_IPOD_SOUND 

#include <pjmedia/sound.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/os.h>

#import <AudioToolbox/AudioToolbox.h>

/* Latency settings */
static unsigned snd_input_latency  = PJMEDIA_SND_DEFAULT_REC_LATENCY;
static unsigned snd_output_latency = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

static pj_thread_desc desc;
static pj_thread_t *  thread;
static pj_bool_t      thread_registered = PJ_FALSE;

#define THIS_FILE       "ipodsound.c"
/* Set STEREO to 1 if you want that stereo is managed in this driver */
#define STEREO              0

#define BITS_PER_SAMPLE     16
#define BYTES_PER_SAMPLE    (BITS_PER_SAMPLE/8)
#define AUDIO_BUFFERS 3

#if 0
#   define TRACE_(x)    PJ_LOG(1,x)
#else
#   define TRACE_(x)
#endif

static pjmedia_snd_dev_info ipod_snd_dev_info = 
{
    "iPod Sound Device",
    1,
    1,
    8000
};

static pj_pool_factory *snd_pool_factory;

typedef struct AQStruct 
{
    AudioQueueRef queue;
    AudioQueueBufferRef mBuffers[AUDIO_BUFFERS];
    AudioStreamBasicDescription mDataFormat;
} AQStruct;

struct pjmedia_snd_stream 
{
	pj_pool_t		*pool;
	pjmedia_dir 		dir;
	int 			rec_id;
	int 			play_id;
	unsigned		clock_rate;
	unsigned		channel_count;
	unsigned		samples_per_frame;
	unsigned		bits_per_sample;
	
    pjmedia_snd_rec_cb	rec_cb;
	pjmedia_snd_play_cb	play_cb;
    
	void			*user_data;
    
    AQStruct    *play_strm;      /**< Playback stream.       */
    AQStruct    *rec_strm;       /**< Capture stream.        */

#if STEREO    
    pj_uint16_t *play_buffer;
#endif
    
    pj_uint32_t     timestamp;
};

static void playAQBufferCallback(
    void *userData,
    AudioQueueRef outQ,
    AudioQueueBufferRef outQB)
{
    pjmedia_snd_stream *play_strm = userData;
    
    pj_status_t status;

//    inData = (AQPlayerStruct *)in;
//    if (inData->frameCount > 0) 
    {
      if(!thread_registered && !pj_thread_is_registered()) 
      {
        if (pj_thread_register(NULL,desc,&thread) == PJ_SUCCESS)
        {
          thread_registered = PJ_TRUE;
        }
      }
      
      /* Calculate bytes per frame */
      outQB->mAudioDataByteSize = play_strm->samples_per_frame * //BYTES_PER_SAMPLE;
          play_strm->bits_per_sample / 8;
      /* Get frame from application. */
#if STEREO 
      if (play_strm->channel_count == 1)
      {
          pj_uint16_t *in, *out;
          int i;
          status = (*play_strm->play_cb)(play_strm->user_data, 
                play_strm->timestamp,
                play_strm->play_buffer,
                outQB->mAudioDataByteSize);
  
         in = play_strm->play_buffer;
         out = (pj_uint16_t *)outQB->mAudioData;
         for (i = 0 ; i < play_strm->samples_per_frame; ++i)
         {
              *out++ = *in;
              *out++ = *in++;
         }     
                
         outQB->mAudioDataByteSize *= 2;
      }
      else
#endif
      {    
        status = (*play_strm->play_cb)(play_strm->user_data, 
                  play_strm->timestamp,
                  (char *)outQB->mAudioData,
                  outQB->mAudioDataByteSize);
      }

      play_strm->timestamp += play_strm->samples_per_frame;
      AudioQueueEnqueueBuffer(outQ, outQB, 0, NULL);
      
      if (status != PJ_SUCCESS)
      {
          PJ_LOG(1, (THIS_FILE, "playAQBufferCallback err %d\n", status));
      }
    }
}

static void recAQBufferCallback (
    void                                *userData,
    AudioQueueRef                       inQ,
    AudioQueueBufferRef                 inQB,
    const AudioTimeStamp                *inStartTime,
    UInt32                              inNumPackets,
    const AudioStreamPacketDescription  *inPacketDesc)
{
    pj_status_t status;
    pj_uint32_t bytes_per_frame;
    pjmedia_snd_stream *rec_strm = userData;

    if(!thread_registered && !pj_thread_is_registered()) 
    {
      if (pj_thread_register(NULL,desc,&thread) == PJ_SUCCESS)
      {
        thread_registered = PJ_TRUE;
      }
    }
    
    bytes_per_frame = rec_strm->samples_per_frame * 
        rec_strm->bits_per_sample / 8;
    

    status = rec_strm->rec_cb(rec_strm->user_data, rec_strm->timestamp, 
        (void*)inQB->mAudioData, bytes_per_frame);
     
    rec_strm->timestamp += rec_strm->samples_per_frame;
    AudioQueueEnqueueBuffer(/*pAqData->mQueue*/inQ, inQB, 0, NULL);

    if(status != PJ_SUCCESS) 
    {
        PJ_LOG(1, (THIS_FILE, "recAQBufferCallback err %d\n", status));
        return;
    }
}

PJ_DEF(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory)
{
    TRACE_((THIS_FILE, "pjmedia_snd_init."));
    
    snd_pool_factory = factory;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_snd_deinit(void)
{
    TRACE_((THIS_FILE, "pjmedia_snd_deinit."));

    snd_pool_factory = NULL;
   
    return PJ_SUCCESS;
}

PJ_DEF(int) pjmedia_snd_get_dev_count(void)
{
    TRACE_((THIS_FILE, "pjmedia_snd_get_dev_count."));
    /* Always return 1 */
    return 1;
}

PJ_DEF(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index)
{
    TRACE_((THIS_FILE, "pjmedia_snd_get_dev_info %d.", index));
    /* Always return the default sound device */
    PJ_ASSERT_RETURN(index==0 || index==(unsigned)-1, NULL);
    return &ipod_snd_dev_info;
}

PJ_DEF(pj_status_t) pjmedia_snd_open_rec( int index,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned bits_per_sample,
					  pjmedia_snd_rec_cb rec_cb,
					  void *user_data,
					  pjmedia_snd_stream **p_snd_strm)
{
    return pjmedia_snd_open(index, -2, clock_rate, channel_count,
    			    samples_per_frame, bits_per_sample,
    			    rec_cb, NULL, user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open_player( int index,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned bits_per_sample,
					pjmedia_snd_play_cb play_cb,
					void *user_data,
					pjmedia_snd_stream **p_snd_strm )
{
    return pjmedia_snd_open(-2, index, clock_rate, channel_count,
    			    samples_per_frame, bits_per_sample,
    			    NULL, play_cb, user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open( int rec_id,
				      int play_id,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned bits_per_sample,
				      pjmedia_snd_rec_cb rec_cb,
				      pjmedia_snd_play_cb play_cb,
				      void *user_data,
				      pjmedia_snd_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_snd_stream *snd_strm;
    AQStruct *aq;

    /* Make sure sound subsystem has been initialized with
     * pjmedia_snd_init() */
    PJ_ASSERT_RETURN( snd_pool_factory != NULL, PJ_EINVALIDOP );

    /* Can only support 16bits per sample */
    PJ_ASSERT_RETURN(bits_per_sample == BITS_PER_SAMPLE, PJ_EINVAL);

    pool = pj_pool_create(snd_pool_factory, NULL, 128, 128, NULL);
    snd_strm = PJ_POOL_ZALLOC_T(pool, pjmedia_snd_stream);
    
    snd_strm->pool = pool;
    
    if (rec_id == -1) rec_id = 0;
    if (play_id == -1) play_id = 0;
    
    if (rec_id != -2 && play_id != -2)
    	snd_strm->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    else if (rec_id != -2)
    	snd_strm->dir = PJMEDIA_DIR_CAPTURE;
    else if (play_id != -2)
    	snd_strm->dir = PJMEDIA_DIR_PLAYBACK;
    
    snd_strm->rec_id = rec_id;
    snd_strm->play_id = play_id;
    snd_strm->clock_rate = clock_rate;
    snd_strm->channel_count = channel_count;
    snd_strm->samples_per_frame = samples_per_frame;
    snd_strm->bits_per_sample = bits_per_sample;
    snd_strm->rec_cb = rec_cb;
    snd_strm->play_cb = play_cb;
#if STEREO 
    snd_strm->play_buffer = NULL;
#endif
    snd_strm->user_data = user_data;
    
    /* Create player stream */
    if (snd_strm->dir & PJMEDIA_DIR_PLAYBACK) 
    {
        aq = snd_strm->play_strm = PJ_POOL_ZALLOC_T(pool, AQStruct);
        // Set up our audio format -- signed interleaved shorts (-32767 -> 32767), 16 bit stereo
        // The iphone does not want to play back float32s.
        aq->mDataFormat.mSampleRate = (Float64)clock_rate; // 8000 / 44100
        aq->mDataFormat.mFormatID = kAudioFormatLinearPCM;
        aq->mDataFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | 
            kAudioFormatFlagIsPacked;
        // this means each packet in the AQ has two samples, one for each 
        // channel -> 4 bytes/frame/packet
        // In uncompressed audio, each packet contains exactly one frame.
        aq->mDataFormat.mFramesPerPacket = 1;
#if STEREO
	aq->mDataFormat.mBytesPerFrame = aq->mDataFormat.mBytesPerPacket = 
            2 * bits_per_sample / 8;
        aq->mDataFormat.mChannelsPerFrame = 2;

        if (channel_count == 1)
        {
            snd_strm->play_buffer = pj_pool_zalloc(pool,
                samples_per_frame * bits_per_sample / 8);
        }
#else
	aq->mDataFormat.mBytesPerFrame = aq->mDataFormat.mBytesPerPacket = 
            channel_count * bits_per_sample / 8;
	aq->mDataFormat.mChannelsPerFrame = channel_count;
#endif
      
        aq->mDataFormat.mBitsPerChannel = 16; // FIXME 
    }
      
    /* Create capture stream */
    if (snd_strm->dir & PJMEDIA_DIR_CAPTURE) 
    {
        aq = snd_strm->rec_strm = PJ_POOL_ZALLOC_T(pool, AQStruct);
        // TODO allocate buffers ??
        aq->mDataFormat.mSampleRate = (Float64)clock_rate;
        aq->mDataFormat.mFormatID = kAudioFormatLinearPCM;
        aq->mDataFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
            kLinearPCMFormatFlagIsPacked;
        // In uncompressed audio, each packet contains exactly one frame.
        aq->mDataFormat.mFramesPerPacket = 1;
        aq->mDataFormat.mBytesPerPacket = aq->mDataFormat.mBytesPerFrame = 
                channel_count * bits_per_sample / 8;
        aq->mDataFormat.mChannelsPerFrame = channel_count;
        aq->mDataFormat.mBitsPerChannel = 16; // FIXME      
    }
    
    *p_snd_strm = snd_strm;

    return PJ_SUCCESS;
}

/**
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_start(pjmedia_snd_stream *stream)
{
    OSStatus status;
    pj_int32_t i;
    AQStruct *aq;
  
    TRACE_((THIS_FILE, "pjmedia_snd_stream_start."));
    if (stream->dir & PJMEDIA_DIR_PLAYBACK) 
    {
        TRACE_((THIS_FILE, "pjmedia_snd_stream_start : play back starting..."));
        // FIXME : move in pjmedia_snd_open
        aq = stream->play_strm;
        status = AudioQueueNewOutput(&(aq->mDataFormat),
                      playAQBufferCallback,
                      stream,
                      NULL,
                      kCFRunLoopCommonModes,
                      0,
                      &(aq->queue));
        if (status)
        {
            PJ_LOG(1, (THIS_FILE, "AudioQueueNewOutput err %d", status));
            return PJMEDIA_ERROR; // FIXME
        }

        UInt32 bufferBytes = stream->samples_per_frame * 
                stream->bits_per_sample / 8 * aq->mDataFormat.mBytesPerFrame;
        for (i=0; i<AUDIO_BUFFERS; i++) 
        {
            status = AudioQueueAllocateBuffer(aq->queue, bufferBytes, 
                &(aq->mBuffers[i]));
            if (status) 
            {
                PJ_LOG(1, (THIS_FILE, 
                    "AudioQueueAllocateBuffer[%d] err %d\n",i, status));
                // return PJMEDIA_ERROR; // FIXME return ???             
            }
            /* "Prime" by calling the callback once per buffer */
            playAQBufferCallback (stream, aq->queue, aq->mBuffers[i]);
        }
        // FIXME: END
    
        status = AudioQueueStart(aq->queue, NULL);         
        if (status) 
        {
            PJ_LOG(1, (THIS_FILE, 
                "AudioQueueStart err %d\n", status));
        }
        TRACE_((THIS_FILE, "pjmedia_snd_stream_start : play back started"));       
    }
    if (stream->dir & PJMEDIA_DIR_CAPTURE)
    {
        TRACE_((THIS_FILE, "pjmedia_snd_stream_start : capture starting..."));
        // FIXME
        aq = stream->rec_strm;
        
        status = AudioQueueNewInput (&(aq->mDataFormat),
            recAQBufferCallback,
            stream,
            NULL,
            kCFRunLoopCommonModes,
            0,
            &(aq->queue));
        if (status)
        {
            PJ_LOG(1, (THIS_FILE, "AudioQueueNewInput err %d", status));
            return PJMEDIA_ERROR; // FIXME
        }
        
        UInt32 bufferBytes;
        bufferBytes = stream->samples_per_frame * 
            stream->bits_per_sample / 8;
        for (i = 0; i < AUDIO_BUFFERS; ++i) 
        {
            status = AudioQueueAllocateBuffer (aq->queue, bufferBytes,
                &(aq->mBuffers[i]));
          
            if (status)          
            {
                PJ_LOG(1, (THIS_FILE, 
                    "AudioQueueAllocateBuffer[%d] err %d\n",i, status));
                // return PJMEDIA_ERROR; // FIXME return ???             
            }
            AudioQueueEnqueueBuffer (aq->queue, aq->mBuffers[i], 0, NULL);
        }
        
        // FIXME : END
        
        status = AudioQueueStart (aq->queue, NULL);
        if (status)
        {
            PJ_LOG(1, (THIS_FILE, "Starting capture stream error %d", status));
            return PJMEDIA_ENOSNDREC;   
        }
        
        UInt32 level = 1;
        status = AudioQueueSetProperty(aq->queue, 
            kAudioQueueProperty_EnableLevelMetering, &level, sizeof(level));
        if (status)
        {
            PJ_LOG(1, (THIS_FILE, "AudioQueueSetProperty err %d", status));
        }	

        TRACE_((THIS_FILE, "pjmedia_snd_stream_start : capture started..."));
    }
    
    return PJ_SUCCESS;
}

/**
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_stop(pjmedia_snd_stream *stream)
{
    OSStatus status;
    AQStruct *aq;
//    int i;
    
    TRACE_((THIS_FILE, "pjmedia_snd_stream_stop."));
    if (stream->dir & PJMEDIA_DIR_PLAYBACK) 
    {
         PJ_LOG(5,(THIS_FILE, "Stopping playback stream"));
         aq = stream->play_strm;
         status = AudioQueuePause (aq->queue);
         if (status)
            PJ_LOG(1, (THIS_FILE, "Stopping playback stream error %d", status));
    }
    if (stream->dir & PJMEDIA_DIR_CAPTURE)
    {
        PJ_LOG(5,(THIS_FILE, "Stopping capture stream"));
        aq = stream->rec_strm;
        status = AudioQueuePause (aq->queue);
        if (status)
          PJ_LOG(1, (THIS_FILE, "Stopping capture stream error %d", status));
    }
    return PJ_SUCCESS;
}

/**
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_get_info(pjmedia_snd_stream *strm,
						pjmedia_snd_stream_info *pi)
{
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    TRACE_((THIS_FILE, "pjmedia_snd_stream_get_info."));

    pj_bzero(pi, sizeof(pjmedia_snd_stream_info));
    pi->dir = strm->dir;
    pi->play_id = strm->play_id;
    pi->rec_id = strm->rec_id;
    pi->clock_rate = strm->clock_rate;
    pi->channel_count = strm->channel_count;
    pi->samples_per_frame = strm->samples_per_frame;
    pi->bits_per_sample = strm->bits_per_sample;
    pi->rec_latency = 0;
    pi->play_latency = 0;
    
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream)
{
    OSStatus status;
    AQStruct *aq;
    int i;
    
    TRACE_((THIS_FILE, "pjmedia_snd_stream_close."));
    if (stream->dir & PJMEDIA_DIR_PLAYBACK) 
    {
         PJ_LOG(5,(THIS_FILE, "Disposing playback stream"));
         aq = stream->play_strm;
         status = AudioQueueStop (aq->queue, true);
         for (i=0; i<AUDIO_BUFFERS; i++) 
         {
            AudioQueueFreeBuffer(aq->queue,aq->mBuffers[i]);
         } 
         status = AudioQueueDispose (aq->queue, true);
         if (status)
            PJ_LOG(1, (THIS_FILE, "Disposing playback stream error %d", status));
    }
    if (stream->dir & PJMEDIA_DIR_CAPTURE)
    {
        PJ_LOG(5,(THIS_FILE, "Disposing capture stream"));
        aq = stream->rec_strm;
        status = AudioQueueStop (aq->queue, true);
        for (i=0; i<AUDIO_BUFFERS; i++) 
        {
            AudioQueueFreeBuffer(aq->queue,aq->mBuffers[i]);
        }
       
        status = AudioQueueDispose (aq->queue, true);
        if (status)
            PJ_LOG(1, (THIS_FILE, "Disposing capture stream error %d", status));
    }
    thread_registered = PJ_FALSE;
    return PJ_SUCCESS;
}

/*
 * Set sound latency.
 */
PJ_DEF(pj_status_t) pjmedia_snd_set_latency(unsigned input_latency, 
					    unsigned output_latency)
{
    snd_input_latency  = (input_latency == 0)? 
			 PJMEDIA_SND_DEFAULT_REC_LATENCY : input_latency;
    snd_output_latency = (output_latency == 0)? 
			 PJMEDIA_SND_DEFAULT_PLAY_LATENCY : output_latency;

    return PJ_SUCCESS;
}



#endif	/* PJMEDIA_SOUND_IMPLEMENTATION */

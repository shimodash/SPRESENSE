/****************************************************************************
 * examples/ble_toio_central/ble_toio_central_main.cxx
 *
 * Copyright 2024 Sony Semiconductor Solutions Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * 3. Neither the name of Sony Semiconductor Solutions Corporation nor
 * the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE ASYNC DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <bluetooth/bt_common.h>
#include <bluetooth/ble_gatt.h>
#include <bluetooth/hal/bt_if.h>
#include <bluetooth/ble_util.h>
#include "ble_central_app.h"
#include "toio.h"

// Audio player includes
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <asmp/mpshm.h>
#include <arch/chip/pm.h>
#include <arch/board/board.h>
#include <sys/stat.h>
#include "audio/audio_high_level_api.h"
#include "memutils/simple_fifo/CMN_SimpleFifo.h"
#include "memutils/memory_manager/MemHandle.h"
#include "memutils/message/Message.h"
#include "include/msgq_id.h"
#include "include/mem_layout.h"
#include "include/memory_layout.h"
#include "include/msgq_pool.h"
#include "include/pool_layout.h"
#include "include/fixed_fence.h"
#ifdef CONFIG_AUDIOUTILS_PLAYLIST
#include <audio/utilities/playlist.h>
#endif
#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC
#include "userproc_command.h"
#endif /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */

/* Section number of memory layout to use */
#define AUDIO_SECTION   SECTION_NO0

using namespace MemMgrLite;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* toio: Service UUID */
const char *SERVICE_UUID = "10B20100-5B3B-4571-9508-CF3EFCD7BBAE";

/* toio: Characteristic UUID */
const char *ID_UUID      = "10B20101-5B3B-4571-9508-CF3EFCD7BBAE";
const char *MOTOR_UUID   = "10B20102-5B3B-4571-9508-CF3EFCD7BBAE";
const char *LIGHT_UUID   = "10B20103-5B3B-4571-9508-CF3EFCD7BBAE";
const char *SOUND_UUID   = "10B20104-5B3B-4571-9508-CF3EFCD7BBAE";
const char *SENSOR_UUID  = "10B20106-5B3B-4571-9508-CF3EFCD7BBAE";
const char *BUTTON_UUID  = "10B20107-5B3B-4571-9508-CF3EFCD7BBAE";
const char *BATTERY_UUID = "10B20108-5B3B-4571-9508-CF3EFCD7BBAE";
const char *SETTING_UUID = "10B201FF-5B3B-4571-9508-CF3EFCD7BBAE";

/* Characteristic UUIDs */
static BLE_UUID g_id_uuid;
static BLE_UUID g_motor_uuid;
static BLE_UUID g_light_uuid;
static BLE_UUID g_sound_uuid;
static BLE_UUID g_sensor_uuid;
static BLE_UUID g_button_uuid;
static BLE_UUID g_battery_uuid;
static BLE_UUID g_setting_uuid;

/* Characteristic handles */
static uint16_t g_id_handle;
static uint16_t g_motor_handle;
static uint16_t g_light_handle;
static uint16_t g_sound_handle;
static uint16_t g_sensor_handle;
static uint16_t g_button_handle;
static uint16_t g_battery_handle;
static uint16_t g_setting_handle;

static volatile bool g_thread_finished; // For toio thread termination

/*------------------------------
 * User definable definitions for Audio Player
 *------------------------------
 */

/* Path of playback file. */
#define PLAYBACK_FILE_PATH "/mnt/sd0/AUDIO"
/* Path of DSP image file. */
#define DSPBIN_FILE_PATH   "/mnt/sd0/BIN"

#ifdef CONFIG_AUDIOUTILS_PLAYLIST
/* Path of playlist file. */
#  define PLAYLIST_FILE_PATH "/mnt/sd0/PLAYLIST"
/* PlayList file name. */
#  define PLAYLIST_FILE_NAME "TRACK_DB.CSV"
#endif

/* Default Volume. -20dB */
// #define PLAYER_DEF_VOLUME -200
#define PLAYER_DEF_VOLUME -200

/* Definition of FIFO info. */
#define FIFO_FRAME_SIZE  3840
#define FIFO_ELEMENT_NUM  10
#define PLAYER_FIFO_PUSH_NUM_MAX  5

/* Definition of content information to be used when not using playlist. */
#define PLAYBACK_FILE_NAME     "Sound.mp3" // 使用するオーディオファイル名
#define PLAYBACK_CH_NUM        AS_CHANNEL_STEREO // オーディオファイルのチャンネル数 (例: ステレオ)
#define PLAYBACK_BIT_LEN       AS_BITLENGTH_16 // オーディオファイルのビット深度 (例: 16ビット)
#define PLAYBACK_SAMPLING_RATE AS_SAMPLINGRATE_48000 // オーディオファイルのサンプリングレート (例: 48kHz)
#define PLAYBACK_CODEC_TYPE    AS_CODECTYPE_MP3 // オーディオファイルのコーデックタイプ (例: MP3)

/*------------------------------
 * Definition specified by config for Audio Player
 *------------------------------
 */

/* Definition for selection of output device. */
/* ### MODIFIED: Changed output device to SPHP as requested ### */
#define PLAYER_OUTPUT_DEV AS_SETPLAYER_OUTPUTDEVICE_SPHP
#define PLAYER_MIXER_OUT  AS_OUT_SP

/* Definition depending on player mode. */
#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_MODE_HIRES
#  define FIFO_FRAME_NUM  4
#else
#  define FIFO_FRAME_NUM  1
#endif

/*------------------------------
 * Definition of example fixed for Audio Player
 *------------------------------
 */

/* FIFO control value. */
#define FIFO_ELEMENT_SIZE  (FIFO_FRAME_SIZE * FIFO_FRAME_NUM)
#define FIFO_QUEUE_SIZE    (FIFO_ELEMENT_SIZE * FIFO_ELEMENT_NUM)

/* Local error code. */
#define FIFO_RESULT_OK  0
#define FIFO_RESULT_ERR 1
#define FIFO_RESULT_EOF 2
#define FIFO_RESULT_FUL 3

/****************************************************************************
 * Private Types (Audio Player related)
 ****************************************************************************/

/* For FIFO */
struct player_fifo_info_s
{
  CMN_SimpleFifoHandle          handle;
  AsPlayerInputDeviceHdlrForRAM input_device;
  uint32_t fifo_area[FIFO_QUEUE_SIZE/sizeof(uint32_t)];
  uint8_t  read_buf[FIFO_ELEMENT_SIZE];
};

#ifndef CONFIG_AUDIOUTILS_PLAYLIST
struct Track
{
  char title[64];
  uint8_t   channel_number;  /* Channel number. */
  uint8_t   bit_length;      /* Bit length.     */
  uint32_t  sampling_rate;   /* Sampling rate.  */
  uint8_t   codec_type;      /* Codec type.     */
};
#endif

/* For play file */
struct player_file_info_s
{
  Track   track;
  int32_t size;
  DIR    *dirp;
  int     fd;
};

/* Player info */
struct player_info_s
{
  struct player_fifo_info_s   fifo;
  struct player_file_info_s   file;
#ifdef CONFIG_AUDIOUTILS_PLAYLIST
  Playlist *playlist_ins = NULL;
#endif
};

/****************************************************************************
 * Private Data (Audio Player related)
 ****************************************************************************/

/* For control player */
static struct player_info_s  s_player_info;

/* For share memory. */
static mpshm_t s_shm;

/* For frequency lock. */
static struct pm_cpu_freqlock_s s_player_lock;

// Mutex and condition variable for audio playback control
static pthread_mutex_t g_audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_audio_cond = PTHREAD_COND_INITIALIZER;
static volatile bool g_play_audio_signal = false;
static volatile bool g_stop_audio_signal = false; // Add stop signal for audio thread
static volatile bool g_play_next_track_signal = false; // Signal for next track
static volatile bool g_is_audio_playing = false; // Flag to indicate if audio is currently playing
static volatile bool g_audio_thread_finished = false; // Flag for audio thread termination

// Added for dynamic track selection
static char g_requested_track_title[64] = {0}; // Stores the title of the track to be played next

/****************************************************************************
 * Private Functions (toio related)
 ****************************************************************************/

/* Get characteristic handle of the discovered UUID */
static void char_discovered(struct ble_gattc_db_disc_char_s *gatt_disc_char)
{
  printf("%s()\n", __func__);

  struct ble_gattc_char_s *ch = &gatt_disc_char->characteristic;

  if (bleutil_uuidcmp(&g_id_uuid, &ch->char_valuuid) == 0)
    {
      g_id_handle = ch->char_valhandle;
    }

  if (bleutil_uuidcmp(&g_motor_uuid, &ch->char_valuuid) == 0)
    {
      g_motor_handle = ch->char_valhandle;
    }

  if (bleutil_uuidcmp(&g_light_uuid, &ch->char_valuuid) == 0)
    {
      g_light_handle = ch->char_valhandle;
    }

  if (bleutil_uuidcmp(&g_sound_uuid, &ch->char_valuuid) == 0)
    {
      g_sound_handle = ch->char_valhandle;
    }

  if (bleutil_uuidcmp(&g_sensor_uuid, &ch->char_valuuid) == 0)
    {
      g_sensor_handle = ch->char_valhandle;
    }

  if (bleutil_uuidcmp(&g_button_uuid, &ch->char_valuuid) == 0)
    {
      g_button_handle = ch->char_valhandle;
    }

  if (bleutil_uuidcmp(&g_battery_uuid, &ch->char_valuuid) == 0)
    {
      g_battery_handle = ch->char_valhandle;
    }

  if (bleutil_uuidcmp(&g_setting_uuid, &ch->char_valuuid) == 0)
    {
      g_setting_handle = ch->char_valhandle;
    }
}

/****************************************************************************
 * Private Functions (Audio Player related)
 ****************************************************************************/

static void app_init_freq_lock(void)
{
  s_player_lock.count = 0;
  s_player_lock.info = PM_CPUFREQLOCK_TAG('A', 'P', 0);
  s_player_lock.flag = PM_CPUFREQLOCK_FLAG_HV;
}

static void app_freq_lock(void)
{
  up_pm_acquire_freqlock(&s_player_lock);
}

static void app_freq_release(void)
{
  up_pm_release_freqlock(&s_player_lock);
}

static bool app_open_contents_dir(void)
{
  DIR *dirp;
  const char *name = PLAYBACK_FILE_PATH;

  printf("Attempting to open audio content directory: %s\n", name); // Added log
  dirp = opendir(name);

  if (!dirp)
    {
      printf("Error: %s directory path error. check the path! errno: %d\n", name, get_errno()); // Added errno
      return false;
    }
  printf("Successfully opened audio content directory: %s\n", name); // Added log

  s_player_info.file.dirp = dirp;

  return true;
}

static bool app_close_contents_dir(void)
{
  closedir(s_player_info.file.dirp);
  printf("Closed audio content directory.\n"); // Added log
  return true;
}

#ifdef CONFIG_AUDIOUTILS_PLAYLIST
static bool app_open_playlist(void)
{
  bool result = false;

  if (s_player_info.playlist_ins != NULL)
    {
      printf("Error: Open playlist failure. Playlist is already open\n");
      return false;
    }

  s_player_info.playlist_ins = new Playlist(PLAYLIST_FILE_NAME);

  result = s_player_info.playlist_ins->init(PLAYLIST_FILE_PATH);
  if (!result)
    {
      printf("Error: Playlist::init() failure.\n");
      return false;
    }

  s_player_info.playlist_ins->setPlayMode(Playlist::PlayModeNormal);
  if (!result)
    {
      printf("Error: Playlist::setPlayMode() failure.\n");
      return false;
    }

  s_player_info.playlist_ins->setRepeatMode(Playlist::RepeatModeOn);
  if (!result)
    {
      printf("Error: Playlist::setRepeatMode() failure.\n");
      return false;
    }

  s_player_info.playlist_ins->select(Playlist::ListTypeAllTrack, NULL);
  if (!result)
    {
      printf("Error: Playlist::select() failure.\n");
      return false;
    }

  return true;
}

static bool app_close_playlist(void)
{
  if (s_player_info.playlist_ins == NULL)
    {
      printf("Error: Close playlist failure. Playlist is not open\n");
      return false;
    }

  delete s_player_info.playlist_ins;
  s_player_info.playlist_ins = NULL;

  return true;
}
#endif /* #ifdef CONFIG_AUDIOUTILS_PLAYLIST */

static bool app_get_next_track(Track* track)
{
  bool ret = true;

#ifdef CONFIG_AUDIOUTILS_PLAYLIST
  if (s_player_info.playlist_ins == NULL)
    {
      printf("Error: Get next track failure. Playlist is not open\n");
      return false;
    }
  ret = s_player_info.playlist_ins->getNextTrack(track);
#else
  // When not using playlist, we only have one predefined track.
  // To simulate "next track" functionality, we might need a simple counter
  // and a list of predefined files, or just loop the same one.
  // For now, we'll just get the same track info.
  snprintf(track->title, sizeof(track->title), "%s", PLAYBACK_FILE_NAME);
  track->channel_number = PLAYBACK_CH_NUM;
  track->bit_length     = PLAYBACK_BIT_LEN;
  track->sampling_rate  = PLAYBACK_SAMPLING_RATE;
  track->codec_type     = PLAYBACK_CODEC_TYPE;
#endif /* #ifdef CONFIG_AUDIOUTILS_PLAYLIST */
  printf("Next track info: Title='%s', SR=%lu, CH=%d, BitLen=%d, Codec=%d\n",
         track->title, (unsigned long)track->sampling_rate, track->channel_number,
         track->bit_length, track->codec_type); // Added log

  return ret;
}

static void app_input_device_callback(uint32_t size)
{
    /* do nothing */
}

static bool app_init_simple_fifo(void)
{
  if (CMN_SimpleFifoInitialize(&s_player_info.fifo.handle,
                               s_player_info.fifo.fifo_area,
                               FIFO_QUEUE_SIZE,
                               NULL) != 0)
    {
      printf("Error: Fail to initialize simple FIFO.");
      return false;
    }
  CMN_SimpleFifoClear(&s_player_info.fifo.handle);
  printf("Simple FIFO initialized. Size: %lu bytes\n", (unsigned long)FIFO_QUEUE_SIZE); // Added log

  s_player_info.fifo.input_device.simple_fifo_handler = (void*)(&s_player_info.fifo.handle);
  s_player_info.fifo.input_device.callback_function = app_input_device_callback;

  return true;
}

static int app_push_simple_fifo(int fd)
{
  int ret;

  ret = read(fd, &s_player_info.fifo.read_buf, FIFO_ELEMENT_SIZE);
  if (ret < 0)
    {
      printf("Error: Fail to read file. errno:%d\n", get_errno());
      return FIFO_RESULT_ERR;
    }
  // printf("FIFO: Read %d bytes from file. Remaining file size before push: %ld\n", ret, (long)s_player_info.file.size); // Verbose log

  if (ret == 0) /* EOF */
    {
        printf("FIFO: End of file reached (0 bytes read).\n"); // Added log
        return FIFO_RESULT_EOF;
    }

  int offer_result = CMN_SimpleFifoOffer(&s_player_info.fifo.handle, (const void*)(s_player_info.fifo.read_buf), ret);
  if (offer_result == 0)
    {
      printf("FIFO: Warning: Simple FIFO full or failed to offer data. Offer result: %d, Data size: %d, Vacant: %lu\n", offer_result, ret, CMN_SimpleFifoGetVacantSize(&s_player_info.fifo.handle)); // Added log
      return FIFO_RESULT_FUL;
    }
  // printf("FIFO: Offered %d bytes to FIFO. FIFO vacant size: %lu\n", ret, CMN_SimpleFifoGetVacantSize(&s_player_info.fifo.handle)); // Verbose log
  s_player_info.file.size = (s_player_info.file.size - ret);

  return FIFO_RESULT_OK;
}

static bool app_first_push_simple_fifo(int fd)
{
  int i;
  int ret = 0;

  printf("FIFO: Initial push to simple FIFO...\n"); // Added log
  for(i = 0; i < FIFO_ELEMENT_NUM - 1; i++)
    {
      if ((ret = app_push_simple_fifo(fd)) != FIFO_RESULT_OK)
        {
          break;
        }
    }
  printf("FIFO: Initial push completed with result: %d\n", ret); // Added log

  return (ret != FIFO_RESULT_ERR) ? true : false;
}

static bool app_refill_simple_fifo(int fd)
{
  int32_t ret = FIFO_RESULT_OK;
  size_t  vacant_size;

  vacant_size = CMN_SimpleFifoGetVacantSize(&s_player_info.fifo.handle);

  if ((vacant_size != 0) && (vacant_size > FIFO_ELEMENT_SIZE))
    {
      int push_cnt = vacant_size / FIFO_ELEMENT_SIZE;

      push_cnt = (push_cnt >= PLAYER_FIFO_PUSH_NUM_MAX) ?
                  PLAYER_FIFO_PUSH_NUM_MAX : push_cnt;

      for (int i = 0; i < push_cnt; i++)
        {
          if ((ret = app_push_simple_fifo(fd)) != FIFO_RESULT_OK)
            {
              break;
            }
        }
    }

  return (ret == FIFO_RESULT_OK || ret == FIFO_RESULT_EOF) ? true : false;
}

static bool printAudCmdResult(uint8_t command_code, AudioResult& result)
{
  if (AUDRLT_ERRORRESPONSE == result.header.result_code) {
    printf("AudioCmdResult Error: Cmd=0x%x, Module=0x%x, ErrCode=0x%lx\n",
            command_code,
            result.error_response_param.module_id,
            result.error_response_param.error_code);
    return false;
  }
  else if (AUDRLT_ERRORATTENTION == result.header.result_code) {
    printf("AudioCmdResult Attention: Cmd=0x%x\n", command_code);
    return false;
  }
  return true;
}

static void app_attention_callback(const ErrorAttentionParam *attparam)
{
  printf("Attention!! %s L%d ecode %d subcode %ld\n",
          attparam->error_filename,
          attparam->line_number,
          attparam->error_code,
          attparam->error_att_sub_code);
}

static bool app_create_audio_sub_system(void)
{
  bool result = false;

  printf("Creating AudioSubSystem manager...\n"); // Added log
  /* Create manager of AudioSubSystem. */
  AudioSubSystemIDs ids;
  ids.app         = MSGQ_AUD_APP;
  ids.mng         = MSGQ_AUD_MNG;
  ids.player_main = MSGQ_AUD_PLY;
  ids.player_sub  = 0xFF;
  ids.mixer       = MSGQ_AUD_OUTPUT_MIX;
  ids.recorder    = 0xFF;
  ids.effector    = 0xFF;
  ids.recognizer  = 0xFF;

  AS_CreateAudioManager(ids, app_attention_callback);
  printf("Audio Manager created.\n"); // Added log

  /* Create player feature. */
  AsCreatePlayerParams_t player_create_param;
  player_create_param.msgq_id.player   = MSGQ_AUD_PLY;
  player_create_param.msgq_id.mng      = MSGQ_AUD_MNG;
  player_create_param.msgq_id.mixer    = MSGQ_AUD_OUTPUT_MIX;
  player_create_param.msgq_id.dsp      = MSGQ_AUD_DSP;
  player_create_param.pool_id.es       = S0_DEC_ES_MAIN_BUF_POOL;
  player_create_param.pool_id.pcm      = S0_REND_PCM_BUF_POOL;
  player_create_param.pool_id.dsp      = S0_DEC_APU_CMD_POOL;
  player_create_param.pool_id.src_work = S0_SRC_WORK_BUF_POOL;

  result = AS_CreatePlayerMulti(AS_PLAYER_ID_0, &player_create_param, NULL);

  if (!result)
    {
      printf("Error: AS_CratePlayer() failure. system memory insufficient!\n");
      return false;
    }
  printf("Audio Player created.\n"); // Added log

  /* Create mixer feature. */
  AsCreateOutputMixParams_t output_mix_act_param;
  output_mix_act_param.msgq_id.mixer = MSGQ_AUD_OUTPUT_MIX;
  output_mix_act_param.msgq_id.render_path0_filter_dsp = MSGQ_AUD_PFDSP0;
  output_mix_act_param.msgq_id.render_path1_filter_dsp = 0xFF;
  output_mix_act_param.pool_id.render_path0_filter_pcm = S0_PF0_PCM_BUF_POOL;
  output_mix_act_param.pool_id.render_path1_filter_pcm = S0_NULL_POOL;
  output_mix_act_param.pool_id.render_path0_filter_dsp = S0_PF0_APU_CMD_POOL;
  output_mix_act_param.pool_id.render_path1_filter_dsp = S0_NULL_POOL;

  result = AS_CreateOutputMixer(&output_mix_act_param, NULL);
  if (!result)
    {
      printf("Error: AS_CreateOutputMixer() failed. system memory insufficient!\n");
      return false;
    }
  printf("Output Mixer created.\n"); // Added log

  /* Create renderer feature. */
  AsCreateRendererParam_t renderer_create_param;
  renderer_create_param.msgq_id.dev0_req  = MSGQ_AUD_RND_PLY;
  renderer_create_param.msgq_id.dev0_sync = MSGQ_AUD_RND_PLY_SYNC;
  renderer_create_param.msgq_id.dev1_req  = 0xFF;
  renderer_create_param.msgq_id.dev1_sync = 0xFF;

  result = AS_CreateRenderer(&renderer_create_param);
  if (!result)
    {
      printf("Error: AS_CreateRenderer() failure. system memory insufficient!\n");
      return false;
    }
  printf("Renderer created.\n"); // Added log

  return true;
}

static void app_deact_audio_sub_system(void)
{
  AS_DeleteAudioManager();
  AS_DeletePlayer(AS_PLAYER_ID_0);
  AS_DeleteOutputMix();
  AS_DeleteRenderer();
  printf("Audio SubSystem deactivated.\n"); // Added log
}

static bool app_power_on(void)
{
  AudioCommand command;
  command.header.packet_length = LENGTH_POWERON;
  command.header.command_code  = AUDCMD_POWERON;
  command.header.sub_code      = 0x00;
  command.power_on_param.enable_sound_effect = AS_DISABLE_SOUNDEFFECT;
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Audio power ON successful.\n"); // Added log
  } else {
      printf("Audio power ON failed.\n"); // Added log
  }
  return success;
}

static bool app_power_off(void)
{
  AudioCommand command;
  command.header.packet_length = LENGTH_SET_POWEROFF_STATUS;
  command.header.command_code  = AUDCMD_SETPOWEROFFSTATUS;
  command.header.sub_code      = 0x00;
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Audio power OFF successful.\n"); // Added log
  } else {
      printf("Audio power OFF failed.\n"); // Added log
  }
  return success;
}

static bool app_set_ready(void)
{
  AudioCommand command;
  command.header.packet_length = LENGTH_SET_READY_STATUS;
  command.header.command_code  = AUDCMD_SETREADYSTATUS;
  command.header.sub_code      = 0x00;
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Audio set READY status successful.\n"); // Added log
  } else {
      printf("Audio set READY status failed.\n"); // Added log
  }
  return success;
}

static AsMngStatus app_get_status(void)
{
  AudioCommand command;
  command.header.packet_length = LENGTH_GETSTATUS;
  command.header.command_code  = AUDCMD_GETSTATUS;
  command.header.sub_code      = 0x00;
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  printAudCmdResult(command.header.command_code, result); // Log result, but return status anyway
  AsMngStatus status = static_cast<AsMngStatus>(result.notify_status.status_info);
  printf("Audio Manager Status: %d\n", status); // Added log
  return status;
}

static bool app_init_output_select(void)
{
  AudioCommand command;
  command.header.packet_length = LENGTH_INITOUTPUTSELECT;
  command.header.command_code  = AUDCMD_INITOUTPUTSELECT;
  command.header.sub_code      = 0;
  command.init_output_select_param.output_device_sel = PLAYER_MIXER_OUT;
  printf("Setting output device selection: %d (0=I2S, 1=SPHP)\n", PLAYER_MIXER_OUT); // Added log
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Output select successful.\n"); // Added log
  } else {
      printf("Output select failed.\n"); // Added log
  }
  return success;
}

static bool app_set_volume(int master_db)
{
    AudioCommand command;
    command.header.packet_length = LENGTH_SETVOLUME;
    command.header.command_code  = AUDCMD_SETVOLUME;
    command.header.sub_code      = 0;
    command.set_volume_param.input1_db = 0;
    command.set_volume_param.input2_db = 0;
    command.set_volume_param.master_db = master_db;
    printf("Setting master volume to %d dB (PLAYER_DEF_VOLUME=%d).\n", master_db, PLAYER_DEF_VOLUME); // Added log
    AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Volume set successful.\n"); // Added log
  } else {
      printf("Volume set failed.\n"); // Added log
  }
  return success;
}

static bool app_set_player_status(void)
{
    AudioCommand command;
    command.header.packet_length = LENGTH_SET_PLAYER_STATUS;
#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC
    command.header.command_code = AUDCMD_SETPLAYERSTATUSPOST;
#else /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */
    command.header.command_code = AUDCMD_SETPLAYERSTATUS;
#endif /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */
    command.header.sub_code = 0x00;
    command.set_player_sts_param.active_player         = AS_ACTPLAYER_MAIN;
    command.set_player_sts_param.player0.input_device  = AS_SETPLAYER_INPUTDEVICE_RAM;
    command.set_player_sts_param.player0.ram_handler   = &s_player_info.fifo.input_device;
    command.set_player_sts_param.player0.output_device = PLAYER_OUTPUT_DEV;
    printf("Setting player status: ActivePlayer=%d, InputDev=%d, OutputDev=%d\n",
           command.set_player_sts_param.active_player,
           command.set_player_sts_param.player0.input_device,
           command.set_player_sts_param.player0.output_device); // Added log

#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC
    command.set_player_sts_param.post0_enable          = PostFilterEnable;
#endif /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */
    command.set_player_sts_param.player1.input_device  = 0x00;
    command.set_player_sts_param.player1.ram_handler   = NULL;
    command.set_player_sts_param.player1.output_device = 0x00;
#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC
    command.set_player_sts_param.post1_enable          = PostFilterEnable;
#endif /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */
    AS_SendAudioCommand(&command);

    AudioResult result;
    AS_ReceiveAudioResult(&result);
    bool success = printAudCmdResult(command.header.command_code, result);
    if (success) {
        printf("Player status set successful.\n"); // Added log
    } else {
        printf("Player status set failed.\n"); // Added log
    }
    return success;
}

static int app_init_player(uint8_t codec_type,
                           uint32_t sampling_rate,
                           uint8_t channel_number,
                           uint8_t bit_length)
{
    AudioCommand command;
    command.header.packet_length = LENGTH_INIT_PLAYER;
    command.header.command_code  = AUDCMD_INITPLAYER;
    command.header.sub_code      = 0x00;
    command.player.player_id                 = AS_PLAYER_ID_0;
    command.player.init_param.codec_type     = codec_type;
    command.player.init_param.bit_length     = bit_length;
    command.player.init_param.channel_number = channel_number;
    command.player.init_param.sampling_rate  = sampling_rate;
    snprintf(command.player.init_param.dsp_path,
             AS_AUDIO_DSP_PATH_LEN,
             "%s",
             DSPBIN_FILE_PATH);
    printf("Initializing player for Codec=%d, SR=%lu, CH=%d, BitLen=%d, DSP Path='%s'\n",
           codec_type, (unsigned long)sampling_rate, channel_number, bit_length, DSPBIN_FILE_PATH); // Added log
    AS_SendAudioCommand(&command);

    AudioResult result;
    AS_ReceiveAudioResult(&result);
    bool success = printAudCmdResult(command.header.command_code, result);
    if (success) {
        printf("Player initialization successful.\n"); // Added log
    } else {
        printf("Player initialization failed.\n"); // Added log
    }
    return success;
}

static int app_play_player(void)
{
    AudioCommand command;
    command.header.packet_length = LENGTH_PLAY_PLAYER;
    command.header.command_code  = AUDCMD_PLAYPLAYER;
    command.header.sub_code      = 0x00;
    command.player.player_id     = AS_PLAYER_ID_0;
    AS_SendAudioCommand(&command);

    AudioResult result;
    AS_ReceiveAudioResult(&result);
    bool success = printAudCmdResult(command.header.command_code, result);
    if (success) {
        printf("Player play command successful.\n"); // Added log
    } else {
        printf("Player play command failed.\n"); // Added log
    }
    return success;
}

static bool app_stop_player(int mode)
{
    AudioCommand command;
    command.header.packet_length = LENGTH_STOP_PLAYER;
    command.header.command_code  = AUDCMD_STOPPLAYER;
    command.header.sub_code      = 0x00;
    command.player.player_id            = AS_PLAYER_ID_0;
    command.player.stop_param.stop_mode = mode;
    printf("Stopping player with mode: %d\n", mode); // Added log
    AS_SendAudioCommand(&command);

    AudioResult result;
    AS_ReceiveAudioResult(&result);
    bool success = printAudCmdResult(command.header.command_code, result);
    if (success) {
        printf("Player stop command successful.\n"); // Added log
    } else {
        printf("Player stop command failed.\n"); // Added log
    }
    return success;
}

static bool app_set_clkmode(int clk_mode)
{
  AudioCommand command;
  command.header.packet_length = LENGTH_SETRENDERINGCLK;
  command.header.command_code  = AUDCMD_SETRENDERINGCLK;
  command.header.sub_code      = 0x00;
  command.set_renderingclk_param.clk_mode = clk_mode;
  printf("Setting rendering clock mode: %d\n", clk_mode); // Added log
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Clock mode set successful.\n"); // Added log
  } else {
      printf("Clock mode set failed.\n"); // Added log
  }
  return success;
}

static bool app_init_outputmixer(void)
{
  AudioCommand command;
  command.header.packet_length = LENGTH_INIT_OUTPUTMIXER;
  command.header.command_code  = AUDCMD_INIT_OUTPUTMIXER;
  command.header.sub_code      = 0x00;
  command.init_mixer_param.player_id     = AS_PLAYER_ID_0;
#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC
  command.init_mixer_param.postproc_type = AsPostprocTypeUserCustom;
#else /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */
  command.init_mixer_param.postproc_type = AsPostprocTypeThrough;
#endif /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */
  snprintf(command.init_mixer_param.dsp_path,
           sizeof(command.init_mixer_param.dsp_path),
           "%s/POSTPROC", DSPBIN_FILE_PATH);
  printf("Initializing output mixer with DSP path '%s'\n", command.init_mixer_param.dsp_path); // Added log
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Output mixer initialization successful.\n"); // Added log
  } else {
      printf("Output mixer initialization failed.\n"); // Added log
  }
  return success;
}

#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC
static bool app_send_initpostproc_command(void)
{
  InitParam initpostcmd;

  AudioCommand command;
  command.header.packet_length = LENGTH_INITMPP;
  command.header.command_code  = AUDCMD_INITMPP;
  command.init_mpp_param.player_id        = AS_PLAYER_ID_0;
  command.init_mpp_param.initpp_param.addr = reinterpret_cast<uint8_t *>(&initpostcmd);
  command.init_mpp_param.initpp_param.size = sizeof(initpostcmd);

  /* Create Postfilter command */
  printf("Sending init post-proc command.\n"); // Added log
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Init post-proc command successful.\n"); // Added log
  } else {
      printf("Init post-proc command failed.\n"); // Added log
  }
  return success;
}

static bool app_send_setpostproc_command(void)
{
  static bool s_toggle = false;
  s_toggle = (s_toggle) ? false : true;

  /* Create packet area (have to ensure until API returns.)  */

  SetParam setpostcmd;
  setpostcmd.enable = s_toggle;
  setpostcmd.coef = 99;

  AudioCommand command;
  command.header.packet_length = LENGTH_SUB_SETMPP_COMMON;
  command.header.command_code  = AUDCMD_SETMPPPARAM;
  command.init_mpp_param.player_id        = AS_PLAYER_ID_0;
  command.init_mpp_param.initpp_param.addr = reinterpret_cast<uint8_t *>(&setpostcmd);
  command.init_mpp_param.initpp_param.size = sizeof(SetParam);

  /* Create Postfilter command */
  printf("Sending set post-proc command. Toggle state: %d\n", s_toggle); // Added log
  AS_SendAudioCommand(&command);

  AudioResult result;
  AS_ReceiveAudioResult(&result);
  bool success = printAudCmdResult(command.header.command_code, result);
  if (success) {
      printf("Set post-proc command successful.\n"); // Added log
  } else {
      printf("Set post-proc command failed.\n"); // Added log
  }
  return success;
}
#endif /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */

static bool app_init_libraries(void)
{
  int ret;
  uint32_t addr = AUD_SRAM_ADDR;

  printf("Initializing audio libraries (shared memory, message lib, memory manager)...\n"); // Added log

  /* Initialize shared memory.*/
  ret = mpshm_init(&s_shm, 1, 1024 * 128 * 2);
  if (ret < 0)
    {
      printf("Error: mpshm_init() failure. %d\n", ret);
      return false;
    }
  printf("Shared memory initialized.\n"); // Added log

  ret = mpshm_remap(&s_shm, (void *)addr);
  if (ret < 0)
    {
      printf("Error: mpshm_remap() failure. %d\n", ret);
      return false;
    }
  printf("Shared memory remapped to 0x%lx.\n", (unsigned long)addr); // Added log

  /* Initalize MessageLib. */
  err_t err = MsgLib::initFirst(NUM_MSGQ_POOLS, MSGQ_TOP_DRM);
  if (err != ERR_OK)
    {
      printf("Error: MsgLib::initFirst() failure. 0x%x\n", err);
      return false;
    }
  printf("MessageLib initFirst successful.\n"); // Added log

  err = MsgLib::initPerCpu();
  if (err != ERR_OK)
    {
      printf("Error: MsgLib::initPerCpu() failure. 0x%x\n", err);
      return false;
    }
  printf("MessageLib initPerCpu successful.\n"); // Added log

  void* mml_data_area = translatePoolAddrToVa(MEMMGR_DATA_AREA_ADDR);
  err = Manager::initFirst(mml_data_area, MEMMGR_DATA_AREA_SIZE);
  if (err != ERR_OK)
    {
      printf("Error: Manager::initFirst() failure. 0x%x\n", err);
      return false;
    }
  printf("Memory Manager initFirst successful.\n"); // Added log

  err = Manager::initPerCpu(mml_data_area, static_pools, pool_num, layout_no);
  if (err != ERR_OK)
    {
      printf("Error: Manager::initPerCpu() failure. 0x%x\n", err);
      return false;
    }
  printf("Memory Manager initPerCpu successful.\n"); // Added log

  /* Create static memory pool. */
  const uint8_t sec_no = AUDIO_SECTION;
  const NumLayout layout_no = MEM_LAYOUT_PLAYER_MAIN_ONLY;
  void* work_va = translatePoolAddrToVa(S0_MEMMGR_WORK_AREA_ADDR);
  const PoolSectionAttr *ptr  = &MemoryPoolLayouts[AUDIO_SECTION][layout_no][0];
  err = Manager::createStaticPools(sec_no, layout_no,
                             work_va,
                             S0_MEMMGR_WORK_AREA_SIZE,
                             ptr);
  if (err != ERR_OK)
    {
      printf("Error: Manager::createStaticPools() failure. %d\n", err);
      return false;
    }
  printf("Static memory pools created.\n"); // Added log

  return true;
}

static bool app_finalize_libraries(void)
{
  printf("Finalizing audio libraries...\n"); // Added log
  /* Finalize MessageLib. */
  MsgLib::finalize();
  printf("MessageLib finalized.\n"); // Added log

  /* Destroy static pools. */
  MemMgrLite::Manager::destroyStaticPools(AUDIO_SECTION);
  printf("Static pools destroyed.\n"); // Added log

  /* Finalize memory manager. */
  MemMgrLite::Manager::finalize();
  printf("Memory manager finalized.\n"); // Added log

  /* Destroy shared memory. */
  int ret;
  ret = mpshm_detach(&s_shm);
  if (ret < 0)
    {
      printf("Error: mpshm_detach() failure. %d\n", ret);
      return false;
    }
  printf("Shared memory detached.\n"); // Added log

  ret = mpshm_destroy(&s_shm);
  if (ret < 0)
    {
      printf("Error: mpshm_destroy() failure. %d\n", ret);
      return false;
    }
  printf("Shared memory destroyed.\n"); // Added log

  return true;
}

static int app_play_file_open(FAR const char *file_path, FAR int32_t *file_size)
{
  int fd = open(file_path, O_RDONLY);

  *file_size = 0;
  if (fd >= 0)
    {
      struct stat stat_buf;
      if (stat(file_path, &stat_buf) == OK)
        {
          *file_size = stat_buf.st_size;
          printf("File opened: %s, size: %ld bytes\n", file_path, (long)stat_buf.st_size); // Added log
        } else {
          printf("Error: stat() failed for %s, errno: %d\n", file_path, get_errno()); // Added log
        }
    } else {
      printf("Error: open() failed for %s, errno: %d\n", file_path, get_errno()); // Added log
    }

  return fd;
}

static bool app_open_next_play_file(void)
{
  /* Get next track */
  // NOTE: app_get_next_track is only called when CONFIG_AUDIOUTILS_PLAYLIST is enabled
  // or for the initial track determination. For dynamically requested tracks
  // via g_requested_track_title, s_player_info.file.track.title is set directly
  // in audio_playback_thread.
  
  char full_path[128];
  snprintf(full_path,
           sizeof(full_path),
           "%s/%s",
           PLAYBACK_FILE_PATH,
           s_player_info.file.track.title); // Use the title set in s_player_info.file.track

  s_player_info.file.fd = app_play_file_open(full_path, &s_player_info.file.size);
  if (s_player_info.file.fd < 0)
    {
      printf("Error: %s open error. check paths and files!\n", full_path);
      return false;
    }
  if (s_player_info.file.size == 0)
    {
      close(s_player_info.file.fd);
      printf("Error: %s file size is abnormal. check files!\n",full_path);
      return false;
    }

  /* Push data to simple fifo */
  if (!app_first_push_simple_fifo(s_player_info.file.fd))
    {
      printf("Error: app_first_push_simple_fifo() failure.\n");
      CMN_SimpleFifoClear(&s_player_info.fifo.handle);
      close(s_player_info.file.fd);
      return false;
    }

  return true;
}

static bool app_close_play_file(void)
{
  if (s_player_info.file.fd >= 0) // Ensure file descriptor is valid
  {
    if (close(s_player_info.file.fd) != 0)
      {
        printf("Error: close() failure.\n");
        return false;
      }
    s_player_info.file.fd = -1; // Invalidate FD
    printf("Playback file closed.\n"); // Added log
  }

  CMN_SimpleFifoClear(&s_player_info.fifo.handle);
  printf("Simple FIFO cleared.\n"); // Added log

  return true;
}

static bool app_start_player_operation(void)
{
  /* Init Player */
  Track *t = &s_player_info.file.track;
  if (!app_init_player(t->codec_type,
                       t->sampling_rate,
                       t->channel_number,
                       t->bit_length))
    {
      printf("Error: app_init_player() failure.\n");
      app_close_play_file();
      return false;
    }

  /* Play Player */
  if (!app_play_player())
    {
      printf("Error: app_play_player() failure.\n");
      app_close_play_file();
      return false;
    }

  return true;
}

static bool app_stop_player_operation(void)
{
  bool result = true;

  /* Set stop mode. */
  int  stop_mode = (s_player_info.file.size != 0) ?
                    AS_STOPPLAYER_NORMAL : AS_STOPPLAYER_ESEND;

  if (!app_stop_player(stop_mode))
    {
      printf("Error: app_stop_player() failure.\n");
      result = false;
    }

  if (!app_close_play_file())
    {
      printf("Error: app_close_play_file() failure.\n");
      result = false;
    }

  return result;
}

static bool audio_player_init(void)
{
  int clk_mode = -1; // Current clock mode

  printf("Initializing AudioPlayer...\n");

  /* First, initialize the shared memory and memory utility used by AudioSubSystem. */
  if (!app_init_libraries())
    {
      printf("Error: init_libraries() failure.\n");
      return false;
    }

  /* Next, Create the features used by AudioSubSystem. */
  if (!app_create_audio_sub_system())
    {
      printf("Error: act_audiosubsystem() failure.\n");
      goto errout_act_audio_sub_system;
    }

  /* Open directory of play contents. */
  if (!app_open_contents_dir())
    {
      printf("Error: app_open_contents_dir() failure.\n");
      goto errout_open_contents_dir;
    }

  /* Initialize frequency lock parameter. */
  app_init_freq_lock();

  /* Lock cpu frequency to high. */
  app_freq_lock();

  /* Change AudioSubsystem to Ready state so that I/O parameters can be changed. */
  if (!app_power_on())
    {
      printf("Error: app_power_on() failure.\n");
      goto errout_power_on;
    }

#ifdef CONFIG_AUDIOUTILS_PLAYLIST
  /* Open playlist. */
  if (!app_open_playlist())
    {
      printf("Error: app_open_playlist() failure.\n");
      goto errout_init_simple_fifo; /* This is an error path, but goes to the right cleanup */
    }
#endif

  /* Initialize simple fifo. */
  if (!app_init_simple_fifo())
    {
      printf("Error: app_init_simple_fifo() failure.\n");
#ifdef CONFIG_AUDIOUTILS_PLAYLIST
      goto errout_open_playlist;
#else
      goto errout_power_on; // Simplified path if no playlist
#endif
    }

  /* Set the device to output the mixed audio. */
  if (!app_init_output_select())
    {
      printf("Error: app_init_output_select() failure.\n");
      goto errout_init_output_select;
    }

  /* Determine clock mode based on the first track */
  // Initial track info is needed to set clock mode. This will use PLAYBACK_FILE_NAME
  if (!app_get_next_track(&s_player_info.file.track)) {
      printf("Error: Could not get initial track info.\n");
      goto errout_get_track;
  }
  // Rewind track for later playback (if playlist is not used, track info is static)
#ifdef CONFIG_AUDIOUTILS_PLAYLIST
  if (s_player_info.playlist_ins) { // if playlist is used
      s_player_info.playlist_ins->select(Playlist::ListTypeAllTrack, NULL);
  }
#endif

  int cur_clk_mode;
  if (s_player_info.file.track.sampling_rate <= AS_SAMPLINGRATE_48000)
    {
      cur_clk_mode = AS_CLKMODE_NORMAL;
    }
  else
    {
      cur_clk_mode = AS_CLKMODE_HIRES;
    }

#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_MODE_NORMAL
  if (cur_clk_mode == AS_CLKMODE_HIRES)
    {
      printf("Hi-Res file is not supported. Please change player mode to Hi-Res with config.\n");
      goto errout_high_res_unsupported;
    }
#endif

  if (clk_mode != cur_clk_mode)
    {
      clk_mode = cur_clk_mode;
      printf("Clock mode changed to %d. Reconfiguring audio path...\n", clk_mode); // Added log

      if (AS_MNG_STATUS_READY != app_get_status())
        {
          if (board_external_amp_mute_control(true) != OK)
            {
              printf("Error: board_external_amp_mute_control(true) failure.\n");
              goto errout_amp_mute_control;
            }
          printf("External amp muted.\n"); // Added log

          if (!app_set_ready())
            {
              printf("Error: app_set_ready() failure.\n");
              goto errout_set_ready_status;
            }
        }

      if (!app_set_clkmode(clk_mode))
        {
          printf("Error: app_set_clkmode() failure.\n");
          goto errout_set_clkmode;
        }

      if (!app_set_player_status())
        {
          printf("Error: app_set_player_status() failure.\n");
          goto errout_set_player_status;
        }

      if (!app_init_outputmixer())
        {
          printf("Error: app_init_outputmixer() failure.\n");
          goto errout_init_outputmixer;
        }

#ifdef CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC
      if (!app_send_initpostproc_command())
        {
          printf("Error: app_send_initpostproc_command() failure.\n");
          goto errout_init_postproc;
        }
#endif /* CONFIG_EXAMPLES_AUDIO_PLAYER_USEPOSTPROC */

       if(!app_set_volume(PLAYER_DEF_VOLUME))
        {
          printf("Error: app_set_volume() failure.\n");
          goto errout_init_outputmixer; /* Can use same error path */
        }

      if (board_external_amp_mute_control(false) != OK)
        {
          printf("Error: board_external_amp_mute_control(false) failure.\n");
          goto errout_amp_mute_control;
        }
      printf("External amp unmuted.\n"); // Added log
    }

  printf("AudioPlayer initialized successfully.\n");
  return true;

errout_init_postproc:
errout_init_outputmixer:
errout_set_player_status:
errout_set_clkmode:
errout_set_ready_status:
errout_amp_mute_control:
  if (board_external_amp_mute_control(true) != OK) {
    printf("Error: Final mute control failed.\n");
  }
errout_high_res_unsupported:
errout_get_track:
errout_init_output_select:
errout_init_simple_fifo:
#ifdef CONFIG_AUDIOUTILS_PLAYLIST
errout_open_playlist:
  app_close_playlist();
#endif /* CONFIG_AUDIOUTILS_PLAYLIST */
errout_power_on:
  if (AS_MNG_STATUS_READY != app_get_status())
    {
      app_set_ready();
    }
  app_power_off();
  app_freq_release();
  app_close_contents_dir();
errout_open_contents_dir:
  app_deact_audio_sub_system();
errout_act_audio_sub_system:
  app_finalize_libraries();
  return false;
}

static void audio_player_finalize(void)
{
  printf("Finalizing AudioPlayer...\n"); // Added log
  board_external_amp_mute_control(true);
  printf("External amp muted during finalization.\n"); // Added log
  // app_stop_player_operation(); // This should be called after g_stop_audio_signal is handled by audio_playback_thread
  if (AS_MNG_STATUS_READY != app_get_status())
    {
      app_set_ready();
    }
  app_power_off();
  app_freq_release();
#ifdef CONFIG_AUDIOUTILS_PLAYLIST
  app_close_playlist();
#endif
  app_close_contents_dir();
  app_deact_audio_sub_system();
  app_finalize_libraries();
  printf("AudioPlayer finalized.\n"); // Added log
}

// Function to stop current playback and prepare for next track
static void audio_stop_and_prepare_next(const char* track_title) { // Modified to accept track_title
    pthread_mutex_lock(&g_audio_mutex);
    if (g_is_audio_playing) {
        g_stop_audio_signal = true; // Signal to stop current playback
        printf("Signaled audio thread to STOP current playback for next track.\n");
    }
    // Copy the requested track title
    strncpy(g_requested_track_title, track_title, sizeof(g_requested_track_title) - 1);
    g_requested_track_title[sizeof(g_requested_track_title) - 1] = '\0'; // Ensure null termination

    g_play_next_track_signal = true; // Signal to play next track
    pthread_cond_signal(&g_audio_cond); // Wake up the audio thread
    pthread_mutex_unlock(&g_audio_mutex);
}

static void *audio_playback_thread(void *arg)
{
  printf("Audio playback thread started.\n");

  if (!audio_player_init()) {
      printf("Audio player initialization failed. Exiting audio thread.\n");
      g_audio_thread_finished = true; // Mark thread as finished
      return NULL;
  }

  while (!g_audio_thread_finished)
    {
      pthread_mutex_lock(&g_audio_mutex);
      // Wait for play, next track, or termination signal
      while (!g_play_audio_signal && !g_play_next_track_signal && !g_audio_thread_finished)
        {
          pthread_cond_wait(&g_audio_cond, &g_audio_mutex);
        }

      if (g_audio_thread_finished)
        {
          pthread_mutex_unlock(&g_audio_mutex);
          break; // Exit loop if thread needs to terminate
        }

      // Handle stop signal first (if any previous playback needs to be stopped)
      if (g_stop_audio_signal) {
          if (g_is_audio_playing) {
              printf("Audio thread received STOP signal. Stopping playback.\n");
              app_stop_player_operation(); // Stop current playback
              g_is_audio_playing = false; // Ensure flag is reset
          }
          g_stop_audio_signal = false; // Reset stop signal
      }

      bool should_play_current_request = g_play_next_track_signal; // Was a specific track requested?
      bool should_play_default = g_play_audio_signal && !g_is_audio_playing; // Play default only if not playing

      // Reset signals as they are being processed
      g_play_next_track_signal = false;
      g_play_audio_signal = false;

      if (should_play_current_request || should_play_default) {
          g_is_audio_playing = true; // Indicate that audio is now playing
          pthread_mutex_unlock(&g_audio_mutex); // Unlock during potentially long operations

          printf("Audio playback signal received. Starting playback.\n");

          // Determine which track to prepare and play
          if (should_play_current_request) {
              // Specific track requested via g_requested_track_title
              strncpy(s_player_info.file.track.title, g_requested_track_title, sizeof(s_player_info.file.track.title) - 1);
              s_player_info.file.track.title[sizeof(s_player_info.file.track.title) - 1] = '\0';
              
              // Assume default audio properties for dynamically loaded tracks
              s_player_info.file.track.channel_number = PLAYBACK_CH_NUM;
              s_player_info.file.track.bit_length     = PLAYBACK_BIT_LEN;
              s_player_info.file.track.sampling_rate  = PLAYBACK_SAMPLING_RATE;
              s_player_info.file.track.codec_type     = PLAYBACK_CODEC_TYPE;
              printf("Preparing to play requested track: '%s'\n", s_player_info.file.track.title);

          } else { // should_play_default is true
              // No specific request, play the default track (e.g., from PLAYBACK_FILE_NAME)
              if (!app_get_next_track(&s_player_info.file.track)) {
                  printf("Error: Could not get default initial track info for playback.\n");
                  pthread_mutex_lock(&g_audio_mutex); // Re-lock to update g_is_audio_playing
                  g_is_audio_playing = false;
                  pthread_mutex_unlock(&g_audio_mutex);
                  continue; // Skip to next loop iteration
              }
              printf("Preparing to play default track: '%s'\n", s_player_info.file.track.title);
          }

          // Open and start playback of the determined file
          if (app_open_next_play_file())
            {
              if (app_start_player_operation())
                {
                  printf("Audio is playing...\n");
                  // Keep refilling FIFO until file ends or stop signal
                  while (s_player_info.file.size > 0 && g_is_audio_playing && !g_audio_thread_finished)
                    {
                      usleep(20 * 1000); // Check every 20ms
                      pthread_mutex_lock(&g_audio_mutex); // Lock to check g_is_audio_playing and g_stop_audio_signal
                      bool current_is_playing = g_is_audio_playing;
                      bool current_stop_signal = g_stop_audio_signal;
                      pthread_mutex_unlock(&g_audio_mutex);

                      if (!current_is_playing || current_stop_signal) {
                          printf("Audio thread: Stop signal received during playback loop or g_is_audio_playing set to false.\n");
                          break; // Break if stop requested or playback explicitly turned off
                      }

                      if (!app_refill_simple_fifo(s_player_info.file.fd))
                        {
                          // EOF is not an error here, it just means refill is done.
                          if (s_player_info.file.size > 0)
                            {
                               printf("Audio thread: Error refilling FIFO.\n");
                               break;
                            }
                        }
                    }
                  printf("Audio playback finished or stopped.\n");
                  app_stop_player_operation();
                }
            } else {
                printf("Failed to open audio file for playback.\n");
            }
          pthread_mutex_lock(&g_audio_mutex); // Re-lock before setting g_is_audio_playing
          g_is_audio_playing = false; // Audio is no longer playing
          pthread_mutex_unlock(&g_audio_mutex);
      } else {
          pthread_mutex_unlock(&g_audio_mutex); // Unlock if no playback initiated
      }
    }

  audio_player_finalize();
  printf("Audio playback thread terminated.\n");
  return NULL;
}

/****************************************************************************
 * toio_thread_main
 ****************************************************************************/

static void *toio_thread_main(void *arg)
{
  Identification id(g_id_handle);
  Motor   motor(g_motor_handle);
  Light   light(g_light_handle);
  Sound   sound(g_sound_handle);
  Sensor  sensor(g_sensor_handle);
  Button  button(g_button_handle);
  Battery battery(g_battery_handle);
  Setting setting(g_setting_handle);

  bool mat = false;
  
  // Variable to hold the current volume level. Initialized with the default value.
  static int current_volume = PLAYER_DEF_VOLUME;

  printf("Protocol version: %s\n", setting.version());
  printf("Battery level: %d %%\n", battery.level());

  printf("toio_thread_main: Initializing toio functions...\n");

  printf("Play sound effect: Enter\n");
  // sound.play_sound_effect(Sound::Enter); // Commented out as requested


  while (!g_thread_finished)
    {
      if (sensor.tilt_detected())
        {
          // ### MODIFIED: START/STOP audio playback based on tilt direction ###
          int tilt_direction = sensor.posture();
          printf("Motion: Tilt detected! Direction: %d\n", tilt_direction);

          // 5: 上向き / Tilted upward
          // 6: 下向き / Tilted downward
          switch (tilt_direction)
            {
              case 4: // tilt_direction 4 を検出したら再生
                printf("Turn on light: Blue\n");
                light.on(0, 0, 255);
                printf("Play sound effect: Get1\n");
                // sound.play_sound_effect(Sound::Get1); // Commented out as requested

                // オーディオスレッドに再生開始を通知
                pthread_mutex_lock(&g_audio_mutex);
                if (!g_is_audio_playing) { // 再生中でなければ開始シグナルを送る
                    g_play_audio_signal = true;
                    pthread_cond_signal(&g_audio_cond);
                    printf("Signaled audio thread to PLAY.\n");
                } else {
                    printf("Audio is already playing, ignoring start signal.\n");
                }
                pthread_mutex_unlock(&g_audio_mutex);
                break;

              default: // その他を検出したら停止する。
                printf("Turn on light: Yellow\n");
                light.on(255, 255, 0);
                printf("Play sound effect: Effect1\n");
                // sound.play_sound_effect(Sound::Effect1); // Commented out as requested

                // オーディオスレッドに停止を通知
                // pthread_mutex_lock(&g_audio_mutex);
                // if (g_is_audio_playing) { // 再生中であれば停止フラグを立てる
                //     g_is_audio_playing = false; // このフラグで再生ループが終了する
                //     printf("Signaled audio thread to STOP.\n");
                // } else {
                //     printf("Audio is not playing, ignoring stop signal.\n");
                // }
                // pthread_mutex_unlock(&g_audio_mutex);
                break;
            }
        }

      if (sensor.collision_detected())
        {
          printf("Motion: Collision detected!!\n");
          printf("Turn on light: Red:\n");
          light.on(255, 0, 0);
          printf("Play sound effect: Get2\n");
          printf("Play Sound3.mp3\n");
          audio_stop_and_prepare_next("Sound3.mp3"); // "Sound3.mp3"を再生
          // sound.play_sound_effect(Sound::Get2); // Commented out as requested
        }

      if (sensor.shake_detected())
        {
          printf("Motion: Shake detected!!\n");
          printf("Turn on light: Green\n");
          light.on(0, 255, 0);
          // ### MODIFIED: Play Sound3.mp3 on shake detection ###
          printf("Play Sound3.mp3\n");
          // audio_stop_and_prepare_next("Sound3.mp3"); // "Sound3.mp3"を再生
        }

      if (button.is_pressed())
        {
          printf("Battery level: %d %%\n", battery.level());
          printf("Turn off light\n");
          // light.off();
          // ### MODIFIED: Play next track on button press ###
          printf("Button pressed: Requesting next track.\n");
          // audio_stop_and_prepare_next(PLAYBACK_FILE_NAME); // 次の曲（デフォルトの曲）を再生
          if (sensor.posture()==4){
            audio_stop_and_prepare_next("Sound4.mp3"); 
            light.on(0, 255, 0);
          }else if (sensor.posture()==5){
            audio_stop_and_prepare_next("Sound5.mp3"); 
            light.on(0, 0, 255);
          }else{
            audio_stop_and_prepare_next("Sound6.mp3"); 
            light.on(0, 255, 255);
          }
        }

      if (!mat && (id.is_valid_position() || id.is_valid_standard()))
        {
          /* In the mat for the first time. */
          mat = true;
          printf("Play sound effect: MatIn\n");
          // sound.play_sound_effect(Sound::MatIn); // Commented out as requested
          if (id.is_valid_position())
            {
              // ### MODIFICATION START ###
              // The original printf is kept.
              printf("(x, y, angle) = (%4d, %4d, %3d)\n", id.x(), id.y(), id.angle());

              // Adjust volume based on x-coordinate.
              if (id.x() <= 900)
                {
                  // Decrease volume (make it more negative)
                  current_volume -= 20;
                  // Clamp to a minimum value to prevent it from becoming too quiet.
                  if (current_volume < -1000) current_volume = -1000;
                  printf("Volume down: %d\n", current_volume);
                }
              else
                {
                  // Increase volume (make it less negative)
                  current_volume += 20;
                  // Clamp to a maximum value (0 dB is typically max).
                  if (current_volume > 0) current_volume = 0;
                  printf("Volume up: %d\n", current_volume);
                }
              
              // Apply the new volume setting.
              app_set_volume(current_volume);
              // ### MODIFICATION END ###
            }
          if (id.is_valid_standard())
            {
              printf("(stadard, angle) = (0x%08lx, %3d)\n", id.value(), id.angle());

            if(id.value()==0x380130){
              audio_stop_and_prepare_next("Sound130.mp3"); 
              }
            if(id.value()==0x380131){
              audio_stop_and_prepare_next("Sound131.mp3"); 
              }
            if(id.value()==0x380132){
              audio_stop_and_prepare_next("Sound132.mp3"); 
              }
            if(id.value()==0x380136){
              audio_stop_and_prepare_next("Sound136.mp3"); 
              }
            if(id.value()==0x380137){
              audio_stop_and_prepare_next("Sound137.mp3"); 
              }
            if(id.value()==0x380138){
              audio_stop_and_prepare_next("Sound138.mp3"); 
              }
            if(id.value()==0x380139){
              audio_stop_and_prepare_next("Sound139.mp3"); 
              }
            if(id.value()==0x380143){
              audio_stop_and_prepare_next("Sound143.mp3"); 
              }
            if(id.value()==0x380144){
              audio_stop_and_prepare_next("Sound144.mp3"); 
              }
            if(id.value()==0x380145){
              audio_stop_and_prepare_next("Sound145.mp3"); 
              }


            }
        }

      if (mat && (!id.is_valid_position() && !id.is_valid_standard()))
        {
          /* Out of the mat. */
          mat = false;
          printf("Play sound effect: MatOut\n");
          // sound.play_sound_effect(Sound::MatOut); // Commented out as requested
        }

      /* Yield periodically to dispatch to other threads. */
      usleep(100 * 1000); // 100ms sleep to avoid busy-waiting and chattering
    }

  printf("toio thread finished.\n");
  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

extern "C" int main(int argc, FAR char *argv[])
{
  int ret;
  char myname[]  = "BLE_TOIO_CENTRAL_AUDIO";
  BT_ADDR myaddr = {{0x19, 0x84, 0x06, 0x14, 0xAB, 0xCD}};
  BLE_UUID service_uuid;
  pthread_t toio_thread = (pthread_t)0;
  pthread_t audio_thread = (pthread_t)0;

  /* Convert UUID strings to BLE_UUID value. */
  bleutil_convert_str2uuid(const_cast<char *>(SERVICE_UUID), &service_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(ID_UUID),      &g_id_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(MOTOR_UUID),   &g_motor_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(LIGHT_UUID),   &g_light_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(SOUND_UUID),   &g_sound_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(SENSOR_UUID),  &g_sensor_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(BUTTON_UUID),  &g_button_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(BATTERY_UUID), &g_battery_uuid);
  bleutil_convert_str2uuid(const_cast<char *>(SETTING_UUID), &g_setting_uuid);

  /* Initialize BLE central application. */
  ble_app_init();

  ble_set_name(myname);
  ble_set_address(&myaddr);

  ret = bt_enable();
  if (ret != BT_SUCCESS)
    {
      printf("ERROR: bt_enable() ret=%d\n", ret);
      goto error_ble_disable;
    }

  ret = ble_enable();
  if (ret != BT_SUCCESS)
    {
      printf("ERROR: ble_enable() ret=%d\n", ret);
      goto error_ble_disable;
    }

  /* Allow the vendor specific UUIDs to be discovered. */
  ble_set_vendor_uuid(&service_uuid);

  /* Set scan filter to connect if the specified UUID is found. */
  scan_filter_uuid(&service_uuid);

  /* Discover characteristic to get the handle. */
  register_discover(&g_id_uuid,      char_discovered);
  register_discover(&g_motor_uuid,   char_discovered);
  register_discover(&g_light_uuid,   char_discovered);
  register_discover(&g_sound_uuid,   char_discovered);
  register_discover(&g_sensor_uuid,  char_discovered);
  register_discover(&g_button_uuid,  char_discovered);
  register_discover(&g_battery_uuid, char_discovered);
  register_discover(&g_setting_uuid, char_discovered);

  // Create audio playback thread
  ret = pthread_create(&audio_thread, NULL, audio_playback_thread, NULL);
  if (ret != 0) {
      printf("ERROR: pthread_create(audio_thread) failed: %d\n", ret);
      goto error_unregister_discover;
  }

  ret = ble_start_scan(false);
  if (ret != BT_SUCCESS)
    {
      printf("[%s] ble_start_scan() failed. ret = %d\n", __func__, ret);
      goto error_stop_threads;
    }

  while (1)
    {
      ble_app_event ev = ble_app_wait_event();
      if (g_audio_thread_finished)
        {
          // Exit if audio thread terminated prematurely
          printf("Audio thread terminated, exiting application.\n");
          goto error_stop_threads;
        }

      switch (ev)
        {
          case BLE_APP_CONNECTED:
            g_thread_finished = false; // Reset toio thread termination flag
            ret = pthread_create(&toio_thread, NULL, toio_thread_main, NULL);
            if (ret != 0)
              {
                printf("ERROR: pthread_create(toio_thread) failed: %d\n", ret);
                goto error_stop_threads;
              }
            break;
          case BLE_APP_DISCONNECTED:
            printf("Finish application.\n");
            goto error_stop_threads; // Proceed to cleanup
            break;
          default:
            break;
        }
    }

error_stop_threads:
  if (toio_thread != (pthread_t)0)
    {
      g_thread_finished = true; // Signal toio thread to terminate
      pthread_join(toio_thread, NULL); // Wait for toio thread to exit
    }
  if (audio_thread != (pthread_t)0)
    {
      // Signal audio thread to terminate and wait for it
      pthread_mutex_lock(&g_audio_mutex);
      g_audio_thread_finished = true;
      pthread_cond_signal(&g_audio_cond); // Wake up thread if it's waiting
      pthread_mutex_unlock(&g_audio_mutex);
      pthread_join(audio_thread, NULL); // Wait for audio thread to exit
    }

error_unregister_discover:
  /* Unregister discover callbacks. */
  unregister_discover(&g_id_uuid);
  unregister_discover(&g_motor_uuid);
  unregister_discover(&g_light_uuid);
  unregister_discover(&g_sound_uuid);
  unregister_discover(&g_sensor_uuid);
  unregister_discover(&g_button_uuid);
  unregister_discover(&g_battery_uuid);
  unregister_discover(&g_setting_uuid);

error_ble_disable:
  ble_disable();
  bt_disable();

  printf("Application terminated.\n");
  return ret;
}

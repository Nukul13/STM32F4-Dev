/**
  ******************************************************************************
  * @file    Audio/Audio_playback_and_record/Src/waveplayer.c
  * @author  MCD Application Team
  * @version V1.2.1
  * @date    13-March-2015
  * @brief   I2S Audio player program.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "waveplayer.h"
//#include "stm32f4_discovery_audio.h"

/** @addtogroup STM32F4-Discovery_Audio_Player_Recorder
* @{
*/
#ifdef USE_STM32F4_AUDIO
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define AUDIO_BUFFER_SIZE             4096

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* LED State (Toggle or OFF)*/
__IO uint32_t LEDsState;

/* Counter for User button presses. Defined as external in waveplayer.c file */
__IO uint32_t PressCount = 0;

/* Wave Player Pause/Resume Status. Defined as external in waveplayer.c file */
__IO uint32_t PauseResumeStatus = IDLE_STATUS;

/* Re-play Wave file status on/off.
   Defined as external in waveplayer.c file */
__IO uint32_t RepeatState = REPEAT_ON;

/* Audio Play Start variable.
   Defined as external in main.c*/
__IO uint32_t AudioPlayStart = 0;

/* Audio wave data length to be played */
static uint32_t WaveDataLength = 0;

/* Audio wave remaining data length to be played */
static __IO uint32_t AudioRemSize = 0;

/* Ping-Pong buffer used for audio play */
uint8_t Audio_Buffer[AUDIO_BUFFER_SIZE];

/* Position in the audio play buffer */
__IO BUFFER_StateTypeDef buffer_offset = BUFFER_OFFSET_NONE;

/* Initial Volume level (from 0 (Mute) to 100 (Max)) */
static uint8_t Volume = 70;

/* Variable used to indicate audio mode (play, record or stop). */
/* Defined in main.c */
extern __IO uint32_t CmdIndex;

/* Variable used by FatFs*/
FIL FileRead;
DIR Directory;

/* Variable used to switch play from audio sample available on USB to recorded file. */
/* Defined in waverecorder.c */
extern uint32_t WaveRecStatus;

/* Variable to indicate USB state (start/idle) */
/* Defined in main.c */
extern ApplicationTypeDef Appli_state;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Plays Wave from a mass storage.
  * @param  AudioFreq: Audio Sampling Frequency
  * @retval None
*/
void WavePlayBack(uint32_t AudioFreq)
{
  UINT bytesread = 0;

  /* Start playing */
  AudioPlayStart = 1;
  RepeatState = REPEAT_ON;

  /* Initialize Wave player (Codec, DMA, I2C) */
  if(WavePlayerInit(AudioFreq) != 0)
  {
    uprintf("Error\n\r");
  }

  /* Get Data from USB Flash Disk */
  f_lseek(&FileRead, 0);
  f_read (&FileRead, &Audio_Buffer[0], AUDIO_BUFFER_SIZE, &bytesread);
  AudioRemSize = WaveDataLength - bytesread;

  /* Start playing Wave */
  BSP_AUDIO_OUT_Play((uint16_t*)&Audio_Buffer[0], AUDIO_BUFFER_SIZE);
  PauseResumeStatus = RESUME_STATUS;
  PressCount = 0;

  /* Check if the device is connected.*/
  while((AudioRemSize != 0) && (Appli_state != APPLICATION_IDLE))
  {
    /* Test on the command: Playing */
    if(CmdIndex == CMD_PLAY)
    {
      if(PauseResumeStatus == PAUSE_STATUS)
      {
        /* Stop Toggling LED2 to signal Pause */
        /* Pause playing Wave */
        WavePlayerPauseResume(PauseResumeStatus);
        PauseResumeStatus = IDLE_STATUS;
      }
      else if(PauseResumeStatus == RESUME_STATUS)
      {
        /* Toggling LED6 to signal Play */
        /* Resume playing Wave */
        WavePlayerPauseResume(PauseResumeStatus);
        PauseResumeStatus = IDLE_STATUS;
      }

      bytesread = 0;

      if(buffer_offset == BUFFER_OFFSET_HALF)
      {

        f_read(&FileRead,
               &Audio_Buffer[0],
               AUDIO_BUFFER_SIZE/2,
               (void *)&bytesread);

          buffer_offset = BUFFER_OFFSET_NONE;
      }

      if(buffer_offset == BUFFER_OFFSET_FULL)
      {
        f_read(&FileRead,
               &Audio_Buffer[AUDIO_BUFFER_SIZE/2],
               AUDIO_BUFFER_SIZE/2,
               (void *)&bytesread);

          buffer_offset = BUFFER_OFFSET_NONE;
      }
      if(AudioRemSize > (AUDIO_BUFFER_SIZE / 2))
      {
        AudioRemSize -= bytesread;
      }
      else
      {
        AudioRemSize = 0;
      }
    }
    else
    {
      /* Stop playing Wave */
      WavePlayerStop();
      /* Close file */
      f_close(&FileRead);
      AudioRemSize = 0;
      RepeatState = REPEAT_ON;
      break;
    }
  }
#ifdef PLAY_REPEAT_DISABLED
  RepeatState = REPEAT_OFF;
  /* Stop playing Wave */
  WavePlayerStop();
  /* Close file */
  f_close(&FileRead);
  /* Test on the command: Playing */
  if(CmdIndex == CMD_PLAY)
  {
    LEDsState = LED4_TOGGLE;
  }
#else
  RepeatState = REPEAT_ON;
  AudioPlayStart = 0;
  /* Stop playing Wave */
  WavePlayerStop();
  /* Close file */
  f_close(&FileRead);
#endif /* PLAY_REPEAT_DISABLED */
}

/**
  * @brief  Pauses or Resumes a played Wave.
  * @param  state: Player state: Pause, Resume or Idle
  * @retval None
  */
void WavePlayerPauseResume(uint32_t wState)
{
  if(wState == PAUSE_STATUS)
  {
    BSP_AUDIO_OUT_Pause();
  }
  else
  {
    BSP_AUDIO_OUT_Resume();
  }
}

/**
  * @brief  Stops playing Wave.
  * @param  None
  * @retval None
  */
void WavePlayerStop(void)
{
  BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW);
}

/**
* @brief  Initializes the Wave player.
* @param  AudioFreq: Audio sampling frequency
* @retval None
*/
int WavePlayerInit(uint32_t AudioFreq)
{
  /* MEMS Accelerometer configure to manage PAUSE, RESUME operations */
//  BSP_ACCELERO_Click_ITConfig();

  /* Initialize the Audio codec and all related peripherals (I2S, I2C, IOExpander, IOs...) */
  return(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, Volume, AudioFreq));
}

/*--------------------------------
Callbacks implementation:
The callbacks prototypes are defined in the stm32f4_discovery_audio_codec.h file
and their implementation should be done in the user code if they are needed.
Below some examples of callback implementations.
--------------------------------------------------------*/

/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
  buffer_offset = BUFFER_OFFSET_HALF;
}

/**
* @brief  Calculates the remaining file size and new position of the pointer.
* @param  None
* @retval None
*/
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
  buffer_offset = BUFFER_OFFSET_FULL;
  BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)&Audio_Buffer[0], AUDIO_BUFFER_SIZE / 2);
}

/**
* @brief  Manages the DMA FIFO error interrupt.
* @param  None
* @retval None
*/
void BSP_AUDIO_OUT_Error_CallBack(void)
{
  /* Stop the program with an infinite loop */
  while (1)
  {}

  /* Could also generate a system reset to recover from the error */
  /* .... */
}

/**
  * @brief  Starts Wave player.
  * @param  None
  * @retval None
  */
void WavePlayerStart(void)
{
  UINT bytesread = 0;
  char path[] = "0:/";
  char* wavefilename = NULL;
  WAVE_FormatTypeDef waveformat;

  /* Get the read out protection status */
  if(f_opendir(&Directory, path) == FR_OK)
  {
    if(WaveRecStatus == 1)
    {
      wavefilename = REC_WAVE_NAME;
    }
    else
    {
      wavefilename = WAVE_NAME;
    }
    /* Open the Wave file to be played */
    if(f_open(&FileRead, wavefilename , FA_READ) != FR_OK)
    {
      CmdIndex = CMD_RECORD;
    }
    else
    {
      /* Read sizeof(WaveFormat) from the selected file */
      f_read (&FileRead, &waveformat, sizeof(waveformat), &bytesread);

      /* Set WaveDataLenght to the Speech Wave length */
      WaveDataLength = waveformat.FileSize;

      /* Play the Wave */
      WavePlayBack(waveformat.SampleRate);
    }
  }
}

void BSP_Start_Audio(uint8_t filename[255])
{
  UINT bytesread = 0;
  char path[] = "0:/";
  char* wavefilename = NULL;
  WAVE_FormatTypeDef waveformat;
  uint32_t WaveDataLength = 0;

  /* Get the read out protection status */
  if(f_opendir(&Directory, path) == FR_OK)
  {
    wavefilename = filename;

    /* Open the Wave file to be played */
    if(f_open(&FileRead, wavefilename , FA_READ) == FR_OK)
    {
      HAL_Delay(1);
      /* Read sizeof(WaveFormat) from the selected file */
      f_read (&FileRead, &waveformat, sizeof(waveformat), &bytesread);

      /* Set WaveDataLenght to the Speech Wave length */
      WaveDataLength = waveformat.FileSize;

      /* Play */

      /* Initialize  I2S */
      I2S3_Init(waveformat.SampleRate);
      /* Initialize Wave player (Codec, DMA, I2C) */
      if(WavePlayerInit(waveformat.SampleRate) != 0)
      {
        while(1);
      }
      /* Get Data from USB Flash Disk */
      f_lseek(&FileRead, 0);
      f_read (&FileRead, &Audio_Buffer[0], AUDIO_BUFFER_SIZE, &bytesread);
      AudioRemSize = WaveDataLength - bytesread;

      /* Start playing Wave */
      BSP_AUDIO_OUT_Play((uint16_t*)&Audio_Buffer[0], AUDIO_BUFFER_SIZE);
      PauseResumeStatus = RESUME_STATUS;
      PressCount = 0;

      /* Check if the device is connected.*/
      while((AudioRemSize != 0) && (Appli_state != APPLICATION_IDLE))
      {
        /* Test on the command: Playing */
        if(CmdIndex == CMD_PLAY)
        {
          if(PauseResumeStatus == PAUSE_STATUS)
          {
            /* Stop Toggling LED2 to signal Pause */
            /* Pause playing Wave */
            WavePlayerPauseResume(PauseResumeStatus);
            PauseResumeStatus = IDLE_STATUS;
          }
          else if(PauseResumeStatus == RESUME_STATUS)
          {
            /* Toggling LED6 to signal Play */
            /* Resume playing Wave */
            WavePlayerPauseResume(PauseResumeStatus);
            PauseResumeStatus = IDLE_STATUS;
          }

          bytesread = 0;

          if(buffer_offset == BUFFER_OFFSET_HALF)
          {
            f_read(&FileRead,&Audio_Buffer[0],
                   AUDIO_BUFFER_SIZE/2,(void *)&bytesread);
            buffer_offset = BUFFER_OFFSET_NONE;
          }

          if(buffer_offset == BUFFER_OFFSET_FULL)
          {
            f_read(&FileRead,&Audio_Buffer[AUDIO_BUFFER_SIZE/2],
                   AUDIO_BUFFER_SIZE/2,(void *)&bytesread);
            buffer_offset = BUFFER_OFFSET_NONE;
          }
          if(AudioRemSize > (AUDIO_BUFFER_SIZE / 2))
          {
            AudioRemSize -= bytesread;
          }
          else
          {
            /* Stop playing Wave */
            WavePlayerStop();
            f_close(&FileRead);
            HAL_I2S_DeInit(&hAudioOutI2s);
            break;
          }
        }
      }
    }
  }
}

/**
  * @brief Wave player.
  * @param  None
  * @retval None
  */
void WavePlayer_CallBack(void)
{
  if(Appli_state != APPLICATION_IDLE)
  {
    /* Reset the Wave player variables */
    RepeatState = REPEAT_ON;
    AudioPlayStart = 0;
    PauseResumeStatus = RESUME_STATUS;
    WaveDataLength =0;
    PressCount = 0;

    /* Stop the Codec */
    if(BSP_AUDIO_OUT_Stop(CODEC_PDWN_HW) != AUDIO_OK)
    {
      uprintf("ERROR\n\r");
    }

    /* Turn OFF LED3, LED4 and LED6 */
  }
}

#endif
/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

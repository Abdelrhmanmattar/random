/*
 * bootloader.c
 *
 *  Created on: May 8, 2024
 *      Author: abdelrhman mattar
 */
#include "rtc.h"
#include "bootloader.h"


static uint8_t RX_LEN;
static uint8_t RxBuffer[150];

static uint8_t IS_REQUEST = False;
static uint8_t Req_len;
static uint8_t Req_ID;
static uint8_t *Req_Data;
static uint8_t UPDATE_TYPE;
static connect_state download_state;

static uint8_t ack_var;
#define Response_positive() ack_var=Req_ID+0x40; HAL_UART_Transmit(&huart2,&ack_var,1, HAL_MAX_DELAY)

#define Response_negative() ack_var = 0xff; HAL_UART_Transmit(&huart2,&ack_var,1, HAL_MAX_DELAY)




uint32_t app_validtion(void)
{
	uint32_t ret = 0xff;
	HAL_PWR_EnableBkUpAccess();
	ret = HAL_RTCEx_BKUPRead(&hrtc, BKP_DR1_D);
	HAL_PWR_DisableBkUpAccess();
	return ret;
}
uint32_t request_validation(void)
{
	uint32_t ret = 0xff;
	HAL_PWR_EnableBkUpAccess();
	ret = HAL_RTCEx_BKUPRead(&hrtc, BKP_DR2_D);
	HAL_PWR_DisableBkUpAccess();
	return ret;
}
uint32_t patch_validtion(void)
{
	uint32_t ret = 0xff;
	HAL_PWR_EnableBkUpAccess();
	ret = HAL_RTCEx_BKUPRead(&hrtc, BKP_DR2_D);
	HAL_PWR_DisableBkUpAccess();
	return ret;
}

void app_config(uint32_t data)
{
	HAL_PWR_EnableBkUpAccess();
	HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR1_D, data);
	HAL_PWR_DisableBkUpAccess();
}
void request_config(uint32_t data)
{
	HAL_PWR_EnableBkUpAccess();
	HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR2_D, data);
	HAL_PWR_DisableBkUpAccess();
}

void patch_config(uint32_t data)
{
	HAL_PWR_EnableBkUpAccess();
	HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR3_D, data);
	HAL_PWR_DisableBkUpAccess();
}

uint32_t determind_path(uint8_t type__up)
{
	uint32_t address;
	if(type__up==1)     //patch
	{
		address = PATCH_TARGET_ADDRESS;
	}
	else if(type__up==2)//full
	{
		if(app_validtion()==2)
		{
			address = FIRST_TARGET_ADDRESS;
		}
		else address = SECOND_TARGET_ADDRESS;
	}
	return address;
}



void RX_HANDLE(void)
{
	//printf("RX_HANDLE\n");
  static uint8_t index = 0;
  static uint8_t state = IDLE;
  // static unsigned short addr_x=10;
  uint8_t x = 0;
  while (!x)
  {
    HAL_UART_Receive(&huart2, &x, 1, HAL_MAX_DELAY);
  }
  //printf("%x ", x);
  if (state == IDLE)
  {
    RX_LEN = x;
    state = BUSY;
  }
  else
  {
    RxBuffer[index++] = x;
    if (index == RX_LEN)
    {
      index = 0;
      state = IDLE;
      Req_Notification(RxBuffer, RX_LEN);
    }
  }
}

void Req_Notification(uint8_t *req, uint8_t len)
{
  IS_REQUEST = True;
  Req_ID = req[0];
  Req_len = len;
  Req_Data = &req[1];
  //printf("\n_func_Req_Notification\n");
}

void Flash_MainTask(void)
{
  static uint8_t page_number = 0;
  static uint16_t code_size = 0, recevied_code = 0;
  uint8_t req_valid = False;
  uint32_t address_= 0x8008000;
  if (IS_REQUEST == True)
  {
    switch (Req_ID)
    {
    case SESSION_CONTROL:
    {
      if (Req_Data[0] == PROGRAMMING_SESSION && Req_len == 2 && download_state == waiting_ProgrammingSession)
      {
        Response_positive();
        download_state=waiting_DownloadRequest;
        //printf("Programming Session\n");
      }
      else
      {
        download_state == waiting_ProgrammingSession;
        Response_negative();
      }
    }
    break;

    case DOWNLOAD_REQUEST:
    {
      if (download_state == waiting_DownloadRequest && Req_len == 6)
      {
        code_size = *(uint32_t*)Req_Data;
        UPDATE_TYPE = Req_Data[4];
        if ( (code_size < MAX_CODE_SIZE && UPDATE_TYPE == 1) || (code_size < MAX_patch_SIZE && UPDATE_TYPE == 2) )
        {
          Response_positive();
          download_state = waiting_TransferData;
          req_valid = True;
          address_=determind_path(UPDATE_TYPE);
          //printf("Download Request\n");
        }
      }
      if (req_valid == False)
      {
        download_state == waiting_ProgrammingSession;
        Response_negative();
      }
      memset(RxBuffer,0xff,150);
      
    }
    break;
    case TRANSFER_DATA:
    {
      if(download_state == waiting_TransferData && Req_len==129)
      {
        if(recevied_code < code_size)
        {
          Flash_Write(address_, Req_Data, 128);
          address_+=128;
          recevied_code+=128;
          Response_positive();

          //printf("Transfer Data %08x\n",address_);
        }
        else
        {
          download_state = waiting_TransferExit;
          return;
        }
      }
      else
      {
        download_state = waiting_ProgrammingSession;
        Response_negative();
      }
      
    }
  }
  IS_REQUEST = False;
  }
}

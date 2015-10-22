#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * R820T tuner driver, taken from Realteks RTL2832U Linux Kernel Driver
 */

#ifndef _WIN32
#include <stdint.h>
#include <stdio.h>
#endif // _WIN32

#include "rtl2832-tuner_r820t.h"
#include <memory.h>

#define LOG_PREFIX			"[r820t] "

///////////////////////////////////////////////////////////////////////////////

#define R820T_I2C_ADDR		0x34
#define R820T_CHECK_ADDR	0x00
#define R820T_CHECK_VAL		0x69

#define R820T_IF_FREQ		3570000

#define VERSION				"R820T_v1.49_ASTRO"
#define VER_NUM				49

#define USE_16M_XTAL		FALSE
#define R828_Xtal			28800

#define USE_DIPLEXER		FALSE
#define TUNER_CLK_OUT		TRUE

#define DIP_FREQ			320000
#define IMR_TRIAL			9
#define VCO_pwr_ref			0x02

#define FUNCTION_SUCCESS	0
#define FUNCTION_ERROR		-1

typedef enum _Rafael_Chip_Type  //Don't modify chip list
{
	R828 = 0,
	R828D,
	R828S,
	R820T,
	R820C,
	R620D,
	R620S
}Rafael_Chip_Type;

static const UINT8 Rafael_Chip = R820T;	// FIXME: Move to tuner class

enum XTAL_CAP_VALUE
{
	XTAL_LOW_CAP_30P = 0,
	XTAL_LOW_CAP_20P,
	XTAL_LOW_CAP_10P,
	XTAL_LOW_CAP_0P,
	XTAL_HIGH_CAP_0P
};

///////////////////////////////////////////////////////////////////////////////

namespace RTL2832_NAMESPACE { namespace TUNERS_NAMESPACE {

int r820t::TUNER_PROBE_FN_NAME(demod* d)
{
	I2C_REPEATER_SCOPE(d);
	
	d->set_gpio_output(5);	// initialise GPIOs
	d->set_gpio_bit(5, 1);	// reset tuner before probing
	d->set_gpio_bit(5, 0);
	
	uint8_t reg = 0;
	//CHECK_LIBUSB_RESULT_RETURN_EX(d,d->i2c_read_reg(R820T_I2C_ADDR, R820T_CHECK_ADDR, reg));
	int r = d->i2c_read_reg(R820T_I2C_ADDR, R820T_CHECK_ADDR, reg);
	if (r <= 0) return r;
	return ((reg == R820T_CHECK_VAL) ? SUCCESS : FAILURE);
}

r820t::r820t(demod* p)
	: tuner_skeleton(p)
	//, Rafael_Chip(R820T)
	, R828_IF_khz(0)
	, R828_CAL_LO_khz(0)
	, R828_IMR_point_num(0)
	, R828_IMR_done_flag(FALSE)
	, Xtal_cap_sel(XTAL_LOW_CAP_0P)
	, Xtal_cap_sel_tmp(XTAL_LOW_CAP_0P)
{
	memset(&R828_I2C, 0x00, sizeof(R828_I2C));
	memset(&R828_I2C_Len, 0x00, sizeof(R828_I2C_Len));
	memset(&IMR_Data, 0x00, sizeof(IMR_Data));
	memset(&R828_Arry, 0x00, sizeof(R828_Arry));
	memset(&R828_Fil_Cal_flag, 0x00, sizeof(R828_Fil_Cal_flag));
	memset(&R828_Fil_Cal_code, 0x00, sizeof(R828_Fil_Cal_code));
	memset(&SysFreq_Info1, 0x00, sizeof(SysFreq_Info1));
	memset(&Sys_Info1, 0x00, sizeof(Sys_Info1));
	//memset(&R828_Freq_Info, 0x00, sizeof(R828_Freq_Info));
	memset(&Freq_Info1, 0x00, sizeof(Freq_Info1));
	
//	m_frequency_range = range_t(24e6, 1766e6);
	
	m_gain_range = std::make_pair(0.0, 49.6);
	// FIXME: Set m_gain_values to achieve finer gain steps
	
	m_bandwidth_values.push_back(6000000);
//	m_bandwidth_values.push_back(7000000);
//	m_bandwidth_values.push_back(8000000);
//	values_to_range(m_bandwidth_values, m_bandwidth_range);

	m_bandwidth = m_bandwidth_range.first;	// Default
}

int r820t::initialise(tuner::PPARAMS params /*= NULL*/)
{
	if (tuner_skeleton::initialise(params) != SUCCESS)
		return FAILURE;
	
	THIS_I2C_REPEATER_SCOPE();
	
	if (R828_Init(this) != FUNCTION_SUCCESS)
		return FAILURE;

	if (r820t_SetStandardMode(this, DVB_T_6M) != FUNCTION_SUCCESS)
		return FAILURE;

	if (R828_RfGainMode(this, RF_MANUAL) != RT_Success)
		return FAILURE;

	parent()->set_if(R820T_IF_FREQ);
	
	if (m_params.message_output && m_params.verbose)
		m_params.message_output->on_log_message_ex(rtl2832::log_sink::LOG_LEVEL_VERBOSE, LOG_PREFIX"Initialised (default bandwidth: %i Hz)\n", (uint32_t)bandwidth());
	
	return SUCCESS;
}

// FIXME: r820t_SetStandby(this, SIGLE_IN)

int r820t::set_frequency(double freq)
{
	if ((freq <= 0) || (in_valid_range(m_frequency_range, freq) == false))
		return FAILURE;
	
	THIS_I2C_REPEATER_SCOPE();
	
	if (r820t_SetRfFreqHz(this, (unsigned long)freq) != FUNCTION_SUCCESS)
		return FAILURE;
	
	m_freq = (((unsigned long)freq + 500) / 1000) * 1000;
	
	return SUCCESS;
}

int r820t::set_bandwidth(double bw)
{
	// FIXME: R828_SetFrequency
	//	R828_Filt_Cal BW I2C writes commented out
	//	R828_Filt_Cal BW argument set by Sys_Info.BW
	//	SysInfo1.BW determined by StandardType
	//	Set via r820t_SetStandardMode
	
	return /*FAILURE*/SUCCESS;
}

int r820t::set_gain(double gain)
{
	if (in_range(m_gain_range, gain) == false)
		return FAILURE;
	
	THIS_I2C_REPEATER_SCOPE();
	
	int iGain = (int)(gain * 10.0);
	
	if (R828_SetRfGain(this, iGain) != FUNCTION_SUCCESS)
		return FAILURE;

	m_gain = gain;
	
	return SUCCESS;
}

} } // TUNERS_NAMESPACE, RTL2832_NAMESPACE

int r820t_SetRfFreqHz(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, unsigned long RfFreqHz)
{
	R828_Set_Info R828Info;
	
	//if(pExtra->IsStandardModeSet==NO)
	//	goto error_status_set_tuner_rf_frequency;
	
	//R828Info.R828_Standard = (R828_Standard_Type)pExtra->StandardMode;
	R828Info.R828_Standard = (R828_Standard_Type)DVB_T_6M;
	R828Info.RF_Hz = (UINT32)(RfFreqHz);
	R828Info.RF_KHz = (UINT32)(RfFreqHz/1000);
	
	if(R828_SetFrequency(pTuner, R828Info, NORMAL_MODE) != RT_Success)
		return FUNCTION_ERROR;
	
	return FUNCTION_SUCCESS;
}

int r820t_SetStandardMode(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int StandardMode)
{
	if(R828_SetStandard(pTuner, (R828_Standard_Type)StandardMode) != RT_Success)
		return FUNCTION_ERROR;
	
	return FUNCTION_SUCCESS;
}

int r820t_SetStandby(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int LoopThroughType)
{
	if(R828_Standby(pTuner, (R828_LoopThrough_Type)LoopThroughType) != RT_Success)
		return FUNCTION_ERROR;
	
	return FUNCTION_SUCCESS;
}

/* just reverses the bits of a byte */
static int
r820t_Convert(int InvertNum)
{
	int ReturnNum;
	int AddNum;
	int BitNum;
	int CountNum;
	
	ReturnNum = 0;
	AddNum    = 0x80;
	BitNum    = 0x01;
	
	for(CountNum = 0;CountNum < 8;CountNum ++)
	{
		if(BitNum & InvertNum)
			ReturnNum += AddNum;
		
		AddNum /= 2;
		BitNum *= 2;
	}
	
	return ReturnNum;
}

static R828_ErrCode
_I2C_Write_Len(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_I2C_LEN_TYPE *I2C_Info,
			  const char* function = NULL, int line_number = -1, const char* line = NULL)
{
//	unsigned char DeviceAddr;
	
	unsigned int i, j;
	
	unsigned char RegStartAddr;
	unsigned char *pWritingBytes;
	unsigned long ByteNum;
	
	unsigned char WritingBuffer[128];
	unsigned long WritingByteNum, WritingByteNumMax, WritingByteNumRem;
	unsigned char RegWritingAddr;
	
	// Get regiser start address, writing bytes, and byte number.
	RegStartAddr  = I2C_Info->RegAddr;
	pWritingBytes = I2C_Info->Data;
	ByteNum       = (unsigned long)I2C_Info->Len;
	
	// Calculate maximum writing byte number.
	//	WritingByteNumMax = pBaseInterface->I2cWritingByteNumMax - LEN_1_BYTE;
	WritingByteNumMax = 2 - 1; //9 orig
	
	// Set tuner register bytes with writing bytes.
	// Note: Set tuner register bytes considering maximum writing byte number.
	for(i = 0; i < ByteNum; i += WritingByteNumMax)
	{
		// Set register writing address.
		RegWritingAddr = RegStartAddr + i;
		
		// Calculate remainder writing byte number.
		WritingByteNumRem = ByteNum - i;
		
		// Determine writing byte number.
		WritingByteNum = (WritingByteNumRem > WritingByteNumMax) ? WritingByteNumMax : WritingByteNumRem;
		
		// Set writing buffer.
		// Note: The I2C format of tuner register byte setting is as follows:
		//       start_bit + (DeviceAddr | writing_bit) + RegWritingAddr + writing_bytes (WritingByteNum bytes) +
		//       stop_bit
		WritingBuffer[0] = RegWritingAddr;
		
		for(j = 0; j < WritingByteNum; j++)
			WritingBuffer[j+1] = pWritingBytes[i + j];
		
		// Set tuner register bytes with writing buffer.
		//		if(pI2cBridge->ForwardI2cWritingCmd(pI2cBridge, DeviceAddr, WritingBuffer, WritingByteNum + LEN_1_BYTE) !=
		//			FUNCTION_SUCCESS)
		//			goto error_status_set_tuner_registers;
		
		int r = pTuner->i2c_write(R820T_I2C_ADDR, WritingBuffer, WritingByteNum + 1);
		if (r < 0)
		{
			DEBUG_TUNER_I2C(pTuner,r);
			return RT_Fail;
		}
	}
	
	return RT_Success;
}

static R828_ErrCode
_I2C_Read_Len(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_I2C_LEN_TYPE *I2C_Info,
			  const char* function = NULL, int line_number = -1, const char* line = NULL)
{
//	uint8_t DeviceAddr;
	
	unsigned int i;
	
	uint8_t RegStartAddr;
	uint8_t ReadingBytes[128];
	unsigned long ByteNum;
	
	// Get regiser start address, writing bytes, and byte number.
	RegStartAddr  = 0x00;
	ByteNum       = (unsigned long)I2C_Info->Len;
	
	// Set tuner register reading address.
	// Note: The I2C format of tuner register reading address setting is as follows:
	//       start_bit + (DeviceAddr | writing_bit) + RegReadingAddr + stop_bit
	//	if(pI2cBridge->ForwardI2cWritingCmd(pI2cBridge, DeviceAddr, &RegStartAddr, LEN_1_BYTE) != FUNCTION_SUCCESS)
	//		goto error_status_set_tuner_register_reading_address;
	
	int r = pTuner->i2c_write(R820T_I2C_ADDR, &RegStartAddr, 1);
	if (r < 0)
	{
		DEBUG_TUNER_I2C(pTuner,r);
		return RT_Fail;
	}
	
	// Get tuner register bytes.
	// Note: The I2C format of tuner register byte getting is as follows:
	//       start_bit + (DeviceAddr | reading_bit) + reading_bytes (ReadingByteNum bytes) + stop_bit
	//	if(pI2cBridge->ForwardI2cReadingCmd(pI2cBridge, DeviceAddr, ReadingBytes, ByteNum) != FUNCTION_SUCCESS)
	//		goto error_status_get_tuner_registers;
	
	r = pTuner->i2c_read(R820T_I2C_ADDR, ReadingBytes, ByteNum);
	if (r < 0)
	{
		DEBUG_TUNER_I2C(pTuner,r);
		return RT_Fail;
	}
	
	for(i = 0; i<ByteNum; i++)
		I2C_Info->Data[i] = (UINT8)r820t_Convert(ReadingBytes[i]);
	
	return RT_Success;
//error_status_get_tuner_registers:
//error_status_set_tuner_register_reading_address:
	return RT_Fail;
}

static R828_ErrCode
_I2C_Write(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_I2C_TYPE *I2C_Info,
		   const char* function = NULL, int line_number = -1, const char* line = NULL)
{
	uint8_t WritingBuffer[2];
	
	// Set writing bytes.
	// Note: The I2C format of tuner register byte setting is as follows:
	//       start_bit + (DeviceAddr | writing_bit) + addr + data + stop_bit
	WritingBuffer[0] = I2C_Info->RegAddr;
	WritingBuffer[1] = I2C_Info->Data;
	
	// Set tuner register bytes with writing buffer.
	//if(pI2cBridge->ForwardI2cWritingCmd(pI2cBridge, DeviceAddr, WritingBuffer, LEN_2_BYTE) != FUNCTION_SUCCESS)
	//	goto error_status_set_tuner_registers;
	
	//printf("called %s: %02x -> %02x\n", __FUNCTION__, WritingBuffer[0], WritingBuffer[1]);
	
	int r = pTuner->i2c_write(R820T_I2C_ADDR, WritingBuffer, 2);
	if (r < 0)
	{
		DEBUG_TUNER_I2C(pTuner,r);
		return RT_Fail;
	}
	
	return RT_Success;
}

#define I2C_Write_Len(t,r)	_I2C_Write_Len(t,r,CURRENT_FUNCTION,__LINE__,"I2C_Write_Len("#t", "#r")")
#define I2C_Read_Len(t,r)	_I2C_Read_Len(t,r,CURRENT_FUNCTION,__LINE__,"I2C_Read_Len("#t", "#r")")
#define I2C_Write(t,r)		_I2C_Write(t,r,CURRENT_FUNCTION,__LINE__,"I2C_Write("#t", "#r")")

////////////////////////////////////////////////////////////////////////////////

static inline void
R828_Delay_MS(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, unsigned long WaitTimeMs)
{
	/* simply don't wait for now */
	return;
}

#if(TUNER_CLK_OUT==TRUE)  //enable tuner clk output for share Xtal application
static const UINT8 R828_iniArry[27] = {0x83, 0x32, 0x75, 0xC0, 0x40, 0xD6, 0x6C, 0xF5, 0x63,
	/*     0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D                                                    */
	
	0x75, 0x68, 0x6C, 0x83, 0x80, 0x00, 0x0F, 0x00, 0xC0,//xtal_check
	/*     0x0E  0x0F  0x10  0x11  0x12  0x13  0x14  0x15  0x16                                                    */
	
	0x30, 0x48, 0xCC, 0x60, 0x00, 0x54, 0xAE, 0x4A, 0xC0};
/*     0x17  0x18  0x19  0x1A  0x1B  0x1C  0x1D  0x1E  0x1F                                                    */
#else
static const UINT8 R828_iniArry[27] = {0x83, 0x32, 0x75, 0xC0, 0x40, 0xD6, 0x6C, 0xF5, 0x63,
	/*     0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D                                                    */
	
	0x75, 0x78, 0x6C, 0x83, 0x80, 0x00, 0x0F, 0x00, 0xC0,//xtal_check
	/*     0x0E  0x0F  0x10  0x11  0x12  0x13  0x14  0x15  0x16                                                    */
	
	0x30, 0x48, 0xCC, 0x60, 0x00, 0x54, 0xAE, 0x4A, 0xC0};
/*     0x17  0x18  0x19  0x1A  0x1B  0x1C  0x1D  0x1E  0x1F                                                    */
#endif

//----------------------------------------------------------//
//                   Internal Functions                     //
//----------------------------------------------------------//

static R828_ErrCode R828_Xtal_Check(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner);
static R828_ErrCode R828_InitReg(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner);
static R828_ErrCode R828_IMR_Prepare(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner);
static R828_ErrCode R828_IMR(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT8 IMR_MEM, int IM_Flag);
static R828_ErrCode R828_PLL(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT32 LO_Freq, R828_Standard_Type R828_Standard);
static R828_ErrCode R828_MUX(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT32 RF_KHz);
static R828_ErrCode R828_IQ(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* IQ_Pont);
static R828_ErrCode R828_IQ_Tree(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT8 FixPot, UINT8 FlucPot, UINT8 PotReg, R828_SectType* CompareTree);
static R828_ErrCode R828_CompreCor(R828_SectType* CorArry);
static R828_ErrCode R828_CompreStep(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* StepArry, UINT8 Pace);
static R828_ErrCode R828_Muti_Read(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT8 IMR_Reg, UINT16* IMR_Result_Data);
static R828_ErrCode R828_Section(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* SectionArry);
static R828_ErrCode R828_F_IMR(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* IQ_Pont);
static R828_ErrCode R828_IMR_Cross(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* IQ_Pont, UINT8* X_Direct);

static Sys_Info_Type R828_Sys_Sel(R828_Standard_Type R828_Standard);
static Freq_Info_Type R828_Freq_Sel(UINT32 RF_freq);
static SysFreq_Info_Type R828_SysFreq_Sel(R828_Standard_Type R828_Standard,UINT32 RF_freq);

static R828_ErrCode R828_Filt_Cal(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT32 Cal_Freq,BW_Type R828_BW);
//static R828_ErrCode R828_SetFrequency(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_Set_Info R828_INFO, R828_SetFreq_Type R828_SetFreqMode);

static Sys_Info_Type R828_Sys_Sel(R828_Standard_Type R828_Standard)
{
	Sys_Info_Type R828_Sys_Info;
	
	switch (R828_Standard)
	{
			
		case DVB_T_6M:
		case DVB_T2_6M:
			R828_Sys_Info.IF_KHz=3570;
			R828_Sys_Info.BW=BW_6M;
			R828_Sys_Info.FILT_CAL_LO=56000; //52000->56000
			R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
			R828_Sys_Info.IMG_R=0x00;		//image negative
			R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
			R828_Sys_Info.HP_COR=0x6B;		// 1.7M disable, +2cap, 1.0MHz
			R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1
			R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
			R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
			R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
			R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
			break;
			
		case DVB_T_7M:
		case DVB_T2_7M:
			R828_Sys_Info.IF_KHz=4070;
			R828_Sys_Info.BW=BW_7M;
			R828_Sys_Info.FILT_CAL_LO=60000;
			R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
			R828_Sys_Info.IMG_R=0x00;		//image negative
			R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
			R828_Sys_Info.HP_COR=0x2B;		// 1.7M disable, +1cap, 1.0MHz
			R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1
			R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
			R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
			R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
			R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
			break;
			
		case DVB_T_7M_2:
		case DVB_T2_7M_2:
			R828_Sys_Info.IF_KHz=4570;
			R828_Sys_Info.BW=BW_7M;
			R828_Sys_Info.FILT_CAL_LO=63000;
			R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
			R828_Sys_Info.IMG_R=0x00;		//image negative
			R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
			R828_Sys_Info.HP_COR=0x2A;		// 1.7M disable, +1cap, 1.25MHz
			R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1
			R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
			R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
			R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
			R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
			break;
			
		case DVB_T_8M:
		case DVB_T2_8M:
			R828_Sys_Info.IF_KHz=4570;
			R828_Sys_Info.BW=BW_8M;
			R828_Sys_Info.FILT_CAL_LO=68500;
			R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
			R828_Sys_Info.IMG_R=0x00;		//image negative
			R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
			R828_Sys_Info.HP_COR=0x0B;		// 1.7M disable, +0cap, 1.0MHz
			R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1
			R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
			R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
			R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
			R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
			break;
			
		case ISDB_T:
			R828_Sys_Info.IF_KHz=4063;
			R828_Sys_Info.BW=BW_6M;
			R828_Sys_Info.FILT_CAL_LO=59000;
			R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
			R828_Sys_Info.IMG_R=0x00;		//image negative
			R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
			R828_Sys_Info.HP_COR=0x6A;		// 1.7M disable, +2cap, 1.25MHz
			R828_Sys_Info.EXT_ENABLE=0x40;  //R30[6], ext enable; R30[5]:0 ext at LNA max
			R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
			R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
			R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
			R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
			break;
			
		default:  //DVB_T_8M
			R828_Sys_Info.IF_KHz=4570;
			R828_Sys_Info.BW=BW_8M;
			R828_Sys_Info.FILT_CAL_LO=68500;
			R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
			R828_Sys_Info.IMG_R=0x00;		//image negative
			R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
			R828_Sys_Info.HP_COR=0x0D;		// 1.7M disable, +0cap, 0.7MHz
			R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1
			R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
			R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
			R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
			R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
			break;
			
	}
	
	return R828_Sys_Info;
}

static Freq_Info_Type R828_Freq_Sel(UINT32 LO_freq)
{
	Freq_Info_Type R828_Freq_Info;
	
	if(LO_freq<50000)
	{
		R828_Freq_Info.OPEN_D=0x08; // low
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0xDF;     //R27[7:0]  band2,band0
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	
	else if(LO_freq>=50000 && LO_freq<55000)
	{
		R828_Freq_Info.OPEN_D=0x08; // low
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0xBE;     //R27[7:0]  band4,band1
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=55000 && LO_freq<60000)
	{
		R828_Freq_Info.OPEN_D=0x08; // low
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x8B;     //R27[7:0]  band7,band4
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=60000 && LO_freq<65000)
	{
		R828_Freq_Info.OPEN_D=0x08; // low
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x7B;     //R27[7:0]  band8,band4
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=65000 && LO_freq<70000)
	{
		R828_Freq_Info.OPEN_D=0x08; // low
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x69;     //R27[7:0]  band9,band6
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=70000 && LO_freq<75000)
	{
		R828_Freq_Info.OPEN_D=0x08; // low
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x58;     //R27[7:0]  band10,band7
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=75000 && LO_freq<80000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x44;     //R27[7:0]  band11,band11
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=80000 && LO_freq<90000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x44;     //R27[7:0]  band11,band11
		R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=90000 && LO_freq<100000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x34;     //R27[7:0]  band12,band11
		R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=100000 && LO_freq<110000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x34;     //R27[7:0]  band12,band11
		R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 0;
	}
	else if( LO_freq>=110000 && LO_freq<120000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x24;     //R27[7:0]  band13,band11
		R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 1;
	}
	else if( LO_freq>=120000 && LO_freq<140000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x24;     //R27[7:0]  band13,band11
		R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 1;
	}
	else if( LO_freq>=140000 && LO_freq<180000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x14;     //R27[7:0]  band14,band11
		R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)
		R828_Freq_Info.XTAL_CAP10P=0x01;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 1;
	}
	else if( LO_freq>=180000 && LO_freq<220000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x13;     //R27[7:0]  band14,band12
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 1;
	}
	else if( LO_freq>=220000 && LO_freq<250000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x13;     //R27[7:0]  band14,band12
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 2;
	}
	else if( LO_freq>=250000 && LO_freq<280000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x11;     //R27[7:0]  highest,highest
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 2;
	}
	else if( LO_freq>=280000 && LO_freq<310000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
		R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 2;
	}
	else if( LO_freq>=310000 && LO_freq<450000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x41;  //R26[7:6]=1 (bypass)  R26[1:0]=1 (middle)
		R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 2;
	}
	else if( LO_freq>=450000 && LO_freq<588000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x41;  //R26[7:6]=1 (bypass)  R26[1:0]=1 (middle)
		R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 3;
	}
	else if( LO_freq>=588000 && LO_freq<650000)
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x40;  //R26[7:6]=1 (bypass)  R26[1:0]=0 (highest)
		R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 3;
	}
	else
	{
		R828_Freq_Info.OPEN_D=0x00; // high
		R828_Freq_Info.RF_MUX_PLOY = 0x40;  //R26[7:6]=1 (bypass)  R26[1:0]=0 (highest)
		R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
		R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)
		R828_Freq_Info.XTAL_CAP10P=0x00;
		R828_Freq_Info.XTAL_CAP0P=0x00;
		R828_Freq_Info.IMR_MEM = 4;
	}
	
	return R828_Freq_Info;
}

static SysFreq_Info_Type R828_SysFreq_Sel(R828_Standard_Type R828_Standard,UINT32 RF_freq)
{
	SysFreq_Info_Type R828_SysFreq_Info;
	
	switch(R828_Standard)
	{
			
		case DVB_T_6M:
		case DVB_T_7M:
		case DVB_T_7M_2:
		case DVB_T_8M:
			if( (RF_freq==506000) || (RF_freq==666000) || (RF_freq==818000) )
			{
				R828_SysFreq_Info.MIXER_TOP=0x14;	    // MIXER TOP:14 , TOP-1, low-discharge
				R828_SysFreq_Info.LNA_TOP=0xE5;		    // Detect BW 3, LNA TOP:4, PreDet Top:2
				R828_SysFreq_Info.CP_CUR=0x28;            //101, 0.2
				R828_SysFreq_Info.DIV_BUF_CUR=0x20; // 10, 200u
			}
			else
			{
				R828_SysFreq_Info.MIXER_TOP=0x24;	    // MIXER TOP:13 , TOP-1, low-discharge
				R828_SysFreq_Info.LNA_TOP=0xE5;		// Detect BW 3, LNA TOP:4, PreDet Top:2
				R828_SysFreq_Info.CP_CUR=0x38;            // 111, auto
				R828_SysFreq_Info.DIV_BUF_CUR=0x30; // 11, 150u
			}
			R828_SysFreq_Info.LNA_VTH_L=0x53;		    // LNA VTH 0.84	,  VTL 0.64
			R828_SysFreq_Info.MIXER_VTH_L=0x75;	// MIXER VTH 1.04, VTL 0.84
			R828_SysFreq_Info.AIR_CABLE1_IN=0x00;
			R828_SysFreq_Info.CABLE2_IN=0x00;
			R828_SysFreq_Info.PRE_DECT=0x40;
			R828_SysFreq_Info.LNA_DISCHARGE=14;
			R828_SysFreq_Info.FILTER_CUR=0x40;         // 10, low
			break;
			
			
		case DVB_T2_6M:
		case DVB_T2_7M:
		case DVB_T2_7M_2:
		case DVB_T2_8M:
			R828_SysFreq_Info.MIXER_TOP=0x24;	    // MIXER TOP:13 , TOP-1, low-discharge
			R828_SysFreq_Info.LNA_TOP=0xE5;		    // Detect BW 3, LNA TOP:4, PreDet Top:2
			R828_SysFreq_Info.LNA_VTH_L=0x53;		// LNA VTH 0.84	,  VTL 0.64
			R828_SysFreq_Info.MIXER_VTH_L=0x75;	// MIXER VTH 1.04, VTL 0.84
			R828_SysFreq_Info.AIR_CABLE1_IN=0x00;
			R828_SysFreq_Info.CABLE2_IN=0x00;
			R828_SysFreq_Info.PRE_DECT=0x40;
			R828_SysFreq_Info.LNA_DISCHARGE=14;
			R828_SysFreq_Info.CP_CUR=0x38;            // 111, auto
			R828_SysFreq_Info.DIV_BUF_CUR=0x30; // 11, 150u
			R828_SysFreq_Info.FILTER_CUR=0x40;    // 10, low
			break;
			
		case ISDB_T:
			R828_SysFreq_Info.MIXER_TOP=0x24;	// MIXER TOP:13 , TOP-1, low-discharge
			R828_SysFreq_Info.LNA_TOP=0xE5;		// Detect BW 3, LNA TOP:4, PreDet Top:2
			R828_SysFreq_Info.LNA_VTH_L=0x75;		// LNA VTH 1.04	,  VTL 0.84
			R828_SysFreq_Info.MIXER_VTH_L=0x75;	// MIXER VTH 1.04, VTL 0.84
			R828_SysFreq_Info.AIR_CABLE1_IN=0x00;
			R828_SysFreq_Info.CABLE2_IN=0x00;
			R828_SysFreq_Info.PRE_DECT=0x40;
			R828_SysFreq_Info.LNA_DISCHARGE=14;
			R828_SysFreq_Info.CP_CUR=0x38;            // 111, auto
			R828_SysFreq_Info.DIV_BUF_CUR=0x30; // 11, 150u
			R828_SysFreq_Info.FILTER_CUR=0x40;    // 10, low
			break;
			
		default: //DVB-T 8M
			R828_SysFreq_Info.MIXER_TOP=0x24;	    // MIXER TOP:13 , TOP-1, low-discharge
			R828_SysFreq_Info.LNA_TOP=0xE5;		// Detect BW 3, LNA TOP:4, PreDet Top:2
			R828_SysFreq_Info.LNA_VTH_L=0x53;		// LNA VTH 0.84	,  VTL 0.64
			R828_SysFreq_Info.MIXER_VTH_L=0x75;	// MIXER VTH 1.04, VTL 0.84
			R828_SysFreq_Info.AIR_CABLE1_IN=0x00;
			R828_SysFreq_Info.CABLE2_IN=0x00;
			R828_SysFreq_Info.PRE_DECT=0x40;
			R828_SysFreq_Info.LNA_DISCHARGE=14;
			R828_SysFreq_Info.CP_CUR=0x38;            // 111, auto
			R828_SysFreq_Info.DIV_BUF_CUR=0x30; // 11, 150u
			R828_SysFreq_Info.FILTER_CUR=0x40;    // 10, low
			break;
			
	} //end switch
	
	//DTV use Diplexer
#if(USE_DIPLEXER==TRUE)
	if ((Rafael_Chip==R820C) || (Rafael_Chip==R820T) || (Rafael_Chip==R828S))
	{
		// Air-in (>=DIP_FREQ) & cable-1(<DIP_FREQ)
		if(RF_freq >= DIP_FREQ)
		{
			R828_SysFreq_Info.AIR_CABLE1_IN = 0x00; //air in, cable-1 off
			R828_SysFreq_Info.CABLE2_IN = 0x00;     //cable-2 off
		}
		else
		{
			R828_SysFreq_Info.AIR_CABLE1_IN = 0x60; //cable-1 in, air off
			R828_SysFreq_Info.CABLE2_IN = 0x00;     //cable-2 off
		}
	}
#endif
	return R828_SysFreq_Info;
	
}

static R828_ErrCode R828_Xtal_Check(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner)
{
	UINT8 ArrayNum;
	
	ArrayNum = 27;
	for(ArrayNum=0;ArrayNum<27;ArrayNum++)
	{
		pTuner->R828_Arry[ArrayNum] = R828_iniArry[ArrayNum];
	}
	
	//cap 30pF & Drive Low
	pTuner->R828_I2C.RegAddr = 0x10;
	pTuner->R828_Arry[11]    = (pTuner->R828_Arry[11] & 0xF4) | 0x0B ;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[11];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
	    return RT_Fail;
	
	//set pll autotune = 128kHz
	pTuner->R828_I2C.RegAddr = 0x1A;
	pTuner->R828_Arry[21]    = pTuner->R828_Arry[21] & 0xF3;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[21];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//set manual initial reg = 111111;
	pTuner->R828_I2C.RegAddr = 0x13;
	pTuner->R828_Arry[14]    = (pTuner->R828_Arry[14] & 0x80) | 0x7F;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[14];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//set auto
	pTuner->R828_I2C.RegAddr = 0x13;
	pTuner->R828_Arry[14]    = (pTuner->R828_Arry[14] & 0xBF);
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[14];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	R828_Delay_MS(pTuner, 5);
	
	pTuner->R828_I2C_Len.RegAddr = 0x00;
	pTuner->R828_I2C_Len.Len     = 3;
	if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
		return RT_Fail;
	
	// if 30pF unlock, set to cap 20pF
#if (USE_16M_XTAL==TRUE)
	//VCO=2360MHz for 16M Xtal. VCO band 26
    if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) > 29) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) < 23))
#else
		if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) == 0x3F))
#endif
		{
			//cap 20pF
			pTuner->R828_I2C.RegAddr = 0x10;
			pTuner->R828_Arry[11]    = (pTuner->R828_Arry[11] & 0xFC) | 0x02;
			pTuner->R828_I2C.Data    = pTuner->R828_Arry[11];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			R828_Delay_MS(pTuner, 5);
			
			pTuner->R828_I2C_Len.RegAddr = 0x00;
			pTuner->R828_I2C_Len.Len     = 3;
			if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
				return RT_Fail;
			
			// if 20pF unlock, set to cap 10pF
#if (USE_16M_XTAL==TRUE)
			if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) > 29) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) < 23))
#else
				if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) == 0x3F))
#endif
				{
					//cap 10pF
					pTuner->R828_I2C.RegAddr = 0x10;
					pTuner->R828_Arry[11]    = (pTuner->R828_Arry[11] & 0xFC) | 0x01;
					pTuner->R828_I2C.Data    = pTuner->R828_Arry[11];
					if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
						return RT_Fail;
					
					R828_Delay_MS(pTuner, 5);
					
					pTuner->R828_I2C_Len.RegAddr = 0x00;
					pTuner->R828_I2C_Len.Len     = 3;
					if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
						return RT_Fail;
					
					// if 10pF unlock, set to cap 0pF
#if (USE_16M_XTAL==TRUE)
					if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) > 29) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) < 23))
#else
						if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) == 0x3F))
#endif
						{
							//cap 0pF
							pTuner->R828_I2C.RegAddr = 0x10;
							pTuner->R828_Arry[11]    = (pTuner->R828_Arry[11] & 0xFC) | 0x00;
							pTuner->R828_I2C.Data    = pTuner->R828_Arry[11];
							if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
								return RT_Fail;
							
							R828_Delay_MS(pTuner, 5);
							
							pTuner->R828_I2C_Len.RegAddr = 0x00;
							pTuner->R828_I2C_Len.Len     = 3;
							if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
								return RT_Fail;
							
							// if unlock, set to high drive
#if (USE_16M_XTAL==TRUE)
							if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) > 29) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) < 23))
#else
								if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) == 0x3F))
#endif
								{
									//X'tal drive high
									pTuner->R828_I2C.RegAddr = 0x10;
									pTuner->R828_Arry[11]    = (pTuner->R828_Arry[11] & 0xF7) ;
									pTuner->R828_I2C.Data    = pTuner->R828_Arry[11];
									if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
										return RT_Fail;
									
									//R828_Delay_MS(15);
									R828_Delay_MS(pTuner, 20);
									
									pTuner->R828_I2C_Len.RegAddr = 0x00;
									pTuner->R828_I2C_Len.Len     = 3;
									if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
										return RT_Fail;
									
#if (USE_16M_XTAL==TRUE)
									if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) > 29) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) < 23))
#else
										if(((pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((pTuner->R828_I2C_Len.Data[2] & 0x3F) == 0x3F))
#endif
										{
											return RT_Fail;
										}
										else //0p+high drive lock
										{
											pTuner->Xtal_cap_sel_tmp = XTAL_HIGH_CAP_0P;
										}
								}
								else //0p lock
								{
									pTuner->Xtal_cap_sel_tmp = XTAL_LOW_CAP_0P;
								}
						}
						else //10p lock
						{
							pTuner->Xtal_cap_sel_tmp = XTAL_LOW_CAP_10P;
						}
				}
				else //20p lock
				{
					pTuner->Xtal_cap_sel_tmp = XTAL_LOW_CAP_20P;
				}
		}
		else // 30p lock
		{
			pTuner->Xtal_cap_sel_tmp = XTAL_LOW_CAP_30P;
		}
	
    return RT_Success;
}

R828_ErrCode R828_Init(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner)
{
	//	R820T_EXTRA_MODULE *pExtra;
    UINT8 i;
	
	// Get tuner extra module.
	//	pExtra = &(pTuner->Extra.R820t);
	
    //write initial reg
	//if(R828_InitReg(pTuner) != RT_Success)
	//	return RT_Fail;
	
	if(pTuner->R828_IMR_done_flag==FALSE)
	{
		
		//write initial reg
		//	  if(R828_InitReg(pTuner) != RT_Success)
		//		  return RT_Fail;
		
		//Do Xtal check
		if((Rafael_Chip==R820T) || (Rafael_Chip==R828S) || (Rafael_Chip==R820C))
		{
			pTuner->Xtal_cap_sel = XTAL_HIGH_CAP_0P;
		}
		else
		{
			if(R828_Xtal_Check(pTuner) != RT_Success)        //1st
				return RT_Fail;
			
			pTuner->Xtal_cap_sel = pTuner->Xtal_cap_sel_tmp;
			
			if(R828_Xtal_Check(pTuner) != RT_Success)        //2nd
				return RT_Fail;
			
			if(pTuner->Xtal_cap_sel_tmp > pTuner->Xtal_cap_sel)
			{
				pTuner->Xtal_cap_sel = pTuner->Xtal_cap_sel_tmp;
			}
			
			if(R828_Xtal_Check(pTuner) != RT_Success)        //3rd
				return RT_Fail;
			
			if(pTuner->Xtal_cap_sel_tmp > pTuner->Xtal_cap_sel)
			{
				pTuner->Xtal_cap_sel = pTuner->Xtal_cap_sel_tmp;
			}
			
		}
		
		//reset filter cal.
		for (i=0; i<STD_SIZE; i++)
		{
			pTuner->R828_Fil_Cal_flag[i] = FALSE;
			pTuner->R828_Fil_Cal_code[i] = 0;
		}
		
#if 0
		//start imr cal.
		if(R828_InitReg(pTuner) != RT_Success)        //write initial reg before doing cal
			return RT_Fail;
		
		if(R828_IMR_Prepare(pTuner) != RT_Success)
			return RT_Fail;
		
		if(R828_IMR(pTuner, 3, TRUE) != RT_Success)       //Full K node 3
			return RT_Fail;
		
		if(R828_IMR(pTuner, 1, FALSE) != RT_Success)
			return RT_Fail;
		
		if(R828_IMR(pTuner, 0, FALSE) != RT_Success)
			return RT_Fail;
		
		if(R828_IMR(pTuner, 2, FALSE) != RT_Success)
			return RT_Fail;
		
		if(R828_IMR(pTuner, 4, FALSE) != RT_Success)
			return RT_Fail;
		
		pTuner->R828_IMR_done_flag = TRUE;
#endif
	}
	
	//write initial reg
	if(R828_InitReg(pTuner) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

static R828_ErrCode R828_InitReg(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner)
{
	UINT8 InitArryCount;
	UINT8 InitArryNum;
	
	InitArryCount = 0;
	InitArryNum  = 27;
	
	//UINT32 LO_KHz      = 0;
	
	//Write Full Table
	pTuner->R828_I2C_Len.RegAddr = 0x05;
	pTuner->R828_I2C_Len.Len     = InitArryNum;
	for(InitArryCount = 0;InitArryCount < InitArryNum;InitArryCount ++)
	{
		pTuner->R828_I2C_Len.Data[InitArryCount] = R828_iniArry[InitArryCount];
	}
	if(I2C_Write_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

static R828_ErrCode R828_IMR_Prepare(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner)
{
	UINT8 ArrayNum;
	
	ArrayNum=27;
	
	for(ArrayNum=0;ArrayNum<27;ArrayNum++)
	{
		pTuner->R828_Arry[ArrayNum] = R828_iniArry[ArrayNum];
	}
	//IMR Preparation
	//lna off (air-in off)
	pTuner->R828_I2C.RegAddr = 0x05;
	pTuner->R828_Arry[0]     = pTuner->R828_Arry[0]  | 0x20;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[0];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//mixer gain mode = manual
	pTuner->R828_I2C.RegAddr = 0x07;
	pTuner->R828_Arry[2]     = (pTuner->R828_Arry[2] & 0xEF);
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[2];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//filter corner = lowest
	pTuner->R828_I2C.RegAddr = 0x0A;
	pTuner->R828_Arry[5]     = pTuner->R828_Arry[5] | 0x0F;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[5];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//filter bw=+2cap, hp=5M
	pTuner->R828_I2C.RegAddr = 0x0B;
	pTuner->R828_Arry[6]    = (pTuner->R828_Arry[6] & 0x90) | 0x60;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[6];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//adc=on, vga code mode, gain = 26.5dB
	pTuner->R828_I2C.RegAddr = 0x0C;
	pTuner->R828_Arry[7]    = (pTuner->R828_Arry[7] & 0x60) | 0x0B;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[7];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//ring clk = on
	pTuner->R828_I2C.RegAddr = 0x0F;
	pTuner->R828_Arry[10]   &= 0xF7;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[10];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//ring power = on
	pTuner->R828_I2C.RegAddr = 0x18;
	pTuner->R828_Arry[19]    = pTuner->R828_Arry[19] | 0x10;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[19];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//from ring = ring pll in
	pTuner->R828_I2C.RegAddr = 0x1C;
	pTuner->R828_Arry[23]    = pTuner->R828_Arry[23] | 0x02;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[23];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//sw_pdect = det3
	pTuner->R828_I2C.RegAddr = 0x1E;
	pTuner->R828_Arry[25]    = pTuner->R828_Arry[25] | 0x80;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[25];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	// Set filt_3dB
	pTuner->R828_Arry[1]  = pTuner->R828_Arry[1] | 0x20;
	pTuner->R828_I2C.RegAddr  = 0x06;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[1];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

static R828_ErrCode R828_IMR(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT8 IMR_MEM, int IM_Flag)
{
	
	UINT32 RingVCO;
	UINT32 RingFreq;
	UINT32 RingRef;
	UINT8 n_ring;
	UINT8 n;
	
	R828_SectType IMR_POINT;
	
	
	RingVCO = 0;
	RingFreq = 0;
	RingRef = 0;
	n_ring = 0;
	
	if (R828_Xtal>24000)
		RingRef = R828_Xtal /2;
	else
		RingRef = R828_Xtal;
	
	for(n=0;n<16;n++)
	{
		if((16+n)* 8 * RingRef >= 3100000)
		{
			n_ring=n;
			break;
		}
		
		if(n==15)   //n_ring not found
		{
            //return RT_Fail;
			n_ring=n;
		}
		
	}
	
	pTuner->R828_Arry[19] &= 0xF0;      //set ring[3:0]
	pTuner->R828_Arry[19] |= n_ring;
	RingVCO = (16+n_ring)* 8 * RingRef;
	pTuner->R828_Arry[19]&=0xDF;   //clear ring_se23
	pTuner->R828_Arry[20]&=0xFC;   //clear ring_seldiv
	pTuner->R828_Arry[26]&=0xFC;   //clear ring_att
	
	switch(IMR_MEM)
	{
		case 0:
			RingFreq = RingVCO/48;
			pTuner->R828_Arry[19]|=0x20;  // ring_se23 = 1
			pTuner->R828_Arry[20]|=0x03;  // ring_seldiv = 3
			pTuner->R828_Arry[26]|=0x02;  // ring_att 10
			break;
		case 1:
			RingFreq = RingVCO/16;
			pTuner->R828_Arry[19]|=0x00;  // ring_se23 = 0
			pTuner->R828_Arry[20]|=0x02;  // ring_seldiv = 2
			pTuner->R828_Arry[26]|=0x00;  // pw_ring 00
			break;
		case 2:
			RingFreq = RingVCO/8;
			pTuner->R828_Arry[19]|=0x00;  // ring_se23 = 0
			pTuner->R828_Arry[20]|=0x01;  // ring_seldiv = 1
			pTuner->R828_Arry[26]|=0x03;  // pw_ring 11
			break;
		case 3:
			RingFreq = RingVCO/6;
			pTuner->R828_Arry[19]|=0x20;  // ring_se23 = 1
			pTuner->R828_Arry[20]|=0x00;  // ring_seldiv = 0
			pTuner->R828_Arry[26]|=0x03;  // pw_ring 11
			break;
		case 4:
			RingFreq = RingVCO/4;
			pTuner->R828_Arry[19]|=0x00;  // ring_se23 = 0
			pTuner->R828_Arry[20]|=0x00;  // ring_seldiv = 0
			pTuner->R828_Arry[26]|=0x01;  // pw_ring 01
			break;
		default:
			RingFreq = RingVCO/4;
			pTuner->R828_Arry[19]|=0x00;  // ring_se23 = 0
			pTuner->R828_Arry[20]|=0x00;  // ring_seldiv = 0
			pTuner->R828_Arry[26]|=0x01;  // pw_ring 01
			break;
	}
	
	
	//write pw_ring,n_ring,ringdiv2 to I2C
	
	//------------n_ring,ring_se23----------//
	pTuner->R828_I2C.RegAddr = 0x18;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[19];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//------------ring_sediv----------------//
	pTuner->R828_I2C.RegAddr = 0x19;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[20];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	//------------pw_ring-------------------//
	pTuner->R828_I2C.RegAddr = 0x1f;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[26];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Must do before PLL()
	if(R828_MUX(pTuner, RingFreq - 5300) != RT_Success)				//MUX input freq ~ RF_in Freq
		return RT_Fail;
	
	if(R828_PLL(pTuner, (RingFreq - 5300) * 1000, STD_SIZE) != RT_Success)                //set pll freq = ring freq - 6M
	    return RT_Fail;
	
	if(IM_Flag == TRUE)
	{
		if(R828_IQ(pTuner, &IMR_POINT) != RT_Success)
			return RT_Fail;
	}
	else
	{
		IMR_POINT.Gain_X = pTuner->IMR_Data[3].Gain_X;
		IMR_POINT.Phase_Y = pTuner->IMR_Data[3].Phase_Y;
		IMR_POINT.Value = pTuner->IMR_Data[3].Value;
		if(R828_F_IMR(pTuner, &IMR_POINT) != RT_Success)
			return RT_Fail;
	}
	
	//Save IMR Value
	switch(IMR_MEM)
	{
		case 0:
			pTuner->IMR_Data[0].Gain_X  = IMR_POINT.Gain_X;
			pTuner->IMR_Data[0].Phase_Y = IMR_POINT.Phase_Y;
			pTuner->IMR_Data[0].Value   = IMR_POINT.Value;
			break;
		case 1:
			pTuner->IMR_Data[1].Gain_X  = IMR_POINT.Gain_X;
			pTuner->IMR_Data[1].Phase_Y = IMR_POINT.Phase_Y;
			pTuner->IMR_Data[1].Value   = IMR_POINT.Value;
			break;
		case 2:
			pTuner->IMR_Data[2].Gain_X  = IMR_POINT.Gain_X;
			pTuner->IMR_Data[2].Phase_Y = IMR_POINT.Phase_Y;
			pTuner->IMR_Data[2].Value   = IMR_POINT.Value;
			break;
		case 3:
			pTuner->IMR_Data[3].Gain_X  = IMR_POINT.Gain_X;
			pTuner->IMR_Data[3].Phase_Y = IMR_POINT.Phase_Y;
			pTuner->IMR_Data[3].Value   = IMR_POINT.Value;
			break;
		case 4:
			pTuner->IMR_Data[4].Gain_X  = IMR_POINT.Gain_X;
			pTuner->IMR_Data[4].Phase_Y = IMR_POINT.Phase_Y;
			pTuner->IMR_Data[4].Value   = IMR_POINT.Value;
			break;
		default:
			pTuner->IMR_Data[4].Gain_X  = IMR_POINT.Gain_X;
			pTuner->IMR_Data[4].Phase_Y = IMR_POINT.Phase_Y;
			pTuner->IMR_Data[4].Value   = IMR_POINT.Value;
			break;
	}
	return RT_Success;
}

static R828_ErrCode R828_PLL(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT32 LO_Freq, R828_Standard_Type R828_Standard)
{
	
	//	R820T_EXTRA_MODULE *pExtra;
	
	UINT8  MixDiv;
	UINT8  DivBuf;
	UINT8  Ni;
	UINT8  Si;
	UINT8  DivNum;
	UINT8  Nint;
	UINT32 VCO_Min_kHz;
	UINT32 VCO_Max_kHz;
	uint64_t VCO_Freq;
	UINT32 PLL_Ref;		//Max 24000 (kHz)
	UINT32 VCO_Fra;		//VCO contribution by SDM (kHz)
	UINT16 Nsdm;
	UINT16 SDM;
	UINT16 SDM16to9;
	UINT16 SDM8to1;
	//UINT8  Judge    = 0;
	UINT8  VCO_fine_tune;
	
	MixDiv   = 2;
	DivBuf   = 0;
	Ni       = 0;
	Si       = 0;
	DivNum   = 0;
	Nint     = 0;
	VCO_Min_kHz  = 1770000;
	VCO_Max_kHz  = VCO_Min_kHz*2;
	VCO_Freq = 0;
	PLL_Ref	= 0;		//Max 24000 (kHz)
	VCO_Fra	= 0;		//VCO contribution by SDM (kHz)
	Nsdm		= 2;
	SDM		= 0;
	SDM16to9	= 0;
	SDM8to1  = 0;
	//UINT8  Judge    = 0;
	VCO_fine_tune = 0;
	
#if 0
	if ((Rafael_Chip==R620D) || (Rafael_Chip==R828D) || (Rafael_Chip==R828))  //X'tal can't not exceed 20MHz for ATV
	{
		if(R828_Standard <= SECAM_L1)	  //ref set refdiv2, reffreq = Xtal/2 on ATV application
		{
			pTuner->R828_Arry[11] |= 0x10; //b4=1
			PLL_Ref = R828_Xtal /2;
		}
		else //DTV, FilCal, IMR
		{
			pTuner->R828_Arry[11] &= 0xEF;
			PLL_Ref = R828_Xtal;
		}
	}
	else
	{
		if(R828_Xtal > 24000)
		{
			pTuner->R828_Arry[11] |= 0x10; //b4=1
			PLL_Ref = R828_Xtal /2;
		}
		else
		{
			pTuner->R828_Arry[11] &= 0xEF;
			PLL_Ref = R828_Xtal;
		}
	}
#endif
	//Hack
	pTuner->R828_Arry[11] &= 0xEF;
	PLL_Ref = 28800000;
	
	pTuner->R828_I2C.RegAddr = 0x10;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[11];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//set pll autotune = 128kHz
	pTuner->R828_I2C.RegAddr = 0x1A;
	pTuner->R828_Arry[21]    = pTuner->R828_Arry[21] & 0xF3;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[21];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Set VCO current = 100
	pTuner->R828_I2C.RegAddr = 0x12;
	pTuner->R828_Arry[13]    = (pTuner->R828_Arry[13] & 0x1F) | 0x80;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[13];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Divider
	while(MixDiv <= 64)
	{
		if((((LO_Freq/1000) * MixDiv) >= VCO_Min_kHz) && (((LO_Freq/1000) * MixDiv) < VCO_Max_kHz))
		{
			DivBuf = MixDiv;
			while(DivBuf > 2)
			{
				DivBuf = DivBuf >> 1;
				DivNum ++;
			}
			break;
		}
		MixDiv = MixDiv << 1;
	}
	
	pTuner->R828_I2C_Len.RegAddr = 0x00;
	pTuner->R828_I2C_Len.Len     = 5;
	if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
		return RT_Fail;
	
	VCO_fine_tune = (pTuner->R828_I2C_Len.Data[4] & 0x30)>>4;
	
	if(VCO_fine_tune > VCO_pwr_ref)
		DivNum = DivNum - 1;
	else if(VCO_fine_tune < VCO_pwr_ref)
	    DivNum = DivNum + 1;
	
	pTuner->R828_I2C.RegAddr = 0x10;
	pTuner->R828_Arry[11] &= 0x1F;
	pTuner->R828_Arry[11] |= (DivNum << 5);
	pTuner->R828_I2C.Data = pTuner->R828_Arry[11];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	VCO_Freq = (uint64_t)(LO_Freq * (uint64_t)MixDiv);
	Nint     = (UINT8) (VCO_Freq / 2 / PLL_Ref);
	VCO_Fra  = (UINT16) ((VCO_Freq - 2 * PLL_Ref * Nint) / 1000);
	
	//Hack
	PLL_Ref /= 1000;
	
	//	printf("VCO_Freq = %lu, Nint= %u, VCO_Fra= %lu, LO_Freq= %u, MixDiv= %u\n", VCO_Freq, Nint, VCO_Fra, LO_Freq, MixDiv);
	
	//boundary spur prevention
	if (VCO_Fra < PLL_Ref/64)           //2*PLL_Ref/128
		VCO_Fra = 0;
	else if (VCO_Fra > PLL_Ref*127/64)  //2*PLL_Ref*127/128
	{
		VCO_Fra = 0;
		Nint ++;
	}
	else if((VCO_Fra > PLL_Ref*127/128) && (VCO_Fra < PLL_Ref)) //> 2*PLL_Ref*127/256,  < 2*PLL_Ref*128/256
		VCO_Fra = PLL_Ref*127/128;      // VCO_Fra = 2*PLL_Ref*127/256
	else if((VCO_Fra > PLL_Ref) && (VCO_Fra < PLL_Ref*129/128)) //> 2*PLL_Ref*128/256,  < 2*PLL_Ref*129/256
		VCO_Fra = PLL_Ref*129/128;      // VCO_Fra = 2*PLL_Ref*129/256
	else
		VCO_Fra = VCO_Fra;
	
	if (Nint > 63) {
		fprintf(stderr, "[R820T] No valid PLL values for %u Hz!\n", LO_Freq);
		return RT_Fail;
	}
	
	//N & S
	Ni       = (Nint - 13) / 4;
	Si       = Nint - 4 *Ni - 13;
	pTuner->R828_I2C.RegAddr = 0x14;
	pTuner->R828_Arry[15]  = 0x00;
	pTuner->R828_Arry[15] |= (Ni + (Si << 6));
	pTuner->R828_I2C.Data = pTuner->R828_Arry[15];
	
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//pw_sdm
	pTuner->R828_I2C.RegAddr = 0x12;
	pTuner->R828_Arry[13] &= 0xF7;
	if(VCO_Fra == 0)
		pTuner->R828_Arry[13] |= 0x08;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[13];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//SDM calculator
	while(VCO_Fra > 1)
	{
		if (VCO_Fra > (2*PLL_Ref / Nsdm))
		{
			SDM = SDM + 32768 / (Nsdm/2);
			VCO_Fra = VCO_Fra - 2*PLL_Ref / Nsdm;
			if (Nsdm >= 0x8000)
				break;
		}
		Nsdm = Nsdm << 1;
	}
	
	SDM16to9 = SDM >> 8;
	SDM8to1 =  SDM - (SDM16to9 << 8);
	
	pTuner->R828_I2C.RegAddr = 0x16;
	pTuner->R828_Arry[17]    = (UINT8) SDM16to9;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[17];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	pTuner->R828_I2C.RegAddr = 0x15;
	pTuner->R828_Arry[16]    = (UINT8) SDM8to1;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[16];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//	R828_Delay_MS(10);
	
	if ((Rafael_Chip==R620D) || (Rafael_Chip==R828D) || (Rafael_Chip==R828))
	{
		if(R828_Standard <= SECAM_L1)
			R828_Delay_MS(pTuner, 20);
		else
			R828_Delay_MS(pTuner, 10);
	}
	else
	{
		R828_Delay_MS(pTuner, 10);
	}
	
	//check PLL lock status
	pTuner->R828_I2C_Len.RegAddr = 0x00;
	pTuner->R828_I2C_Len.Len     = 3;
	if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
		return RT_Fail;
	
	if( (pTuner->R828_I2C_Len.Data[2] & 0x40) == 0x00 )
	{
		fprintf(stderr, "[R820T] PLL not locked for %u Hz!\n", LO_Freq);
		pTuner->R828_I2C.RegAddr = 0x12;
		pTuner->R828_Arry[13]    = (pTuner->R828_Arry[13] & 0x1F) | 0x60;  //increase VCO current
		pTuner->R828_I2C.Data    = pTuner->R828_Arry[13];
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		return RT_Fail;
	}
	
	//set pll autotune = 8kHz
	pTuner->R828_I2C.RegAddr = 0x1A;
	pTuner->R828_Arry[21]    = pTuner->R828_Arry[21] | 0x08;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[21];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

static R828_ErrCode R828_MUX(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT32 RF_KHz)
{
	UINT8 RT_Reg08;
	UINT8 RT_Reg09;
	
	RT_Reg08   = 0;
	RT_Reg09   = 0;
	
	//Freq_Info_Type pTuner->Freq_Info1;
	pTuner->Freq_Info1 = R828_Freq_Sel(RF_KHz);
	
	// Open Drain
	pTuner->R828_I2C.RegAddr = 0x17;
	pTuner->R828_Arry[18] = (pTuner->R828_Arry[18] & 0xF7) | pTuner->Freq_Info1.OPEN_D;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[18];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// RF_MUX,Polymux
	pTuner->R828_I2C.RegAddr = 0x1A;
	pTuner->R828_Arry[21] = (pTuner->R828_Arry[21] & 0x3C) | pTuner->Freq_Info1.RF_MUX_PLOY;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[21];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// TF BAND
	pTuner->R828_I2C.RegAddr = 0x1B;
	pTuner->R828_Arry[22] &= 0x00;
	pTuner->R828_Arry[22] |= pTuner->Freq_Info1.TF_C;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[22];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// XTAL CAP & Drive
	pTuner->R828_I2C.RegAddr = 0x10;
	pTuner->R828_Arry[11] &= 0xF4;
	switch(pTuner->Xtal_cap_sel)
	{
		case XTAL_LOW_CAP_30P:
		case XTAL_LOW_CAP_20P:
			pTuner->R828_Arry[11] = pTuner->R828_Arry[11] | pTuner->Freq_Info1.XTAL_CAP20P | 0x08;
			break;
			
		case XTAL_LOW_CAP_10P:
			pTuner->R828_Arry[11] = pTuner->R828_Arry[11] | pTuner->Freq_Info1.XTAL_CAP10P | 0x08;
			break;
			
		case XTAL_LOW_CAP_0P:
			pTuner->R828_Arry[11] = pTuner->R828_Arry[11] | pTuner->Freq_Info1.XTAL_CAP0P | 0x08;
			break;
			
		case XTAL_HIGH_CAP_0P:
			pTuner->R828_Arry[11] = pTuner->R828_Arry[11] | pTuner->Freq_Info1.XTAL_CAP0P | 0x00;
			break;
			
		default:
			pTuner->R828_Arry[11] = pTuner->R828_Arry[11] | pTuner->Freq_Info1.XTAL_CAP0P | 0x08;
			break;
	}
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[11];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Set_IMR
	if(pTuner->R828_IMR_done_flag == TRUE)
	{
		RT_Reg08 = pTuner->IMR_Data[pTuner->Freq_Info1.IMR_MEM].Gain_X & 0x3F;
		RT_Reg09 = pTuner->IMR_Data[pTuner->Freq_Info1.IMR_MEM].Phase_Y & 0x3F;
	}
	else
	{
		RT_Reg08 = 0;
	    RT_Reg09 = 0;
	}
	
	pTuner->R828_I2C.RegAddr = 0x08;
	pTuner->R828_Arry[3] = R828_iniArry[3] & 0xC0;
	pTuner->R828_Arry[3] = pTuner->R828_Arry[3] | RT_Reg08;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[3];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x09;
	pTuner->R828_Arry[4] = R828_iniArry[4] & 0xC0;
	pTuner->R828_Arry[4] = pTuner->R828_Arry[4] | RT_Reg09;
	pTuner->R828_I2C.Data =pTuner->R828_Arry[4]  ;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

static R828_ErrCode R828_IQ(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* IQ_Pont)
{
	R828_SectType Compare_IQ[3];
	//	R828_SectType CompareTemp;
	//	UINT8 IQ_Count  = 0;
	UINT8 VGA_Count;
	UINT16 VGA_Read;
	UINT8  X_Direction;  // 1:X, 0:Y
	
	VGA_Count = 0;
	VGA_Read = 0;
	
	// increase VGA power to let image significant
	for(VGA_Count = 12;VGA_Count < 16;VGA_Count ++)
	{
		pTuner->R828_I2C.RegAddr = 0x0C;
		pTuner->R828_I2C.Data    = (pTuner->R828_Arry[7] & 0xF0) + VGA_Count;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		R828_Delay_MS(pTuner, 10); //
		
		if(R828_Muti_Read(pTuner, 0x01, &VGA_Read) != RT_Success)
			return RT_Fail;
		
		if(VGA_Read > 40*4)
			break;
	}
	
	//initial 0x08, 0x09
	//Compare_IQ[0].Gain_X  = 0x40; //should be 0xC0 in R828, Jason
	//Compare_IQ[0].Phase_Y = 0x40; //should be 0x40 in R828
	Compare_IQ[0].Gain_X  = R828_iniArry[3] & 0xC0; // Jason modified, clear b[5], b[4:0]
	Compare_IQ[0].Phase_Y = R828_iniArry[4] & 0xC0; //
	
	//while(IQ_Count < 3)
	//{
	// Determine X or Y
	if(R828_IMR_Cross(pTuner, &Compare_IQ[0], &X_Direction) != RT_Success)
		return RT_Fail;
	
	//if(X_Direction==1)
	//{
	//    if(R828_IQ_Tree(Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != RT_Success) //X
	//	  return RT_Fail;
	//}
	//else
	//{
	//   if(R828_IQ_Tree(Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success) //Y
	//	return RT_Fail;
	//}
	
	/*
	 //--- X direction ---//
	 //X: 3 points
	 if(R828_IQ_Tree(Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != RT_Success) //
	 return RT_Fail;
	 
	 //compare and find min of 3 points. determine I/Q direction
	 if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
	 return RT_Fail;
	 
	 //increase step to find min value of this direction
	 if(R828_CompreStep(&Compare_IQ[0], 0x08) != RT_Success)
	 return RT_Fail;
	 */
	
	if(X_Direction==1)
	{
		//compare and find min of 3 points. determine I/Q direction
		if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
			return RT_Fail;
		
		//increase step to find min value of this direction
		if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x08) != RT_Success)  //X
			return RT_Fail;
	}
	else
	{
		//compare and find min of 3 points. determine I/Q direction
		if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
			return RT_Fail;
		
		//increase step to find min value of this direction
		if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x09) != RT_Success)  //Y
			return RT_Fail;
	}
	/*
	 //--- Y direction ---//
	 //Y: 3 points
	 if(R828_IQ_Tree(Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success) //
	 return RT_Fail;
	 
	 //compare and find min of 3 points. determine I/Q direction
	 if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
	 return RT_Fail;
	 
	 //increase step to find min value of this direction
	 if(R828_CompreStep(&Compare_IQ[0], 0x09) != RT_Success)
	 return RT_Fail;
	 */
	
	//Another direction
	if(X_Direction==1)
	{
		if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success) //Y
			return RT_Fail;
		
		//compare and find min of 3 points. determine I/Q direction
		if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
			return RT_Fail;
		
		//increase step to find min value of this direction
		if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x09) != RT_Success)  //Y
			return RT_Fail;
	}
	else
	{
		if(R828_IQ_Tree(pTuner, Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != RT_Success) //X
			return RT_Fail;
		
		//compare and find min of 3 points. determine I/Q direction
		if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
			return RT_Fail;
		
		//increase step to find min value of this direction
		if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x08) != RT_Success) //X
			return RT_Fail;
	}
	//CompareTemp = Compare_IQ[0];
	
	//--- Check 3 points again---//
	if(X_Direction==1)
	{
		if(R828_IQ_Tree(pTuner, Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != RT_Success) //X
			return RT_Fail;
	}
	else
	{
		if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success) //Y
			return RT_Fail;
	}
	
	//if(R828_IQ_Tree(Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != RT_Success) //
	//	return RT_Fail;
	
	if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	//if((CompareTemp.Gain_X == Compare_IQ[0].Gain_X) && (CompareTemp.Phase_Y == Compare_IQ[0].Phase_Y))//Ben Check
	//	break;
	
	//IQ_Count ++;
	//}
	//if(IQ_Count ==  3)
	//	return RT_Fail;
	
	//Section-4 Check
    /*
	 CompareTemp = Compare_IQ[0];
	 for(IQ_Count = 0;IQ_Count < 5;IQ_Count ++)
	 {
	 if(R828_Section(&Compare_IQ[0]) != RT_Success)
	 return RT_Fail;
	 
	 if((CompareTemp.Gain_X == Compare_IQ[0].Gain_X) && (CompareTemp.Phase_Y == Compare_IQ[0].Phase_Y))
	 break;
	 }
	 */
	
    //Section-9 check
    //if(R828_F_IMR(&Compare_IQ[0]) != RT_Success)
	if(R828_Section(pTuner, &Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	*IQ_Pont = Compare_IQ[0];
	
	//reset gain/phase control setting
	pTuner->R828_I2C.RegAddr = 0x08;
	pTuner->R828_I2C.Data    = R828_iniArry[3] & 0xC0; //Jason
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x09;
	pTuner->R828_I2C.Data    = R828_iniArry[4] & 0xC0;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

//--------------------------------------------------------------------------------------------
// Purpose: record IMC results by input gain/phase location
//          then adjust gain or phase positive 1 step and negtive 1 step, both record results
// input: FixPot: phase or gain
//        FlucPot phase or gain
//        PotReg: 0x08 or 0x09
//        CompareTree: 3 IMR trace and results
// output: TRUE or FALSE
//--------------------------------------------------------------------------------------------
static R828_ErrCode R828_IQ_Tree(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT8 FixPot, UINT8 FlucPot, UINT8 PotReg, R828_SectType* CompareTree)
{
	UINT8 TreeCount;
	UINT8 TreeTimes;
	UINT8 TempPot;
	UINT8 PntReg;
	
	TreeCount  = 0;
	TreeTimes = 3;
	TempPot   = 0;
	PntReg    = 0;
	
	if(PotReg == 0x08)
		PntReg = 0x09; //phase control
	else
		PntReg = 0x08; //gain control
	
	for(TreeCount = 0;TreeCount < TreeTimes;TreeCount ++)
	{
		pTuner->R828_I2C.RegAddr = PotReg;
		pTuner->R828_I2C.Data    = FixPot;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		pTuner->R828_I2C.RegAddr = PntReg;
		pTuner->R828_I2C.Data    = FlucPot;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		if(R828_Muti_Read(pTuner, 0x01, &CompareTree[TreeCount].Value) != RT_Success)
			return RT_Fail;
		
		if(PotReg == 0x08)
		{
			CompareTree[TreeCount].Gain_X  = FixPot;
			CompareTree[TreeCount].Phase_Y = FlucPot;
		}
		else
		{
			CompareTree[TreeCount].Phase_Y  = FixPot;
			CompareTree[TreeCount].Gain_X = FlucPot;
		}
		
		if(TreeCount == 0)   //try right-side point
			FlucPot ++;
		else if(TreeCount == 1) //try left-side point
		{
			if((FlucPot & 0x1F) < 0x02) //if absolute location is 1, change I/Q direction
			{
				TempPot = 2 - (FlucPot & 0x1F);
				if(FlucPot & 0x20) //b[5]:I/Q selection. 0:Q-path, 1:I-path
				{
					FlucPot &= 0xC0;
					FlucPot |= TempPot;
				}
				else
				{
					FlucPot |= (0x20 | TempPot);
				}
			}
			else
				FlucPot -= 2;
		}
	}
	
	return RT_Success;
}

//-----------------------------------------------------------------------------------/
// Purpose: compare IMC result aray [0][1][2], find min value and store to CorArry[0]
// input: CorArry: three IMR data array
// output: TRUE or FALSE
//-----------------------------------------------------------------------------------/
static R828_ErrCode R828_CompreCor(R828_SectType* CorArry)
{
	UINT8 CompCount;
	R828_SectType CorTemp;
	
	CompCount = 0;
	
	for(CompCount = 3;CompCount > 0;CompCount --)
	{
		if(CorArry[0].Value > CorArry[CompCount - 1].Value) //compare IMC result [0][1][2], find min value
		{
			CorTemp = CorArry[0];
			CorArry[0] = CorArry[CompCount - 1];
			CorArry[CompCount - 1] = CorTemp;
		}
	}
	
	return RT_Success;
}

//-------------------------------------------------------------------------------------//
// Purpose: if (Gain<9 or Phase<9), Gain+1 or Phase+1 and compare with min value
//          new < min => update to min and continue
//          new > min => Exit
// input: StepArry: three IMR data array
//        Pace: gain or phase register
// output: TRUE or FALSE
//-------------------------------------------------------------------------------------//
static R828_ErrCode R828_CompreStep(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* StepArry, UINT8 Pace)
{
	//UINT8 StepCount = 0;
	R828_SectType StepTemp;
	
	//min value already saved in StepArry[0]
	StepTemp.Phase_Y = StepArry[0].Phase_Y;
	StepTemp.Gain_X  = StepArry[0].Gain_X;
	
	while(((StepTemp.Gain_X & 0x1F) < IMR_TRIAL) && ((StepTemp.Phase_Y & 0x1F) < IMR_TRIAL))  //5->10
	{
		if(Pace == 0x08)
			StepTemp.Gain_X ++;
		else
			StepTemp.Phase_Y ++;
		
		pTuner->R828_I2C.RegAddr = 0x08;
		pTuner->R828_I2C.Data    = StepTemp.Gain_X ;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		pTuner->R828_I2C.RegAddr = 0x09;
		pTuner->R828_I2C.Data    = StepTemp.Phase_Y;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		if(R828_Muti_Read(pTuner, 0x01, &StepTemp.Value) != RT_Success)
			return RT_Fail;
		
		if(StepTemp.Value <= StepArry[0].Value)
		{
			StepArry[0].Gain_X  = StepTemp.Gain_X;
			StepArry[0].Phase_Y = StepTemp.Phase_Y;
			StepArry[0].Value   = StepTemp.Value;
		}
		else
		{
			break;
		}
		
	} //end of while()
	
	return RT_Success;
}

//-----------------------------------------------------------------------------------/
// Purpose: read multiple IMC results for stability
// input: IMR_Reg: IMC result address
//        IMR_Result_Data: result
// output: TRUE or FALSE
//-----------------------------------------------------------------------------------/
static R828_ErrCode R828_Muti_Read(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT8 IMR_Reg, UINT16* IMR_Result_Data)  //jason modified
{
	UINT8 ReadCount;
	UINT16 ReadAmount;
	UINT8 ReadMax;
	UINT8 ReadMin;
	UINT8 ReadData;
	
	ReadCount     = 0;
	ReadAmount  = 0;
	ReadMax = 0;
	ReadMin = 255;
	ReadData = 0;
	
    R828_Delay_MS(pTuner, 5);
	
	for(ReadCount = 0;ReadCount < 6;ReadCount ++)
	{
		pTuner->R828_I2C_Len.RegAddr = 0x00;
		pTuner->R828_I2C_Len.Len     = IMR_Reg + 1;  //IMR_Reg = 0x01
		if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
			return RT_Fail;
		
		ReadData = pTuner->R828_I2C_Len.Data[1];
		
		ReadAmount = ReadAmount + (UINT16)ReadData;
		
		if(ReadData < ReadMin)
			ReadMin = ReadData;
		
        if(ReadData > ReadMax)
			ReadMax = ReadData;
	}
	*IMR_Result_Data = ReadAmount - (UINT16)ReadMax - (UINT16)ReadMin;
	
	return RT_Success;
}

static R828_ErrCode R828_Section(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* IQ_Pont)
{
	R828_SectType Compare_IQ[3];
	R828_SectType Compare_Bet[3];
	
	//Try X-1 column and save min result to Compare_Bet[0]
	if((IQ_Pont->Gain_X & 0x1F) == 0x00)
	{
		/*
		 if((IQ_Pont->Gain_X & 0xE0) == 0x40) //bug => only compare b[5],
		 Compare_IQ[0].Gain_X = 0x61; // Gain=1, I-path //Jason
		 else
		 Compare_IQ[0].Gain_X = 0x41; // Gain=1, Q-path
		 */
		Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) & 0xDF) + 1;  //Q-path, Gain=1
	}
	else
		Compare_IQ[0].Gain_X  = IQ_Pont->Gain_X - 1;  //left point
	Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;
	
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success)  // y-direction
		return RT_Fail;
	
	if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	Compare_Bet[0].Gain_X = Compare_IQ[0].Gain_X;
	Compare_Bet[0].Phase_Y = Compare_IQ[0].Phase_Y;
	Compare_Bet[0].Value = Compare_IQ[0].Value;
	
	//Try X column and save min result to Compare_Bet[1]
	Compare_IQ[0].Gain_X = IQ_Pont->Gain_X;
	Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;
	
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	Compare_Bet[1].Gain_X = Compare_IQ[0].Gain_X;
	Compare_Bet[1].Phase_Y = Compare_IQ[0].Phase_Y;
	Compare_Bet[1].Value = Compare_IQ[0].Value;
	
	//Try X+1 column and save min result to Compare_Bet[2]
	if((IQ_Pont->Gain_X & 0x1F) == 0x00)
		Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) | 0x20) + 1;  //I-path, Gain=1
	else
	    Compare_IQ[0].Gain_X = IQ_Pont->Gain_X + 1;
	Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;
	
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	Compare_Bet[2].Gain_X = Compare_IQ[0].Gain_X;
	Compare_Bet[2].Phase_Y = Compare_IQ[0].Phase_Y;
	Compare_Bet[2].Value = Compare_IQ[0].Value;
	
	if(R828_CompreCor(&Compare_Bet[0]) != RT_Success)
		return RT_Fail;
	
	*IQ_Pont = Compare_Bet[0];
	
	return RT_Success;
}

static R828_ErrCode R828_IMR_Cross(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* IQ_Pont, UINT8* X_Direct)
{
	
	R828_SectType Compare_Cross[5]; //(0,0)(0,Q-1)(0,I-1)(Q-1,0)(I-1,0)
	R828_SectType Compare_Temp;
	UINT8 CrossCount;
    UINT8 Reg08;
	UINT8 Reg09;
	
	CrossCount = 0;
    Reg08 = R828_iniArry[3] & 0xC0;
	Reg09 = R828_iniArry[4] & 0xC0;
	
	//memset(&Compare_Temp,0, sizeof(R828_SectType));
	Compare_Temp.Gain_X = 0;
	Compare_Temp.Phase_Y = 0;
	Compare_Temp.Value = 0;
	
	Compare_Temp.Value = 255;
	
	for(CrossCount=0; CrossCount<5; CrossCount++)
	{
		
		if(CrossCount==0)
		{
			Compare_Cross[CrossCount].Gain_X = Reg08;
			Compare_Cross[CrossCount].Phase_Y = Reg09;
		}
		else if(CrossCount==1)
		{
			Compare_Cross[CrossCount].Gain_X = Reg08;       //0
			Compare_Cross[CrossCount].Phase_Y = Reg09 + 1;  //Q-1
		}
		else if(CrossCount==2)
		{
			Compare_Cross[CrossCount].Gain_X = Reg08;               //0
			Compare_Cross[CrossCount].Phase_Y = (Reg09 | 0x20) + 1; //I-1
		}
		else if(CrossCount==3)
		{
			Compare_Cross[CrossCount].Gain_X = Reg08 + 1; //Q-1
			Compare_Cross[CrossCount].Phase_Y = Reg09;
		}
		else
		{
			Compare_Cross[CrossCount].Gain_X = (Reg08 | 0x20) + 1; //I-1
			Compare_Cross[CrossCount].Phase_Y = Reg09;
		}
		
    	pTuner->R828_I2C.RegAddr = 0x08;
	    pTuner->R828_I2C.Data    = Compare_Cross[CrossCount].Gain_X;
	    if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
	    pTuner->R828_I2C.RegAddr = 0x09;
	    pTuner->R828_I2C.Data    = Compare_Cross[CrossCount].Phase_Y;
	    if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
        if(R828_Muti_Read(pTuner, 0x01, &Compare_Cross[CrossCount].Value) != RT_Success)
			return RT_Fail;
		
		if( Compare_Cross[CrossCount].Value < Compare_Temp.Value)
		{
			Compare_Temp.Value = Compare_Cross[CrossCount].Value;
			Compare_Temp.Gain_X = Compare_Cross[CrossCount].Gain_X;
			Compare_Temp.Phase_Y = Compare_Cross[CrossCount].Phase_Y;
		}
	} //end for loop
	
	
    if((Compare_Temp.Phase_Y & 0x1F)==1)  //y-direction
	{
		*X_Direct = (UINT8) 0;
		IQ_Pont[0].Gain_X = Compare_Cross[0].Gain_X;
		IQ_Pont[0].Phase_Y = Compare_Cross[0].Phase_Y;
		IQ_Pont[0].Value = Compare_Cross[0].Value;
		
		IQ_Pont[1].Gain_X = Compare_Cross[1].Gain_X;
		IQ_Pont[1].Phase_Y = Compare_Cross[1].Phase_Y;
		IQ_Pont[1].Value = Compare_Cross[1].Value;
		
		IQ_Pont[2].Gain_X = Compare_Cross[2].Gain_X;
		IQ_Pont[2].Phase_Y = Compare_Cross[2].Phase_Y;
		IQ_Pont[2].Value = Compare_Cross[2].Value;
	}
	else //(0,0) or x-direction
	{
		*X_Direct = (UINT8) 1;
		IQ_Pont[0].Gain_X = Compare_Cross[0].Gain_X;
		IQ_Pont[0].Phase_Y = Compare_Cross[0].Phase_Y;
		IQ_Pont[0].Value = Compare_Cross[0].Value;
		
		IQ_Pont[1].Gain_X = Compare_Cross[3].Gain_X;
		IQ_Pont[1].Phase_Y = Compare_Cross[3].Phase_Y;
		IQ_Pont[1].Value = Compare_Cross[3].Value;
		
		IQ_Pont[2].Gain_X = Compare_Cross[4].Gain_X;
		IQ_Pont[2].Phase_Y = Compare_Cross[4].Phase_Y;
		IQ_Pont[2].Value = Compare_Cross[4].Value;
	}
	return RT_Success;
}

//----------------------------------------------------------------------------------------//
// purpose: search surrounding points from previous point
//          try (x-1), (x), (x+1) columns, and find min IMR result point
// input: IQ_Pont: previous point data(IMR Gain, Phase, ADC Result, RefRreq)
//                 will be updated to final best point
// output: TRUE or FALSE
//----------------------------------------------------------------------------------------//
R828_ErrCode R828_F_IMR(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_SectType* IQ_Pont)
{
	R828_SectType Compare_IQ[3];
	R828_SectType Compare_Bet[3];
	UINT8 VGA_Count;
	UINT16 VGA_Read;
	
	VGA_Count = 0;
	VGA_Read = 0;
	
	//VGA
	for(VGA_Count = 12;VGA_Count < 16;VGA_Count ++)
	{
		pTuner->R828_I2C.RegAddr = 0x0C;
        pTuner->R828_I2C.Data    = (pTuner->R828_Arry[7] & 0xF0) + VGA_Count;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		R828_Delay_MS(pTuner, 10);
		
		if(R828_Muti_Read(pTuner, 0x01, &VGA_Read) != RT_Success)
			return RT_Fail;
		
		if(VGA_Read > 40*4)
			break;
	}
	
	//Try X-1 column and save min result to Compare_Bet[0]
	if((IQ_Pont->Gain_X & 0x1F) == 0x00)
	{
		Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) & 0xDF) + 1;  //Q-path, Gain=1
	}
	else
		Compare_IQ[0].Gain_X  = IQ_Pont->Gain_X - 1;  //left point
	Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;
	
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success)  // y-direction
		return RT_Fail;
	
	if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	Compare_Bet[0].Gain_X = Compare_IQ[0].Gain_X;
	Compare_Bet[0].Phase_Y = Compare_IQ[0].Phase_Y;
	Compare_Bet[0].Value = Compare_IQ[0].Value;
	
	//Try X column and save min result to Compare_Bet[1]
	Compare_IQ[0].Gain_X = IQ_Pont->Gain_X;
	Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;
	
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	Compare_Bet[1].Gain_X = Compare_IQ[0].Gain_X;
	Compare_Bet[1].Phase_Y = Compare_IQ[0].Phase_Y;
	Compare_Bet[1].Value = Compare_IQ[0].Value;
	
	//Try X+1 column and save min result to Compare_Bet[2]
	if((IQ_Pont->Gain_X & 0x1F) == 0x00)
		Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) | 0x20) + 1;  //I-path, Gain=1
	else
	    Compare_IQ[0].Gain_X = IQ_Pont->Gain_X + 1;
	Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;
	
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	if(R828_CompreCor(&Compare_IQ[0]) != RT_Success)
		return RT_Fail;
	
	Compare_Bet[2].Gain_X = Compare_IQ[0].Gain_X;
	Compare_Bet[2].Phase_Y = Compare_IQ[0].Phase_Y;
	Compare_Bet[2].Value = Compare_IQ[0].Value;
	
	if(R828_CompreCor(&Compare_Bet[0]) != RT_Success)
		return RT_Fail;
	
	*IQ_Pont = Compare_Bet[0];
	
	return RT_Success;
}

R828_ErrCode R828_GPIO(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_GPIO_Type R828_GPIO_Conrl)
{
	if(R828_GPIO_Conrl == HI_SIG)
		pTuner->R828_Arry[10] |= 0x01;
	else
		pTuner->R828_Arry[10] &= 0xFE;
	
	pTuner->R828_I2C.RegAddr = 0x0F;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[10];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

R828_ErrCode R828_SetStandard(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_Standard_Type RT_Standard)
{
	
	// Used Normal Arry to Modify
	UINT8 ArrayNum;
	
	ArrayNum = 27;
	for(ArrayNum=0;ArrayNum<27;ArrayNum++)
	{
		pTuner->R828_Arry[ArrayNum] = R828_iniArry[ArrayNum];
	}
	
	
	// Record Init Flag & Xtal_check Result
	if(pTuner->R828_IMR_done_flag == TRUE)
        pTuner->R828_Arry[7]    = (pTuner->R828_Arry[7] & 0xF0) | 0x01 | (pTuner->Xtal_cap_sel<<1);
	else
	    pTuner->R828_Arry[7]    = (pTuner->R828_Arry[7] & 0xF0) | 0x00;
	
	pTuner->R828_I2C.RegAddr = 0x0C;
    pTuner->R828_I2C.Data    = pTuner->R828_Arry[7];
    if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// Record version
	pTuner->R828_I2C.RegAddr = 0x13;
	pTuner->R828_Arry[14]    = (pTuner->R828_Arry[14] & 0xC0) | VER_NUM;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[14];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
	    return RT_Fail;
	
	
    //for LT Gain test
	if(RT_Standard > SECAM_L1)
	{
		pTuner->R828_I2C.RegAddr = 0x1D;  //[5:3] LNA TOP
		pTuner->R828_I2C.Data = (pTuner->R828_Arry[24] & 0xC7) | 0x00;
	    if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		    return RT_Fail;
		
		//R828_Delay_MS(1);
	}
	
	// Look Up System Dependent Table
	pTuner->Sys_Info1 = R828_Sys_Sel(RT_Standard);
	pTuner->R828_IF_khz = pTuner->Sys_Info1.IF_KHz;
	pTuner->R828_CAL_LO_khz = pTuner->Sys_Info1.FILT_CAL_LO;
	
	// Filter Calibration
    if(pTuner->R828_Fil_Cal_flag[RT_Standard] == FALSE)
	{
		// do filter calibration
		if(R828_Filt_Cal(pTuner, pTuner->Sys_Info1.FILT_CAL_LO,pTuner->Sys_Info1.BW) != RT_Success)
		    return RT_Fail;
		
		
		// read and set filter code
		pTuner->R828_I2C_Len.RegAddr = 0x00;
		pTuner->R828_I2C_Len.Len     = 5;
		if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
			return RT_Fail;
		
		pTuner->R828_Fil_Cal_code[RT_Standard] = pTuner->R828_I2C_Len.Data[4] & 0x0F;
		
		//Filter Cali. Protection
		if(pTuner->R828_Fil_Cal_code[RT_Standard]==0 || pTuner->R828_Fil_Cal_code[RT_Standard]==15)
		{
			if(R828_Filt_Cal(pTuner, pTuner->Sys_Info1.FILT_CAL_LO,pTuner->Sys_Info1.BW) != RT_Success)
				return RT_Fail;
			
			pTuner->R828_I2C_Len.RegAddr = 0x00;
			pTuner->R828_I2C_Len.Len     = 5;
			if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
				return RT_Fail;
			
			pTuner->R828_Fil_Cal_code[RT_Standard] = pTuner->R828_I2C_Len.Data[4] & 0x0F;
			
			if(pTuner->R828_Fil_Cal_code[RT_Standard]==15) //narrowest
				pTuner->R828_Fil_Cal_code[RT_Standard] = 0;
			
		}
        pTuner->R828_Fil_Cal_flag[RT_Standard] = TRUE;
	}
	
	// Set Filter Q
	pTuner->R828_Arry[5]  = (pTuner->R828_Arry[5] & 0xE0) | pTuner->Sys_Info1.FILT_Q | pTuner->R828_Fil_Cal_code[RT_Standard];
	pTuner->R828_I2C.RegAddr  = 0x0A;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[5];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// Set BW, Filter_gain, & HP corner
	pTuner->R828_Arry[6]= (pTuner->R828_Arry[6] & 0x10) | pTuner->Sys_Info1.HP_COR;
	pTuner->R828_I2C.RegAddr  = 0x0B;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[6];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// Set Img_R
	pTuner->R828_Arry[2]  = (pTuner->R828_Arry[2] & 0x7F) | pTuner->Sys_Info1.IMG_R;
	pTuner->R828_I2C.RegAddr  = 0x07;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[2];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	
	// Set filt_3dB, V6MHz
	pTuner->R828_Arry[1]  = (pTuner->R828_Arry[1] & 0xCF) | pTuner->Sys_Info1.FILT_GAIN;
	pTuner->R828_I2C.RegAddr  = 0x06;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[1];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
    //channel filter extension
	pTuner->R828_Arry[25]  = (pTuner->R828_Arry[25] & 0x9F) | pTuner->Sys_Info1.EXT_ENABLE;
	pTuner->R828_I2C.RegAddr  = 0x1E;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[25];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	
	//Loop through
	pTuner->R828_Arry[0]  = (pTuner->R828_Arry[0] & 0x7F) | pTuner->Sys_Info1.LOOP_THROUGH;
	pTuner->R828_I2C.RegAddr  = 0x05;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[0];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Loop through attenuation
	pTuner->R828_Arry[26]  = (pTuner->R828_Arry[26] & 0x7F) | pTuner->Sys_Info1.LT_ATT;
	pTuner->R828_I2C.RegAddr  = 0x1F;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[26];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
    //filter extention widest
	pTuner->R828_Arry[10]  = (pTuner->R828_Arry[10] & 0x7F) | pTuner->Sys_Info1.FLT_EXT_WIDEST;
	pTuner->R828_I2C.RegAddr  = 0x0F;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[10];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//RF poly filter current
	pTuner->R828_Arry[20]  = (pTuner->R828_Arry[20] & 0x9F) | pTuner->Sys_Info1.POLYFIL_CUR;
	pTuner->R828_I2C.RegAddr  = 0x19;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[20];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

static R828_ErrCode R828_Filt_Cal(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, UINT32 Cal_Freq, BW_Type R828_BW)
{
	//set in Sys_sel()
	/*
	 if(R828_BW == BW_8M)
	 {
	 //set filt_cap = no cap
	 pTuner->R828_I2C.RegAddr = 0x0B;  //reg11
	 pTuner->R828_Arry[6]   &= 0x9F;  //filt_cap = no cap
	 pTuner->R828_I2C.Data    = pTuner->R828_Arry[6];
	 }
	 else if(R828_BW == BW_7M)
	 {
	 //set filt_cap = +1 cap
	 pTuner->R828_I2C.RegAddr = 0x0B;  //reg11
	 pTuner->R828_Arry[6]   &= 0x9F;  //filt_cap = no cap
	 pTuner->R828_Arry[6]   |= 0x20;  //filt_cap = +1 cap
	 pTuner->R828_I2C.Data    = pTuner->R828_Arry[6];
	 }
	 else if(R828_BW == BW_6M)
	 {
	 //set filt_cap = +2 cap
	 pTuner->R828_I2C.RegAddr = 0x0B;  //reg11
	 pTuner->R828_Arry[6]   &= 0x9F;  //filt_cap = no cap
	 pTuner->R828_Arry[6]   |= 0x60;  //filt_cap = +2 cap
	 pTuner->R828_I2C.Data    = pTuner->R828_Arry[6];
	 }
	 
	 
	 if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
	 return RT_Fail;
	 */
	
    // Set filt_cap
	pTuner->R828_I2C.RegAddr  = 0x0B;
	pTuner->R828_Arry[6]= (pTuner->R828_Arry[6] & 0x9F) | (pTuner->Sys_Info1.HP_COR & 0x60);
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[6];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	
	//set cali clk =on
	pTuner->R828_I2C.RegAddr = 0x0F;  //reg15
	pTuner->R828_Arry[10]   |= 0x04;  //calibration clk=on
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[10];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//X'tal cap 0pF for PLL
	pTuner->R828_I2C.RegAddr = 0x10;
	pTuner->R828_Arry[11]    = (pTuner->R828_Arry[11] & 0xFC) | 0x00;
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[11];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Set PLL Freq = Filter Cali Freq
	if(R828_PLL(pTuner, Cal_Freq * 1000, STD_SIZE) != RT_Success)
		return RT_Fail;
	
	//Start Trigger
	pTuner->R828_I2C.RegAddr = 0x0B;	//reg11
	pTuner->R828_Arry[6]   |= 0x10;	    //vstart=1
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[6];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//delay 0.5ms
	R828_Delay_MS(pTuner, 1);
	
	//Stop Trigger
	pTuner->R828_I2C.RegAddr = 0x0B;
	pTuner->R828_Arry[6]   &= 0xEF;     //vstart=0
	pTuner->R828_I2C.Data    = pTuner->R828_Arry[6];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	
	//set cali clk =off
	pTuner->R828_I2C.RegAddr  = 0x0F;	//reg15
	pTuner->R828_Arry[10]    &= 0xFB;	//calibration clk=off
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[10];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
	
}

R828_ErrCode R828_SetFrequency(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_Set_Info R828_INFO, R828_SetFreq_Type R828_SetFreqMode)
{
	UINT32	LO_Hz;
	
#if 0
	// Check Input Frequency Range
	if((R828_INFO.RF_KHz<40000) || (R828_INFO.RF_KHz>900000))
	{
		return RT_Fail;
	}
#endif
	
	if(R828_INFO.R828_Standard==SECAM_L1)
		LO_Hz = R828_INFO.RF_Hz - (pTuner->Sys_Info1.IF_KHz * 1000);
	else
		LO_Hz = R828_INFO.RF_Hz + (pTuner->Sys_Info1.IF_KHz * 1000);
	
	//Set MUX dependent var. Must do before PLL( )
	if(R828_MUX(pTuner, LO_Hz/1000) != RT_Success)
        return RT_Fail;
	
	//Set PLL
	if(R828_PLL(pTuner, LO_Hz, R828_INFO.R828_Standard) != RT_Success)
        return RT_Fail;
	
	pTuner->R828_IMR_point_num = pTuner->Freq_Info1.IMR_MEM;
	
	
	//Set TOP,VTH,VTL
	pTuner->SysFreq_Info1 = R828_SysFreq_Sel(R828_INFO.R828_Standard, R828_INFO.RF_KHz);
	
    
	// write DectBW, pre_dect_TOP
	pTuner->R828_Arry[24] = (pTuner->R828_Arry[24] & 0x38) | (pTuner->SysFreq_Info1.LNA_TOP & 0xC7);
	pTuner->R828_I2C.RegAddr = 0x1D;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[24];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// write MIXER TOP, TOP+-1
	pTuner->R828_Arry[23] = (pTuner->R828_Arry[23] & 0x07) | (pTuner->SysFreq_Info1.MIXER_TOP & 0xF8);
	pTuner->R828_I2C.RegAddr = 0x1C;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[23];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
        return RT_Fail;
	
	
	// write LNA VTHL
	pTuner->R828_Arry[8] = (pTuner->R828_Arry[8] & 0x00) | pTuner->SysFreq_Info1.LNA_VTH_L;
	pTuner->R828_I2C.RegAddr = 0x0D;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[8];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
        return RT_Fail;
	
	// write MIXER VTHL
	pTuner->R828_Arry[9] = (pTuner->R828_Arry[9] & 0x00) | pTuner->SysFreq_Info1.MIXER_VTH_L;
	pTuner->R828_I2C.RegAddr = 0x0E;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[9];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
        return RT_Fail;
	
	// Cable-1/Air in
	pTuner->R828_I2C.RegAddr = 0x05;
	pTuner->R828_Arry[0] &= 0x9F;
	pTuner->R828_Arry[0] |= pTuner->SysFreq_Info1.AIR_CABLE1_IN;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[0];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// Cable-2 in
	pTuner->R828_I2C.RegAddr = 0x06;
	pTuner->R828_Arry[1] &= 0xF7;
	pTuner->R828_Arry[1] |= pTuner->SysFreq_Info1.CABLE2_IN;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[1];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//CP current
	pTuner->R828_I2C.RegAddr = 0x11;
	pTuner->R828_Arry[12] &= 0xC7;
	pTuner->R828_Arry[12] |= pTuner->SysFreq_Info1.CP_CUR;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[12];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//div buffer current
	pTuner->R828_I2C.RegAddr = 0x17;
	pTuner->R828_Arry[18] &= 0xCF;
	pTuner->R828_Arry[18] |= pTuner->SysFreq_Info1.DIV_BUF_CUR;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[18];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	// Set channel filter current
	pTuner->R828_I2C.RegAddr  = 0x0A;
	pTuner->R828_Arry[5]  = (pTuner->R828_Arry[5] & 0x9F) | pTuner->SysFreq_Info1.FILTER_CUR;
	pTuner->R828_I2C.Data     = pTuner->R828_Arry[5];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Air-In only for Astrometa
	pTuner->R828_Arry[0] =  (pTuner->R828_Arry[0] & 0x9F) | 0x00;
	pTuner->R828_Arry[1] =  (pTuner->R828_Arry[1] & 0xF7) | 0x00;
	
	pTuner->R828_I2C.RegAddr = 0x05;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[0];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x06;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[1];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	//Set LNA
	if(R828_INFO.R828_Standard > SECAM_L1)
	{
		
		if(R828_SetFreqMode==FAST_MODE)       //FAST mode
		{
			//pTuner->R828_Arry[24] = (pTuner->R828_Arry[24] & 0xC7) | 0x20; //LNA TOP:4
			pTuner->R828_Arry[24] = (pTuner->R828_Arry[24] & 0xC7) | 0x00; //LNA TOP:lowest
			pTuner->R828_I2C.RegAddr = 0x1D;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[24];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			pTuner->R828_Arry[23] = (pTuner->R828_Arry[23] & 0xFB);  // 0: normal mode
			pTuner->R828_I2C.RegAddr = 0x1C;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[23];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			pTuner->R828_Arry[1]  = (pTuner->R828_Arry[1] & 0xBF);   //0: PRE_DECT off
			pTuner->R828_I2C.RegAddr  = 0x06;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[1];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			//agc clk 250hz
			pTuner->R828_Arry[21]  = (pTuner->R828_Arry[21] & 0xCF) | 0x30;
			pTuner->R828_I2C.RegAddr  = 0x1A;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[21];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
		}
		else  //NORMAL mode
		{
			
			pTuner->R828_Arry[24] = (pTuner->R828_Arry[24] & 0xC7) | 0x00; //LNA TOP:lowest
			pTuner->R828_I2C.RegAddr = 0x1D;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[24];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			pTuner->R828_Arry[23] = (pTuner->R828_Arry[23] & 0xFB);  // 0: normal mode
			pTuner->R828_I2C.RegAddr = 0x1C;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[23];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			pTuner->R828_Arry[1]  = (pTuner->R828_Arry[1] & 0xBF);   //0: PRE_DECT off
			pTuner->R828_I2C.RegAddr  = 0x06;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[1];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			//agc clk 250hz
			pTuner->R828_Arry[21]  = (pTuner->R828_Arry[21] & 0xCF) | 0x30;   //250hz
			pTuner->R828_I2C.RegAddr  = 0x1A;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[21];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			R828_Delay_MS(pTuner, 250);
			
			// PRE_DECT on
			/*
			 pTuner->R828_Arry[1]  = (pTuner->R828_Arry[1] & 0xBF) | pTuner->SysFreq_Info1.PRE_DECT;
			 pTuner->R828_I2C.RegAddr  = 0x06;
			 pTuner->R828_I2C.Data     = pTuner->R828_Arry[1];
			 if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			 return RT_Fail;
             */
			// write LNA TOP = 3
			//pTuner->R828_Arry[24] = (pTuner->R828_Arry[24] & 0xC7) | (pTuner->SysFreq_Info1.LNA_TOP & 0x38);
			pTuner->R828_Arry[24] = (pTuner->R828_Arry[24] & 0xC7) | 0x18;  //TOP=3
			pTuner->R828_I2C.RegAddr = 0x1D;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[24];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			// write discharge mode
			pTuner->R828_Arry[23] = (pTuner->R828_Arry[23] & 0xFB) | (pTuner->SysFreq_Info1.MIXER_TOP & 0x04);
			pTuner->R828_I2C.RegAddr = 0x1C;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[23];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			// LNA discharge current
			pTuner->R828_Arry[25]  = (pTuner->R828_Arry[25] & 0xE0) | pTuner->SysFreq_Info1.LNA_DISCHARGE;
			pTuner->R828_I2C.RegAddr  = 0x1E;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[25];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			//agc clk 60hz
			pTuner->R828_Arry[21]  = (pTuner->R828_Arry[21] & 0xCF) | 0x20;
			pTuner->R828_I2C.RegAddr  = 0x1A;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[21];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
		}
	}
	else
	{
		if(R828_SetFreqMode==NORMAL_MODE || R828_SetFreqMode==FAST_MODE)
		{
			/*
             // PRE_DECT on
			 pTuner->R828_Arry[1]  = (pTuner->R828_Arry[1] & 0xBF) | pTuner->SysFreq_Info1.PRE_DECT;
			 pTuner->R828_I2C.RegAddr  = 0x06;
			 pTuner->R828_I2C.Data     = pTuner->R828_Arry[1];
			 if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			 return RT_Fail;
             */
			// PRE_DECT off
			pTuner->R828_Arry[1]  = (pTuner->R828_Arry[1] & 0xBF);   //0: PRE_DECT off
			pTuner->R828_I2C.RegAddr  = 0x06;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[1];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			// write LNA TOP
			pTuner->R828_Arry[24] = (pTuner->R828_Arry[24] & 0xC7) | (pTuner->SysFreq_Info1.LNA_TOP & 0x38);
			pTuner->R828_I2C.RegAddr = 0x1D;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[24];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			// write discharge mode
			pTuner->R828_Arry[23] = (pTuner->R828_Arry[23] & 0xFB) | (pTuner->SysFreq_Info1.MIXER_TOP & 0x04);
			pTuner->R828_I2C.RegAddr = 0x1C;
			pTuner->R828_I2C.Data = pTuner->R828_Arry[23];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			// LNA discharge current
			pTuner->R828_Arry[25]  = (pTuner->R828_Arry[25] & 0xE0) | pTuner->SysFreq_Info1.LNA_DISCHARGE;
			pTuner->R828_I2C.RegAddr  = 0x1E;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[25];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			// agc clk 1Khz, external det1 cap 1u
			pTuner->R828_Arry[21]  = (pTuner->R828_Arry[21] & 0xCF) | 0x00;
			pTuner->R828_I2C.RegAddr  = 0x1A;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[21];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
			
			pTuner->R828_Arry[11]  = (pTuner->R828_Arry[11] & 0xFB) | 0x00;
			pTuner->R828_I2C.RegAddr  = 0x10;
			pTuner->R828_I2C.Data     = pTuner->R828_Arry[11];
			if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
				return RT_Fail;
		}
	}
	
	return RT_Success;
	
}

R828_ErrCode R828_Standby(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_LoopThrough_Type R828_LoopSwitch)
{
	if(R828_LoopSwitch == LOOP_THROUGH)
	{
		pTuner->R828_I2C.RegAddr = 0x06;
		pTuner->R828_I2C.Data    = 0xB1;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		pTuner->R828_I2C.RegAddr = 0x05;
		pTuner->R828_I2C.Data = 0x03;
		
		
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
	}
	else
	{
		pTuner->R828_I2C.RegAddr = 0x05;
		pTuner->R828_I2C.Data    = 0xA3;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		pTuner->R828_I2C.RegAddr = 0x06;
		pTuner->R828_I2C.Data    = 0xB1;
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
	}
	
	pTuner->R828_I2C.RegAddr = 0x07;
	pTuner->R828_I2C.Data    = 0x3A;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x08;
	pTuner->R828_I2C.Data    = 0x40;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x09;
	pTuner->R828_I2C.Data    = 0xC0;   //polyfilter off
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x0A;
	pTuner->R828_I2C.Data    = 0x36;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x0C;
	pTuner->R828_I2C.Data    = 0x35;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x0F;
	pTuner->R828_I2C.Data    = /*0x78*/0x68;	// Enable CLK_OUT
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x11;
	pTuner->R828_I2C.Data    = 0x03;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x17;
	pTuner->R828_I2C.Data    = 0xF4;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	pTuner->R828_I2C.RegAddr = 0x19;
	pTuner->R828_I2C.Data    = 0x0C;
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	
	return RT_Success;
}

R828_ErrCode R828_GetRfGain(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, R828_RF_Gain_Info *pR828_rf_gain)
{
	
	pTuner->R828_I2C_Len.RegAddr = 0x00;
	pTuner->R828_I2C_Len.Len     = 4;
	if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
		return RT_Fail;
	
	pR828_rf_gain->RF_gain1 = (pTuner->R828_I2C_Len.Data[3] & 0x0F);
	pR828_rf_gain->RF_gain2 = ((pTuner->R828_I2C_Len.Data[3] & 0xF0) >> 4);
	pR828_rf_gain->RF_gain_comb = pR828_rf_gain->RF_gain1*2 + pR828_rf_gain->RF_gain2;
	
    return RT_Success;
}

/* measured with a Racal 6103E GSM test set at 928 MHz with -60 dBm
 * input power, for raw results see:
 * http://steve-m.de/projects/rtl-sdr/gain_measurement/r820t/
 */

#define VGA_BASE_GAIN	-47
static const int r820t_vga_gain_steps[] = {
	 0, 26, 26, 30, 42, 35, 24, 13, 14, 32, 36, 34, 35, 37, 35, 36
};

static const int r820t_lna_gain_steps[] = {
	 0,  9, 13, 40, 38, 13, 31, 22, 26, 31, 26, 14, 19,  5, 35, 13	// sum = 335
};

static const int r820t_mixer_gain_steps[] = {
	 0,  5, 10, 10, 19,  9, 10, 25, 17, 10,  8, 16, 13,  6,  3, -8	// sum = 153
};

R828_ErrCode R828_SetRfGain(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int gain)
{
	int i, total_gain = 0;
	uint8_t mix_index = 0, lna_index = 0;
	
	for (i = 0; i < 15; i++) {
		if (total_gain >= gain)
			break;
		
		total_gain += r820t_lna_gain_steps[++lna_index];
		
		if (total_gain >= gain)
			break;
		
		total_gain += r820t_mixer_gain_steps[++mix_index];
	}
	
	/* set LNA gain */
	pTuner->R828_I2C.RegAddr = 0x05;
	pTuner->R828_Arry[0] = (pTuner->R828_Arry[0] & 0xF0) | lna_index;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[0];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	/* set Mixer gain */
	pTuner->R828_I2C.RegAddr = 0x07;
	pTuner->R828_Arry[2] = (pTuner->R828_Arry[2] & 0xF0) | mix_index;
	pTuner->R828_I2C.Data = pTuner->R828_Arry[2];
	if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
		return RT_Fail;
	
	return RT_Success;
}

R828_ErrCode R828_RfGainMode(RTL2832_NAMESPACE::TUNERS_NAMESPACE::r820t* pTuner, int manual)
{
	UINT8 MixerGain;
	UINT8 LnaGain;
	
	MixerGain = 0;
	LnaGain = 0;
	
	if (manual) {
		//LNA auto off
		pTuner->R828_I2C.RegAddr = 0x05;
		pTuner->R828_Arry[0] = pTuner->R828_Arry[0] | 0x10;
		pTuner->R828_I2C.Data = pTuner->R828_Arry[0];
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		//Mixer auto off
		pTuner->R828_I2C.RegAddr = 0x07;
		pTuner->R828_Arry[2] = pTuner->R828_Arry[2] & 0xEF;
		pTuner->R828_I2C.Data = pTuner->R828_Arry[2];
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		pTuner->R828_I2C_Len.RegAddr = 0x00;
		pTuner->R828_I2C_Len.Len     = 4;
		if(I2C_Read_Len(pTuner, &pTuner->R828_I2C_Len) != RT_Success)
			return RT_Fail;
		
		/* set fixed VGA gain for now (16.3 dB) */
		pTuner->R828_I2C.RegAddr = 0x0C;
		pTuner->R828_Arry[7]    = (pTuner->R828_Arry[7] & 0x60) | 0x08;
		pTuner->R828_I2C.Data    = pTuner->R828_Arry[7];
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		
	} else {
	    //LNA
		pTuner->R828_I2C.RegAddr = 0x05;
		pTuner->R828_Arry[0] = pTuner->R828_Arry[0] & 0xEF;
		pTuner->R828_I2C.Data = pTuner->R828_Arry[0];
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		//Mixer
		pTuner->R828_I2C.RegAddr = 0x07;
		pTuner->R828_Arry[2] = pTuner->R828_Arry[2] | 0x10;
		pTuner->R828_I2C.Data = pTuner->R828_Arry[2];
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
		
		/* set fixed VGA gain for now (26.5 dB) */
		pTuner->R828_I2C.RegAddr = 0x0C;
		pTuner->R828_Arry[7]    = (pTuner->R828_Arry[7] & 0x60) | 0x0B;
		pTuner->R828_I2C.Data    = pTuner->R828_Arry[7];
		if(I2C_Write(pTuner, &pTuner->R828_I2C) != RT_Success)
			return RT_Fail;
	}
	
    return RT_Success;
}

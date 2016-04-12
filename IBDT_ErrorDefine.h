#if !defined(_INCLUDE_IBDT_ERRORDEFINE_H__)
#define _INCLUDE_IBDT_ERRORDEFINE_H__

#pragma once

#include <string>
enum IBDT_ErrorCode
{
	IBDT_OK = 0,

	IBDT_E_InvalidParam = 1,
	IBDT_E_InitIBDeviceFailed,
	IBDT_E_ConnectServerFailed,
	IBDT_E_RegMRFailed,
	IBDT_E_CreateQPFailed,
	IBDT_E_SyncDataFailed,
	IBDT_E_ConnectQPFailed,
	IBDT_E_PostReceviceFailed,
	IBDT_E_SyncReadyFailed,
	IBDT_E_SendFileInfoFailed,
	IBDT_E_GetFileSizeFailed,
	IBDT_E_TransferFileFailed	
};

inline std::string GetIBDTErrorCodeHint(int nErrorCode, std::string strAppend = "")
{
  std::string strHint;
  strHint += "ErrorCode:%1 ";
  strHint += nErrorCode;
  switch(nErrorCode)
  {
  case IBDT_OK:
	  strHint += "Succeed";
	  break;
  case IBDT_E_InvalidParam:
	  strHint += "Invalid Param";
	  break;
  case IBDT_E_InitIBDeviceFailed:
	  strHint += "Init IB Device Failed";
	  break;
  case IBDT_E_ConnectServerFailed:
	  strHint += "Connect with Server failed";
	  break;
  case IBDT_E_RegMRFailed:
	  strHint += "Register Memory failed";
	  break;
  case IBDT_E_CreateQPFailed:
	  strHint += "Create QP Failed";
	  break;
  case IBDT_E_SyncDataFailed:
	  strHint += "Sync Connect Data With Server Failed";
	  break;
  case IBDT_E_ConnectQPFailed:
	  strHint += "Connect QP Failed";
	  break;
  case IBDT_E_PostReceviceFailed:
	  strHint += "Post Receive Request Failed";
	  break;
  case IBDT_E_SyncReadyFailed:
	  strHint += "Sync Ready With Server Failed";
	  break;
  case IBDT_E_SendFileInfoFailed:
	  strHint += "Send File Info Failed";
	  break;
  case IBDT_E_GetFileSizeFailed:
	  strHint += "Get File Size In Server Side Failed";
	  break;
  case IBDT_E_TransferFileFailed:
	  strHint += "Transfer File Failed";
	  break;
  default:
	  strHint += "None";
	  break;
  }
  if (!strAppend.empty())
  {
	strHint += " [";
	strHint += strAppend;
	strHint += "]";
  }
  return strHint;
}

#endif // _INCLUDE_IBDT_ERRORDEFINE_H__

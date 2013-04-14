#include "RtcOkiMsm6242rs.h"
#include "FMEM.H"
#include "FELLOW.H"
#include "rtc.h"

bool rtc_enabled = false;
RtcOkiMsm6242rs rtc;

UBY rtcReadByte(ULO address)
{
  UWO result = rtc.read(address);
  UBY byte_result = (UBY) ((address & 1) ? result : (result>>8));

#ifdef RTC_LOG
  fellowAddLog("RTC Byte Read: %.8X, returned %.2X\n", address, byte_result);
#endif

  return byte_result;
}

UWO rtcReadWord(ULO address)
{
  UWO result = rtc.read(address);

#ifdef RTC_LOG
  fellowAddLog("RTC Word Read: %.8X, returned %.4X\n", address, result);
#endif

  return result;
}

ULO rtcReadLong(ULO address)
{
  ULO w1 = (ULO) rtc.read(address);
  ULO w2 = (ULO) rtc.read(address+2);
  ULO result = (w1 << 16) | w2;

#ifdef RTC_LOG
  fellowAddLog("RTC Long Read: %.8X, returned %.8X\n", address, result);
#endif

  return result;
}

void rtcWriteByte(UBY data, ULO address)
{
  rtc.write(data, address);

#ifdef RTC_LOG
  fellowAddLog("RTC Byte Write: %.8X %.2X\n", address, data);
#endif
}

void rtcWriteWord(UWO data, ULO address)
{
  rtc.write(data, address);

#ifdef RTC_LOG
  fellowAddLog("RTC Word Write: %.8X %.4X\n", address, data);
#endif
}

void rtcWriteLong(ULO data, ULO address)
{
  rtc.write(data, address);
  rtc.write(data, address + 2);

#ifdef RTC_LOG
  fellowAddLog("RTC Long Write: %.8X %.8X\n", address, data);
#endif
}

bool rtcSetEnabled(bool enabled)
{
  bool needreset = (rtc_enabled != enabled);
  rtc_enabled = enabled;
  return needreset;
}

bool rtcGetEnabled(void)
{
  return rtc_enabled;
}

void rtcMap(void)
{
  if (rtcGetEnabled())
  {
    memoryBankSet(rtcReadByte,
      rtcReadWord,
      rtcReadLong,
      rtcWriteByte, 
      rtcWriteWord, 
      rtcWriteLong,
      NULL, 
      0xdc, 
      0,
      FALSE);

#ifdef RTC_LOG
  fellowAddLog("Mapped RTC at $DC0000\n");
#endif
  }
}

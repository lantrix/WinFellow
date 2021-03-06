/*=========================================================================*/
/* Fellow                                                                  */
/* Keeps track of sprite state                                             */
/*                                                                         */
/* Authors: Petter Schau                                                   */
/*                                                                         */
/*                                                                         */
/* Copyright (C) 1991, 1992, 1996 Free Software Foundation, Inc.           */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify    */
/* it under the terms of the GNU General Public License as published by    */
/* the Free Software Foundation; either version 2, or (at your option)     */
/* any later version.                                                      */
/*                                                                         */
/* This program is distributed in the hope that it will be useful,         */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/* GNU General Public License for more details.                            */
/*                                                                         */
/* You should have received a copy of the GNU General Public License       */
/* along with this program; if not, write to the Free Software Foundation, */
/* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.          */
/*=========================================================================*/

#include "DEFS.H"

#include "chipset.h"
#include "bus.h"
#include "graph.h"
#include "draw.h"
#include "fmem.h"
#include "fileops.h"
#include "SpriteRegisters.h"
#include "SpriteP2CDecoder.h"
#include "SpriteMerger.h"
#include "CycleExactSprites.h"

#include "BitplaneUtility.h"
#include "Graphics.h"

bool CycleExactSprites::Is16Color(ULO spriteNo)
{
  ULO evenSpriteNo = spriteNo & 0xe;
  return SpriteState[evenSpriteNo].attached || SpriteState[evenSpriteNo + 1].attached;
}

void CycleExactSprites::Arm(ULO sprite_number)
{
  // Actually this is kind of wrong since each sprite serialises independently
  bool is16Color = Is16Color(sprite_number);
  if (is16Color && (sprite_number & 1) == 0) // Only arm the odd sprite
  {
    Arm(sprite_number + 1);
    SpriteState[sprite_number].armed = false;
    return;
  }
  SpriteState[sprite_number].armed = true;
  SpriteState[sprite_number].pixels_output = 0;
  SpriteState[sprite_number].dat_hold[0].w = sprite_registers.sprdata[sprite_number];
  SpriteState[sprite_number].dat_hold[1].w = sprite_registers.sprdatb[sprite_number];
  if (is16Color)
  {
    SpriteState[sprite_number].dat_hold[2].w = sprite_registers.sprdata[sprite_number - 1];
    SpriteState[sprite_number].dat_hold[3].w = sprite_registers.sprdatb[sprite_number - 1];
    SpriteP2CDecoder::Decode16(&(SpriteState[sprite_number].dat_decoded.blu[0].l), SpriteState[sprite_number].dat_hold[2].w, SpriteState[sprite_number].dat_hold[3].w, SpriteState[sprite_number].dat_hold[0].w, SpriteState[sprite_number].dat_hold[1].w);
  }
  else
  {
    SpriteP2CDecoder::Decode4(sprite_number, &(SpriteState[sprite_number].dat_decoded.blu[0].l), SpriteState[sprite_number].dat_hold[1].w, SpriteState[sprite_number].dat_hold[0].w);
  }
}

void CycleExactSprites::MergeLores(ULO spriteNo, ULO source_pixel_index, ULO pixel_index, ULO pixel_count)
{
  UBY *playfield = &GraphicsContext.Planar2ChunkyDecoder.GetOddPlayfield()[pixel_index];
  UBY *sprite_data = &SpriteState[spriteNo].dat_decoded.barray[source_pixel_index];

  SpriteMerger::MergeLores(spriteNo, playfield, sprite_data, pixel_count);
}

void CycleExactSprites::MergeHires(ULO spriteNo, ULO source_pixel_index, ULO pixel_index, ULO pixel_count)
{
  UBY *playfield = &GraphicsContext.Planar2ChunkyDecoder.GetOddPlayfield()[pixel_index];
  UBY *sprite_data = &SpriteState[spriteNo].dat_decoded.barray[source_pixel_index];

  SpriteMerger::MergeHires(spriteNo, playfield, sprite_data, pixel_count);
}

void CycleExactSprites::MergeHam(ULO spriteNo, ULO source_pixel_index, ULO pixel_index, ULO pixel_count)
{
  UBY *playfield = &GraphicsContext.Planar2ChunkyDecoder.GetOddPlayfield()[pixel_index];
  UBY *ham_sprites_playfield = &GraphicsContext.Planar2ChunkyDecoder.GetHamSpritesPlayfield()[pixel_index];
  UBY *sprite_data = &SpriteState[spriteNo].dat_decoded.barray[source_pixel_index];

  SpriteMerger::MergeHam(spriteNo, playfield, ham_sprites_playfield, sprite_data, pixel_count);
}

void CycleExactSprites::Merge(ULO spriteNo, ULO source_pixel_index, ULO pixel_index, ULO pixel_count)
{
  if (BitplaneUtility::IsLores())
  {
    MergeLores(spriteNo, source_pixel_index, pixel_index, pixel_count);
  }
  else
  {
    MergeHires(spriteNo, source_pixel_index, pixel_index, pixel_count);
  }
}

bool CycleExactSprites::InRange(ULO spriteNo, ULO startCylinder, ULO cylinderCount)
{
  // Comparison happens at x, sprite image visible one cylinder later
  ULO visible_at_cylinder = SpriteState[spriteNo].x + 1;

  return (visible_at_cylinder >= startCylinder)
    && (visible_at_cylinder < (startCylinder + cylinderCount));
}

void CycleExactSprites::OutputSprite(ULO spriteNo, ULO startCylinder, ULO cylinderCount)
{
  if (SpriteState[spriteNo].armed)
  {
    ULO pixel_index = 0;

    // Look for start of sprite output
    if (!SpriteState[spriteNo].serializing && InRange(spriteNo, startCylinder, cylinderCount))
    {
      SpriteState[spriteNo].serializing = true;
      pixel_index = SpriteState[spriteNo].x + 1 - startCylinder;
    }
    if (SpriteState[spriteNo].serializing)
    {
      // Some output of the sprite will occur in this range.
      ULO pixel_count = cylinderCount - pixel_index;
      ULO pixelsLeft = 16 - SpriteState[spriteNo].pixels_output;
      if (pixel_count > pixelsLeft)
      {
        pixel_count = pixelsLeft;
      }

      if (BitplaneUtility::IsHires())
      {
        pixel_index = pixel_index*2;  // Hires coordinates
      }

      Merge(spriteNo, SpriteState[spriteNo].pixels_output, pixel_index, pixel_count);
      SpriteState[spriteNo].pixels_output += pixel_count;	
      SpriteState[spriteNo].serializing = (SpriteState[spriteNo].pixels_output < 16);
    }
  }
}

void CycleExactSprites::OutputSprites(ULO startCylinder, ULO cylinderCount)
{
  for (ULO spriteNo = 0; spriteNo < 8; spriteNo++)
  {
    OutputSprite(spriteNo, startCylinder, cylinderCount);
  }
}

/* IO Register handlers */

/* SPRXPT - $dff120 to $dff13e */

void CycleExactSprites::NotifySprpthChanged(UWO data, unsigned int sprite_number)
{
  // Nothing
}

void CycleExactSprites::NotifySprptlChanged(UWO data, unsigned int sprite_number)
{
  SpriteState[sprite_number].DMAState.state = SPRITE_DMA_STATE_READ_CONTROL;
}

/* SPRXPOS - $dff140 to $dff178 */

void CycleExactSprites::NotifySprposChanged(UWO data, unsigned int sprite_number)
{
  SpriteState[sprite_number].x = (SpriteState[sprite_number].x & 1) | ((data & 0xff) << 1);
  SpriteState[sprite_number].DMAState.y_first = (SpriteState[sprite_number].DMAState.y_first & 0x100) | ((data & 0xff00) >> 8);
  SpriteState[sprite_number].armed = false;
}

/* SPRXCTL $dff142 to $dff17a */

void CycleExactSprites::NotifySprctlChanged(UWO data, unsigned int sprite_number)
{
  // retrieve the rest of the horizontal and vertical position bits
  SpriteState[sprite_number].x = (SpriteState[sprite_number].x & 0x1fe) | (data & 0x1);
  SpriteState[sprite_number].DMAState.y_first = (SpriteState[sprite_number].DMAState.y_first & 0x0ff) | ((data & 0x4) << 6);

  // attach bit only applies when having an odd sprite
  if (sprite_number & 1)
  {
    SpriteState[sprite_number - 1].attached = !!(data & 0x80);
  }
  SpriteState[sprite_number].attached = !!(data & 0x80);
  SpriteState[sprite_number].DMAState.y_last = ((data & 0xff00) >> 8) | ((data & 0x2) << 7);
  SpriteState[sprite_number].armed = false;
}

/* SPRXDATA $dff144 to $dff17c */

void CycleExactSprites::NotifySprdataChanged(UWO data, unsigned int sprite_number)
{
  Arm(sprite_number);
}

/* SPRXDATB $dff146 to $dff17e */

void CycleExactSprites::NotifySprdatbChanged(UWO data, unsigned int sprite_number)
{
  SpriteState[sprite_number].armed = false;
}

/* Sprite State Machine */

UWO CycleExactSprites::ReadWord(ULO spriteNo)
{
  UWO data = chipmemReadWord(sprite_registers.sprpt[spriteNo]);
  sprite_registers.sprpt[spriteNo] = chipsetMaskPtr(sprite_registers.sprpt[spriteNo] + 2);
  return data;
}

void CycleExactSprites::ReadControlWords(ULO spriteNo)
{
  UWO pos = ReadWord(spriteNo);
  UWO ctl = ReadWord(spriteNo);
  wsprxpos(pos, 0x140 + spriteNo*8);
  wsprxctl(ctl, 0x142 + spriteNo*8);
}

void CycleExactSprites::ReadDataWords(ULO spriteNo)
{
  wsprxdatb(ReadWord(spriteNo), 0x146 + spriteNo*8);
  wsprxdata(ReadWord(spriteNo), 0x144 + spriteNo*8);
}

bool CycleExactSprites::IsFirstLine(ULO spriteNo, ULO rasterY)
{
  return (rasterY >= 24) && (rasterY == SpriteState[spriteNo].DMAState.y_first);
}

bool CycleExactSprites::IsAboveFirstLine(ULO spriteNo, ULO rasterY)
{
  return rasterY > SpriteState[spriteNo].DMAState.y_first;
}

bool CycleExactSprites::IsLastLine(ULO spriteNo, ULO rasterY)
{
  return rasterY == SpriteState[spriteNo].DMAState.y_last;
}

void CycleExactSprites::DMAReadControl(ULO spriteNo, ULO rasterY)
{
  ReadControlWords(spriteNo);

  if (IsFirstLine(spriteNo, rasterY))
  {
    SpriteState[spriteNo].DMAState.state = SPRITE_DMA_STATE_READ_DATA;	
  }
  else
  {
    SpriteState[spriteNo].DMAState.state = SPRITE_DMA_STATE_WAITING_FOR_FIRST_LINE;
  }
}

void CycleExactSprites::DMAWaitingForFirstLine(ULO spriteNo, ULO rasterY)
{
  if (IsFirstLine(spriteNo, rasterY))
  {
    ReadDataWords(spriteNo);
    if (IsLastLine(spriteNo, rasterY))
    {
      SpriteState[spriteNo].DMAState.state = SPRITE_DMA_STATE_READ_CONTROL;
    }
    else
    {
      SpriteState[spriteNo].DMAState.state = SPRITE_DMA_STATE_READ_DATA;
    }
  }
}

void CycleExactSprites::DMAReadData(ULO spriteNo, ULO rasterY)
{
  if (!IsLastLine(spriteNo, rasterY))
  {
    ReadDataWords(spriteNo);
  }
  else
  {
    ReadControlWords(spriteNo);
    if (IsFirstLine(spriteNo, rasterY))
    {
      SpriteState[spriteNo].DMAState.state = SPRITE_DMA_STATE_READ_DATA;
    }
    else
    {
      SpriteState[spriteNo].DMAState.state = SPRITE_DMA_STATE_WAITING_FOR_FIRST_LINE;
    }
  }
}

void CycleExactSprites::DMAHandler(ULO rasterY)
{  
  if ((dmacon & 0x20) == 0 || rasterY < 0x18)
  {
    return;
  }

  rasterY++; // Do DMA for next line

  ULO spriteNo = 0;
  while (spriteNo < 8) 
  {
    switch(SpriteState[spriteNo].DMAState.state) 
    {
      case SPRITE_DMA_STATE_READ_CONTROL:
	DMAReadControl(spriteNo, rasterY);
	break;
      case SPRITE_DMA_STATE_WAITING_FOR_FIRST_LINE:
	DMAWaitingForFirstLine(spriteNo, rasterY);
        break;
      case SPRITE_DMA_STATE_READ_DATA:
	DMAReadData(spriteNo, rasterY);
  	break;
    }
    spriteNo++;
  }
}

/* Module management */

void CycleExactSprites::EndOfLine(ULO rasterY)
{
  for (ULO i = 0; i < 8; ++i)
  {
    SpriteState[i].serializing = false;
  }
  DMAHandler(rasterY);
}

void CycleExactSprites::EndOfFrame()
{
  for (ULO spriteNo = 0; spriteNo < 8; ++spriteNo)
  {
    SpriteState[spriteNo].DMAState.state = SPRITE_DMA_STATE_READ_CONTROL;
  }
}

void CycleExactSprites::ClearState()
{
  memset(SpriteState, 0, sizeof(Sprite)*8);
}

void CycleExactSprites::HardReset()
{
}

void CycleExactSprites::EmulationStart()
{
  ClearState(); // ??????
}

void CycleExactSprites::EmulationStop()
{
}

CycleExactSprites::CycleExactSprites()
  : Sprites()
{
}

CycleExactSprites::~CycleExactSprites()
{
}

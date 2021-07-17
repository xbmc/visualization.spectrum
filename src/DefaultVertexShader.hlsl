/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

struct VS_OUT
{
  float4 pos : SV_POSITION;
  float4 col : COLOR;
};

cbuffer cbViewProj : register(b0)
{
  float4x4 view;
  float4x4 proj;
};

cbuffer cbWorld : register(b1)
{
  float4x4 world;
};

VS_OUT main(float4 pos : POSITION, float4 col : COLOR)
{
  VS_OUT r = (VS_OUT)0;
  r.pos = mul(  pos, world);
  r.pos = mul(r.pos, view);
  r.pos = mul(r.pos, proj);
  r.col = col;
  return r;
}
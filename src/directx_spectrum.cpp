/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 *  Wed May 24 10:49:37 CDT 2000
 *  Fixes to threading/context creation for the nVidia X4 drivers by
 *  Christian Zander <phoenix@minion.de>
 */

/*
 *  Ported to XBMC by d4rk
 *  Also added 'm_hSpeed' to animate transition between bar heights
 */

#include <kodi/addon-instance/Visualization.h>
#include <math.h>
#include <d3d11_1.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <stdio.h>

#define NUM_BANDS 16
#define NUM_VERTICIES 36

using namespace DirectX;
using namespace DirectX::PackedVector;

// Include the precompiled shader code.
namespace
{
  #include "DefaultPixelShader.inc"
  #include "DefaultVertexShader.inc"
}

typedef struct
{
  XMFLOAT3 pos;
  XMFLOAT4 col;
} Vertex_t;

typedef struct
{
  XMFLOAT4X4 view;
  XMFLOAT4X4 proj;
} cbViewProj;

typedef struct
{
  XMFLOAT4X4 world;
} cbWorld;

#define VERTEX_FORMAT (D3DFVF_XYZ | D3DFVF_DIFFUSE)

class CVisualizationSpectrum
  : public kodi::addon::CAddonBase,
    public kodi::addon::CInstanceVisualization
{
public:
  CVisualizationSpectrum();
  virtual ~CVisualizationSpectrum();

  virtual bool Start(int channels, int samplesPerSec, int bitsPerSample, std::string songName) override;
  virtual void Render() override;
  virtual void AudioData(const float* audioData, int audioDataLength, float *freqData, int freqDataLength) override;
  virtual ADDON_STATUS SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue) override;

private:
  void SetBarHeightSetting(int settingValue);
  void SetSpeedSetting(int settingValue);
  void SetModeSetting(int settingValue);

  float heights[16][16], cHeights[16][16], m_scale;
  DWORD m_mode; // D3DFILL_SOLID;
  float m_y_angle, m_y_speed;
  float m_x_angle, m_x_speed;
  float m_z_angle, m_z_speed;
  float m_hSpeed;

  void draw_vertex(Vertex_t * pVertex, float x, float y, float z, XMFLOAT4 color);
  int draw_rectangle(Vertex_t * verts, float x1, float y1, float z1, float x2, float y2, float z2, XMFLOAT4 color);
  void draw_bar(float x_offset, float z_offset, float height, float red, float green, float blue);
  void draw_bars(void);
  bool init_renderer_objs();

  ID3D11Device* m_device;
  ID3D11DeviceContext* m_context;
  ID3D11VertexShader* m_vShader;
  ID3D11PixelShader* m_pShader;
  ID3D11InputLayout* m_inputLayout;
  ID3D11Buffer* m_vBuffer;
  ID3D11Buffer* m_cViewProj;
  ID3D11Buffer* m_cWorld;
  ID3D11RasterizerState* m_rsStateSolid;
  ID3D11RasterizerState* m_rsStateWire;
  ID3D11BlendState* m_omBlend;
  ID3D11DepthStencilState* m_omDepth;
};

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
CVisualizationSpectrum::CVisualizationSpectrum()
  : m_mode(3),
    m_y_angle(45.0f),
    m_y_speed(0.5f),
    m_x_angle(20.0f),
    m_x_speed(0.0f),
    m_z_angle(0.0f),
    m_z_speed(0.0f),
    m_hSpeed(0.05f),
    m_device(nullptr),
    m_context(nullptr),
    m_vShader(nullptr),
    m_pShader(nullptr),
    m_inputLayout(nullptr),
    m_vBuffer(nullptr),
    m_cViewProj(nullptr),
    m_cWorld(nullptr),
    m_rsStateSolid(nullptr),
    m_rsStateWire(nullptr),
    m_omBlend(nullptr),
    m_omDepth(nullptr)
{
  m_context = (ID3D11DeviceContext*)Device();
  m_context->GetDevice(&m_device);

  SetBarHeightSetting(kodi::GetSettingInt("bar_height"));
  SetSpeedSetting(kodi::GetSettingInt("speed"));
  SetModeSetting(kodi::GetSettingInt("mode"));

  if (!init_renderer_objs())
    kodi::Log(ADDON_LOG_ERROR, "Failed to init DirectX");
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
CVisualizationSpectrum::~CVisualizationSpectrum()
{
  if (m_cViewProj)
    m_cViewProj->Release();
  if (m_cWorld)
    m_cWorld->Release();
  if (m_rsStateSolid)
    m_rsStateSolid->Release();
  if (m_rsStateWire)
    m_rsStateWire->Release();
  if (m_omBlend)
    m_omBlend->Release();
  if (m_omDepth)
    m_omDepth->Release();
  if (m_vBuffer)
    m_vBuffer->Release();
  if (m_inputLayout)
    m_inputLayout->Release();
  if (m_vShader)
    m_vShader->Release();
  if (m_pShader)
    m_pShader->Release();
  if (m_device)
    m_device->Release();
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
void CVisualizationSpectrum::Render()
{
  bool configured = true; //FALSE;

  float factors[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  m_context->OMSetBlendState(m_omBlend, factors, 0xFFFFFFFF);
  m_context->OMSetDepthStencilState(m_omDepth, 0);
  switch (m_mode)
  {
  case 1: // D3DFILL_POINT:
  case 2: // D3DFILL_WIREFRAME:
    m_context->RSSetState(m_rsStateWire);
    break;
  case 3: // D3DFILL_SOLID:
    m_context->RSSetState(m_rsStateSolid);
    break;
  }

  unsigned stride = sizeof(Vertex_t), offset = 0;
  m_context->IASetVertexBuffers(0, 1, &m_vBuffer, &stride, &offset);
  m_context->IASetInputLayout(m_inputLayout);
  m_context->VSSetShader(m_vShader, 0, 0);
  m_context->VSSetConstantBuffers(0, 1, &m_cViewProj);
  m_context->VSSetConstantBuffers(1, 1, &m_cWorld);
  m_context->PSSetShader(m_pShader, 0, 0);

  if(configured)
  {
    m_x_angle += m_x_speed;
    if (m_x_angle >= 360.0f)
      m_x_angle -= 360.0f;

    m_y_angle += m_y_speed;
    if (m_y_angle >= 360.0f)
      m_y_angle -= 360.0f;

    m_z_angle += m_z_speed;
    if (m_z_angle >= 360.0f)
      m_z_angle -= 360.0f;

    D3D11_MAPPED_SUBRESOURCE res;
    if (S_OK == m_context->Map(m_cWorld, 0, D3D11_MAP_WRITE_DISCARD, 0, &res))
    {
      cbWorld *cWorld = (cbWorld*)res.pData;
      XMMATRIX
        matRotationX = XMMatrixRotationX(-XMConvertToRadians(m_x_angle)),
        matRotationY = XMMatrixRotationY(-XMConvertToRadians(m_y_angle)),
        matRotationZ = XMMatrixRotationZ(XMConvertToRadians(m_z_angle)),
        matTranslation = XMMatrixTranslation(0.0f, -0.5f, 5.0f),
        matWorld = matRotationZ * matRotationY * matRotationX * matTranslation;
      XMStoreFloat4x4(&cWorld->world, XMMatrixTranspose(matWorld));

      m_context->Unmap(m_cWorld, 0);
    }

    draw_bars();
  }
}

bool CVisualizationSpectrum::Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, std::string szSongName)
{
  int x, y;

  for(x = 0; x < 16; x++)
  {
    for(y = 0; y < 16; y++)
    {
      cHeights[y][x] = 0.0f;
    }
  }

  m_scale = 1.0f / log(256.0f);

  m_x_speed = 0.0f;
  m_y_speed = 0.5f;
  m_z_speed = 0.0f;
  m_x_angle = 20.0f;
  m_y_angle = 45.0f;
  m_z_angle = 0.0f;

  return true;
}

void CVisualizationSpectrum::AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
{
  int i,c;
  int y=0;
  float val;

  int xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};

  for(y = 15; y > 0; y--)
  {
    for(i = 0; i < 16; i++)
    {
      heights[y][i] = heights[y - 1][i];
    }
  }

  for(i = 0; i < NUM_BANDS; i++)
  {
    for(c = xscale[i], y = 0; c < xscale[i + 1]; c++)
    {
      if (c<iAudioDataLength)
      {
        if((int)(pAudioData[c] * (0x07fff+.5f) > y))
          y = (int)(pAudioData[c] * (0x07fff+.5f));
      }
      else
        continue;
    }
    y >>= 7;
    if(y > 0)
      val = (logf((float)y) * m_scale);
    else
      val = 0;
    heights[0][i] = val;
  }
}

void CVisualizationSpectrum::SetBarHeightSetting(int settingValue)
{
  switch (settingValue)
  {
  case 1://standard
    m_scale = 1.f / log(256.f);
    break;

  case 2://big
    m_scale = 2.f / log(256.f);
    break;

  case 3://real big
    m_scale = 3.f / log(256.f);
    break;

  case 4://unused
    m_scale = 0.33f / log(256.f);
    break;

  case 0://small
  default:
    m_scale = 0.5f / log(256.f);
    break;
  }
}

void CVisualizationSpectrum::SetSpeedSetting(int settingValue)
{
  switch (settingValue)
  {
  case 1:
    m_hSpeed = 0.025f;
    break;

  case 2:
    m_hSpeed = 0.0125f;
    break;

  case 3:
    m_hSpeed = 0.1f;
    break;

  case 4:
    m_hSpeed = 0.2f;
    break;

  case 0:
  default:
    m_hSpeed = 0.05f;
    break;
  }
}

void CVisualizationSpectrum::SetModeSetting(int settingValue)
{
  switch (settingValue)
  {
  case 1:
    m_mode = 2; // D3DFILL_WIREFRAME;
    break;

  case 2:
    m_mode = 1; // D3DFILL_POINT;
    break;

  case 0:
  default:
    m_mode = 3; // D3DFILL_SOLID;
    break;
  }
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS CVisualizationSpectrum::SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue)
{
  if (settingName.empty() || settingValue.empty())
    return ADDON_STATUS_UNKNOWN;

  if (settingName == "bar_height")
  {
    SetBarHeightSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }
  else if (settingName == "speed")
  {
    SetSpeedSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }
  else if (settingName == "mode")
  {
    SetModeSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }

  return ADDON_STATUS_UNKNOWN;
}

void CVisualizationSpectrum::draw_vertex(Vertex_t * pVertex, float x, float y, float z, XMFLOAT4 color)
{
  pVertex->col = XMFLOAT4(color);
  pVertex->pos = XMFLOAT3(x, y, z);
}

int CVisualizationSpectrum::draw_rectangle(Vertex_t * verts, float x1, float y1, float z1, float x2, float y2, float z2, XMFLOAT4 color)
{
  if(y1 == y2)
  {
    draw_vertex(&verts[0], x1, y1, z1, color);
    draw_vertex(&verts[1], x2, y1, z1, color);
    draw_vertex(&verts[2], x2, y2, z2, color);

    draw_vertex(&verts[3], x2, y2, z2, color);
    draw_vertex(&verts[4], x1, y2, z2, color);
    draw_vertex(&verts[5], x1, y1, z1, color);
  }
  else
  {
    draw_vertex(&verts[0], x1, y1, z1, color);
    draw_vertex(&verts[1], x2, y1, z2, color);
    draw_vertex(&verts[2], x2, y2, z2, color);

    draw_vertex(&verts[3], x2, y2, z2, color);
    draw_vertex(&verts[4], x1, y2, z1, color);
    draw_vertex(&verts[5], x1, y1, z1, color);
  }
  return 6;
}

void CVisualizationSpectrum::draw_bar(float x_offset, float z_offset, float height, float red, float green, float blue)
{
  Vertex_t  verts[NUM_VERTICIES];
  int verts_idx = 0;

  float width = 0.1f;
  XMFLOAT4 color;

  if (1 == m_mode /*== D3DFILL_POINT*/)
    color = XMFLOAT4(0.2f, 1.0f, 0.2f, 1.0f);

  if (1 != m_mode /*!= D3DFILL_POINT*/)
  {
    color = XMFLOAT4(red, green, blue, 1.0f);
    verts_idx += draw_rectangle(&verts[verts_idx], x_offset, height, z_offset, x_offset + width, height, z_offset + 0.1f, color);
  }
  verts_idx += draw_rectangle(&verts[verts_idx], x_offset, 0.0f, z_offset, x_offset + width, 0.0f, z_offset + 0.1f, color);

  if (1 != m_mode /*!= D3DFILL_POINT*/)
  {
    color = XMFLOAT4(0.5f * red, 0.5f * green, 0.5f * blue, 1.0f);
    verts_idx += draw_rectangle(&verts[verts_idx], x_offset, 0.0f, z_offset + 0.1f, x_offset + width, height, z_offset + 0.1f, color);
  }
  verts_idx += draw_rectangle(&verts[verts_idx], x_offset, 0.0f, z_offset, x_offset + width, height, z_offset, color);

  if (1 != m_mode /*!= D3DFILL_POINT*/)
  {
    color = XMFLOAT4(0.25f * red, 0.25f * green, 0.25f * blue, 1.0f);
    verts_idx += draw_rectangle(&verts[verts_idx], x_offset, 0.0f, z_offset , x_offset, height, z_offset + 0.1f, color);
  }
  verts_idx += draw_rectangle(&verts[verts_idx], x_offset + width, 0.0f, z_offset , x_offset + width, height, z_offset + 0.1f, color);

  D3D11_MAPPED_SUBRESOURCE res;
  if (S_OK == m_context->Map(m_vBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res))
  {
    memcpy(res.pData, verts, sizeof(Vertex_t) * NUM_VERTICIES);
    m_context->Unmap(m_vBuffer, 0);
  }

  m_context->IASetPrimitiveTopology(m_mode != 1 /*D3DFILL_POINT*/ ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  m_context->Draw(verts_idx, 0);
}

void CVisualizationSpectrum::draw_bars(void)
{
  int x,y;
  float x_offset, z_offset, r_base, b_base;

  for(y = 0; y < 16; y++)
  {
    z_offset = -1.6f + ((15 - y) * 0.2f);

    b_base = y * (1.0f / 15);
    r_base = 1.0f - b_base;

    for(x = 0; x < 16; x++)
    {
      x_offset = -1.6f + (x * 0.2f);
      if (::fabs(cHeights[y][x]-heights[y][x])>m_hSpeed)
      {
        if (cHeights[y][x]<heights[y][x])
          cHeights[y][x] += m_hSpeed;
        else
          cHeights[y][x] -= m_hSpeed;
      }
      draw_bar(x_offset, z_offset,
               cHeights[y][x], r_base - (x * (r_base / 15.0f)),
               x * (1.0f / 15), b_base);
    }
  }
}

bool CVisualizationSpectrum::init_renderer_objs()
{
  if (S_OK != m_device->CreateVertexShader(DefaultVertexShaderCode, sizeof(DefaultVertexShaderCode), nullptr, &m_vShader))
    return false;

  // Create input layout
  D3D11_INPUT_ELEMENT_DESC layout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };
  if (S_OK != m_device->CreateInputLayout(layout, ARRAYSIZE(layout), DefaultVertexShaderCode, sizeof(DefaultVertexShaderCode), &m_inputLayout))
    return false;

  // Create pixel shader
  if (S_OK != m_device->CreatePixelShader(DefaultPixelShaderCode, sizeof(DefaultPixelShaderCode), nullptr, &m_pShader))
    return false;

  // create buffers
  CD3D11_BUFFER_DESC desc(sizeof(Vertex_t) * NUM_VERTICIES, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
  if (S_OK != m_device->CreateBuffer(&desc, NULL, &m_vBuffer))
    return false;

  desc.ByteWidth = sizeof(cbWorld);
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  if (S_OK != m_device->CreateBuffer(&desc, NULL, &m_cWorld))
    return false;

  cbViewProj cViewProj;
  XMStoreFloat4x4(&cViewProj.view, XMMatrixTranspose(XMMatrixIdentity()));
  XMStoreFloat4x4(&cViewProj.proj, XMMatrixTranspose(XMMatrixPerspectiveOffCenterLH(-1.0f, 1.0f, -1.0f, 1.0f, 1.5f, 10.0f)));

  desc.ByteWidth = sizeof(cbViewProj);
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.CPUAccessFlags = 0;
  D3D11_SUBRESOURCE_DATA initData = { 0 };
  initData.pSysMem = &cViewProj;
  if (S_OK != m_device->CreateBuffer(&desc, &initData, &m_cViewProj))
    return false;

  // create blend state
  D3D11_BLEND_DESC blendState = { 0 };
  ZeroMemory(&blendState, sizeof(D3D11_BLEND_DESC));
  blendState.RenderTarget[0].BlendEnable = true;
  blendState.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE; 
  blendState.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
  blendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  if (S_OK != m_device->CreateBlendState(&blendState, &m_omBlend))
    return false;

  // create depth state
  D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
  ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

  // Set up the description of the stencil state.
  depthStencilDesc.DepthEnable = true;
  depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
  depthStencilDesc.StencilEnable = true;
  depthStencilDesc.StencilReadMask = 0xFF;
  depthStencilDesc.StencilWriteMask = 0xFF;

  // Stencil operations if pixel is front-facing.
  depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
  depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

  // Stencil operations if pixel is back-facing.
  depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
  depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

  if (S_OK != m_device->CreateDepthStencilState(&depthStencilDesc, &m_omDepth))
    return false;

  // create raster states
  D3D11_RASTERIZER_DESC rasterizerState;
  rasterizerState.CullMode = D3D11_CULL_NONE;
  rasterizerState.FillMode = D3D11_FILL_SOLID;
  rasterizerState.FrontCounterClockwise = false;
  rasterizerState.DepthBias = 0;
  rasterizerState.DepthBiasClamp = 0.0f;
  rasterizerState.DepthClipEnable = true;
  rasterizerState.SlopeScaledDepthBias = 0.0f;
  rasterizerState.ScissorEnable = false;
  rasterizerState.MultisampleEnable = false;
  rasterizerState.AntialiasedLineEnable = false;

  if (S_OK != m_device->CreateRasterizerState(&rasterizerState, &m_rsStateSolid))
    return false;

  rasterizerState.FillMode = D3D11_FILL_WIREFRAME;
  if (S_OK != m_device->CreateRasterizerState(&rasterizerState, &m_rsStateWire))
    return false;

  // we are ready
  return true;
}

ADDONCREATOR(CVisualizationSpectrum)

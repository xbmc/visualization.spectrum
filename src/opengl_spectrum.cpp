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
 *
 *  Ported to GLES 2.0 by Gimli
 */

#define __STDC_LIMIT_MACROS

#include <kodi/addon-instance/Visualization.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <cstddef>
#include "shaders/GUIShader.h"

#if defined(HAS_GLES2)

#ifndef M_PI
#define M_PI       3.141592654f
#endif

#elif defined(HAS_OPENGL)
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

#define NUM_BANDS 16
#define DEG2RAD(d) ( (d) * M_PI/180.0f )
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

class ATTRIBUTE_HIDDEN CVisualizationSpectrum
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

  GLfloat m_heights[16][16];
  GLfloat m_cHeights[16][16];
  GLfloat m_scale;
  GLenum m_mode;
  float m_y_angle, m_y_speed;
  float m_x_angle, m_x_speed;
  float m_z_angle, m_z_speed;
  float m_hSpeed;

  CGUIShader *m_shader;

  void draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue );
  void draw_bars(void);
};

CVisualizationSpectrum::CVisualizationSpectrum()
  : m_mode(GL_TRIANGLES),
    m_y_angle(45.0f),
    m_y_speed(0.5f),
    m_x_angle(20.0f),
    m_x_speed(0.0f),
    m_z_angle(0.0f),
    m_z_speed(0.0f),
    m_hSpeed(0.05f)
{
  m_scale = 1.0 / log(256.0);

  SetBarHeightSetting(kodi::GetSettingInt("bar_height"));
  SetSpeedSetting(kodi::GetSettingInt("speed"));
  SetModeSetting(kodi::GetSettingInt("mode"));

  m_shader = new CGUIShader("vert.glsl", "frag.glsl");
}

CVisualizationSpectrum::~CVisualizationSpectrum()
{
  m_shader->Free();
  delete m_shader;
}

void CVisualizationSpectrum::draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue )
{
  // avoid zero sized bars, which results in overlapping triangles of same depth and display artefacts
  height = std::max(height, 1e-3f);

  GLint posLoc = m_shader->GetPosLoc();
  GLint colLoc = m_shader->GetColLoc();

  GLubyte idx[] =  {
                      // Bottom
                      0, 1, 2,
                      0, 2, 3,
                      // Left
                      0, 4, 7,
                      0, 7, 3,
                      // Back
                      3, 7, 6,
                      3, 6, 2,
                      // Right
                      1, 5, 6,
                      1, 6, 2,
                      // Front
                      0, 4, 5,
                      0, 5, 1,
                      // Top
                      4, 5, 6,
                      4, 6, 7
                   };

#if defined (HAS_OPENGL)

  struct PackedVertex
  {
	GLfloat x, y, z;
	GLfloat r, g, b;
  }vertex[8];

  vertex[0].x = x_offset + 0.0f;
  vertex[0].y = 0.0f;
  vertex[0].z = z_offset + 0.0f;
  vertex[1].x = x_offset + 0.1f;
  vertex[1].y = 0.0f;
  vertex[1].z = z_offset + 0.0f;
  vertex[2].x = x_offset + 0.1f;
  vertex[2].y = 0.0f;
  vertex[2].z = z_offset + 0.1f;
  vertex[3].x = x_offset + 0.0f;
  vertex[3].y = 0.0f;
  vertex[3].z = z_offset + 0.1f;

  vertex[4].x = x_offset + 0.0f;
  vertex[4].y = height;
  vertex[4].z = z_offset + 0.0f;
  vertex[5].x = x_offset + 0.1f;
  vertex[5].y = height;
  vertex[5].z = z_offset + 0.0f;
  vertex[6].x = x_offset + 0.1f;
  vertex[6].y = height;
  vertex[6].z = z_offset + 0.1f;
  vertex[7].x = x_offset + 0.0f;
  vertex[7].y = height;
  vertex[7].z = z_offset + 0.1f;

  vertex[0].r = red * 0.1f;
  vertex[0].g = green * 0.1f;
  vertex[0].b = blue * 0.1f;
  vertex[1].r = red * 0.2f;
  vertex[1].g = green * 0.2f;
  vertex[1].b = blue * 0.2f;
  vertex[2].r = red * 0.3f;
  vertex[2].g = green * 0.3f;
  vertex[2].b = blue * 0.3f;
  vertex[3].r = red * 0.4f;
  vertex[3].g = green * 0.4f;
  vertex[3].b = blue * 0.4f;

  vertex[4].r = red * 0.5f;
  vertex[4].g = green * 0.5f;
  vertex[4].b = blue * 0.5f;
  vertex[5].r = red * 0.6f;
  vertex[5].g = green * 0.6f;
  vertex[5].b = blue * 0.6f;
  vertex[6].r = red * 0.7f;
  vertex[6].g = green * 0.7f;
  vertex[6].b = blue * 0.7f;
  vertex[7].r = red * 0.8f;
  vertex[7].g = green * 0.8f;
  vertex[7].b = blue * 0.8f;

  GLuint vertexVBO;
  GLuint indexVBO;

  glGenBuffers(1, &vertexVBO);
  glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(PackedVertex)*8, &vertex[0], GL_STATIC_DRAW);

  glVertexAttribPointer(posLoc, 3, GL_FLOAT, 0, sizeof(PackedVertex), BUFFER_OFFSET(offsetof(PackedVertex, x)));
  glVertexAttribPointer(colLoc, 3, GL_FLOAT, 0, sizeof(PackedVertex), BUFFER_OFFSET(offsetof(PackedVertex, r)));

  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(colLoc);

  glGenBuffers(1, &indexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte)*36, idx, GL_STATIC_DRAW);

  glDrawElements(m_mode, 36, GL_UNSIGNED_BYTE, 0);

  glDisableVertexAttribArray(posLoc);
  glDisableVertexAttribArray(colLoc);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &vertexVBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &indexVBO);

#else
  GLfloat col[] =  {
                      red * 0.1f, green * 0.1f, blue * 0.1f,
                      red * 0.2f, green * 0.2f, blue * 0.2f,
                      red * 0.3f, green * 0.3f, blue * 0.3f,
                      red * 0.4f, green * 0.4f, blue * 0.4f,
                      red * 0.5f, green * 0.5f, blue * 0.5f,
                      red * 0.6f, green * 0.6f, blue * 0.6f,
                      red * 0.7f, green * 0.7f, blue * 0.7f,
                      red * 0.8f, green * 0.8f, blue * 0.8f
                   };
  GLfloat ver[] =  {
                      x_offset + 0.0f, 0.0f,    z_offset + 0.0f,
                      x_offset + 0.1f, 0.0f,    z_offset + 0.0f,
                      x_offset + 0.1f, 0.0f,    z_offset + 0.1f,
                      x_offset + 0.0f, 0.0f,    z_offset + 0.1f,
                      x_offset + 0.0f, height,  z_offset + 0.0f,
                      x_offset + 0.1f, height,  z_offset + 0.0f,
                      x_offset + 0.1f, height,  z_offset + 0.1f,
                      x_offset + 0.0f, height,  z_offset + 0.1f
                   };

  glVertexAttribPointer(colLoc, 3, GL_FLOAT, 0, 0, col);
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, 0, 0, ver);

  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(colLoc);

  glDrawElements(m_mode, 36, GL_UNSIGNED_BYTE, idx);

  glDisableVertexAttribArray(posLoc);
  glDisableVertexAttribArray(colLoc);
#endif
}

void CVisualizationSpectrum::draw_bars(void)
{
  int x,y;
  GLfloat x_offset, z_offset, r_base, b_base;

  glClear(GL_DEPTH_BUFFER_BIT);
  m_shader->PushMatrix();
  m_shader->Translatef(0.0,-0.5,-5.0);
  m_shader->Rotatef(DEG2RAD(m_x_angle),1.0,0.0,0.0);
  m_shader->Rotatef(DEG2RAD(m_y_angle),0.0,1.0,0.0);
  m_shader->Rotatef(DEG2RAD(m_z_angle),0.0,0.0,1.0);
  
  m_shader->Enable();
  
  for(y = 0; y < 16; y++)
  {
    z_offset = -1.6 + ((15 - y) * 0.2);

    b_base = y * (1.0 / 15);
    r_base = 1.0 - b_base;

    for(x = 0; x < 16; x++)
    {
      x_offset = -1.6 + ((float)x * 0.2);
      if (::fabs(m_cHeights[y][x]-m_heights[y][x])>m_hSpeed)
      {
        if (m_cHeights[y][x]<m_heights[y][x])
          m_cHeights[y][x] += m_hSpeed;
        else
          m_cHeights[y][x] -= m_hSpeed;
      }
      draw_bar(x_offset, z_offset,
        m_cHeights[y][x], r_base - (float(x) * (r_base / 15.0)),
        (float)x * (1.0 / 15), b_base);
    }
  }
  m_shader->Disable();

  m_shader->PopMatrix();
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
void CVisualizationSpectrum::Render()
{
  glDisable(GL_BLEND);
  m_shader->MatrixMode(MM_PROJECTION);
  m_shader->PushMatrix();
  m_shader->LoadIdentity();
  m_shader->Frustum(-1, 1, -1, 1, 1.5, 10);
  m_shader->MatrixMode(MM_MODELVIEW);
  m_shader->PushMatrix();
  m_shader->LoadIdentity();
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  m_x_angle += m_x_speed;
  if(m_x_angle >= 360.0)
    m_x_angle -= 360.0;

  m_y_angle += m_y_speed;
  if(m_y_angle >= 360.0)
    m_y_angle -= 360.0;

  m_z_angle += m_z_speed;
  if(m_z_angle >= 360.0)
    m_z_angle -= 360.0;

  draw_bars();
  m_shader->PopMatrix();
  m_shader->MatrixMode(MM_PROJECTION);
  m_shader->PopMatrix();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
}

bool CVisualizationSpectrum::Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, std::string szSongName)
{
  if (!m_shader->CompileAndLink())
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to create Open GL ES 2.0 visualization GUI shader");
    return false;
  }

  int x, y;

  for(x = 0; x < 16; x++)
  {
    for(y = 0; y < 16; y++)
    {
      m_cHeights[y][x] = 0.0;
    }
  }

  m_x_speed = 0.0;
  m_y_speed = 0.5;
  m_z_speed = 0.0;
  m_x_angle = 20.0;
  m_y_angle = 45.0;
  m_z_angle = 0.0;

  return true;
}

void CVisualizationSpectrum::AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
{
  int i,c;
  int y=0;
  GLfloat val;

  int xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};

  for(y = 15; y > 0; y--)
  {
    for(i = 0; i < 16; i++)
    {
      m_heights[y][i] = m_heights[y - 1][i];
    }
  }

  for(i = 0; i < NUM_BANDS; i++)
  {
    for(c = xscale[i], y = 0; c < xscale[i + 1]; c++)
    {
      if (c<iAudioDataLength)
      {
        if((int)(pAudioData[c] * (INT16_MAX)) > y)
          y = (int)(pAudioData[c] * (INT16_MAX));
      }
      else
        continue;
    }
    y >>= 7;
    if(y > 0)
      val = (logf(y) * m_scale);
    else
      val = 0;
    m_heights[0][i] = val;
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
      m_mode = GL_LINE_LOOP;
      break;

    case 2:
      m_mode = GL_LINES; //no points on gles!
      break;

    case 0:
    default:
      m_mode = GL_TRIANGLES;
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

ADDONCREATOR(CVisualizationSpectrum)

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
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#if defined(HAS_GLES2)
#include "VisGUIShader.h"

#ifndef M_PI
#define M_PI       3.141592654f
#endif
#define DEG2RAD(d) ( (d) * M_PI/180.0f )

//OpenGL wrapper - allows us to use same code of functions draw_bars and render
#define GL_PROJECTION             MM_PROJECTION
#define GL_MODELVIEW              MM_MODELVIEW

#define glPushMatrix()            m_visShader->PushMatrix()
#define glPopMatrix()             m_visShader->PopMatrix()
#define glTranslatef(x,y,z)       m_visShader->Translatef(x,y,z)
#define glRotatef(a,x,y,z)        m_visShader->Rotatef(DEG2RAD(a),x,y,z)
#define glPolygonMode(a,b)        ;
#define glBegin(a)                m_visShader->Enable()
#define glEnd()                   m_visShader->Disable()
#define glMatrixMode(a)           m_visShader->MatrixMode(a)
#define glLoadIdentity()          m_visShader->LoadIdentity()
#define glFrustum(a,b,c,d,e,f)    m_visShader->Frustum(a,b,c,d,e,f)

static const char *frag = "precision mediump float; \n"
                   "varying lowp vec4 m_colour; \n"
                   "void main () \n"
                   "{ \n"
                   "  gl_FragColor = m_colour; \n"
                   "}\n";

static const char *vert = "attribute vec4 m_attrpos;\n"
                   "attribute vec4 m_attrcol;\n"
                   "attribute vec4 m_attrcord0;\n"
                   "attribute vec4 m_attrcord1;\n"
                   "varying vec4   m_cord0;\n"
                   "varying vec4   m_cord1;\n"
                   "varying lowp   vec4 m_colour;\n"
                   "uniform mat4   m_proj;\n"
                   "uniform mat4   m_model;\n"
                   "void main ()\n"
                   "{\n"
                   "  mat4 mvp    = m_proj * m_model;\n"
                   "  gl_Position = mvp * m_attrpos;\n"
                   "  m_colour    = m_attrcol;\n"
                   "  m_cord0     = m_attrcord0;\n"
                   "  m_cord1     = m_attrcord1;\n"
                   "}\n";

#elif defined(HAS_OPENGL)
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

#define NUM_BANDS 16

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

  GLfloat m_heights[16][16], m_cHeights[16][16], m_scale;
  GLenum m_mode;
  float m_y_angle, m_y_speed;
  float m_x_angle, m_x_speed;
  float m_z_angle, m_z_speed;
  float m_hSpeed;

#if defined(HAS_GLES2)
  CVisGUIShader *m_visShader;
  GLfloat  *m_col;
  GLfloat  *m_ver;
  GLushort *m_idx;
  void init_bars(void);
#endif
#if defined(HAS_OPENGL)
  void draw_rectangle(GLfloat x1, GLfloat y1, GLfloat z1, GLfloat x2, GLfloat y2, GLfloat z2);
#endif
#if defined(HAS_GLES2) || defined(HAS_OPENGL)
  void draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue, int index);
#endif
  void draw_bars(void);
};

CVisualizationSpectrum::CVisualizationSpectrum()
#if defined(HAS_GLES2)
  : m_mode(GL_TRIANGLES),
#elif defined(HAS_OPENGL)
  : m_mode(GL_FILL),
#endif
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

#if defined(HAS_GLES2)
  m_visShader = new CVisGUIShader(vert, frag);
  m_col = (GLfloat  *)malloc(16*16*3*8 * sizeof(GLfloat *));
  m_ver = (GLfloat  *)malloc(16*16*3*8 * sizeof(GLfloat *));
  m_idx = (GLushort *)malloc(16*16*36  * sizeof(GLushort *));
#endif
}

CVisualizationSpectrum::~CVisualizationSpectrum()
{
#if defined(HAS_GLES2)
  m_visShader->Free();
  delete m_visShader;
  free(m_col);
  free(m_ver);
  free(m_idx);
  m_col = nullptr;
  m_ver = nullptr;
  m_idx = nullptr;
#endif
}

#if defined(HAS_OPENGL)
void CVisualizationSpectrum::draw_rectangle(GLfloat x1, GLfloat y1, GLfloat z1, GLfloat x2, GLfloat y2, GLfloat z2)
{
  if(y1 == y2)
  {
    glVertex3f(x1, y1, z1);
    glVertex3f(x2, y1, z1);
    glVertex3f(x2, y2, z2);

    glVertex3f(x2, y2, z2);
    glVertex3f(x1, y2, z2);
    glVertex3f(x1, y1, z1);
  }
  else
  {
    glVertex3f(x1, y1, z1);
    glVertex3f(x2, y1, z2);
    glVertex3f(x2, y2, z2);

    glVertex3f(x2, y2, z2);
    glVertex3f(x1, y2, z1);
    glVertex3f(x1, y1, z1);
  }
}

void CVisualizationSpectrum::draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue, int index)
{
  GLfloat width = 0.1;

  if (m_mode == GL_POINT)
    glColor3f(0.2, 1.0, 0.2);

  if (m_mode != GL_POINT)
  {
    glColor3f(red,green,blue);
    draw_rectangle(x_offset, height, z_offset, x_offset + width, height, z_offset + 0.1);
  }
  draw_rectangle(x_offset, 0, z_offset, x_offset + width, 0, z_offset + 0.1);

  if (m_mode != GL_POINT)
  {
    glColor3f(0.5 * red, 0.5 * green, 0.5 * blue);
    draw_rectangle(x_offset, 0.0, z_offset + 0.1, x_offset + width, height, z_offset + 0.1);
  }
  draw_rectangle(x_offset, 0.0, z_offset, x_offset + width, height, z_offset );

  if (m_mode != GL_POINT)
  {
    glColor3f(0.25 * red, 0.25 * green, 0.25 * blue);
    draw_rectangle(x_offset, 0.0, z_offset , x_offset, height, z_offset + 0.1);
  }
  draw_rectangle(x_offset + width, 0.0, z_offset , x_offset + width, height, z_offset + 0.1);
}

#elif defined(HAS_GLES2)

void CVisualizationSpectrum::draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue, int index)
{
  if (!m_col || !m_ver || !m_idx)
    return;

  // avoid zero sized bars, which results in overlapping triangles of same depth and display artefacts
  height = std::max(height, 1e-3f);

  GLfloat *ver = m_ver + 3 * 8 * index;
  // just need to update the height vertex, all else is the same
  for (int i=0; i<8; i++)
  {
    ver[1] = (((i+0)>>2)&1) * height;
    ver += 3;
  }
  // on last index, draw the object
  if (index == 16*16-1)
  {
    GLint   posLoc = m_visShader->GetPosLoc();
    GLint   colLoc = m_visShader->GetColLoc();

    glVertexAttribPointer(colLoc, 3, GL_FLOAT, 0, 0, m_col);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, 0, 0, m_ver);

    glEnableVertexAttribArray(posLoc);
    glEnableVertexAttribArray(colLoc);

    glDrawElements(m_mode, 16*16*36, GL_UNSIGNED_SHORT, m_idx);

    glDisableVertexAttribArray(posLoc);
    glDisableVertexAttribArray(colLoc);
  }
}

void CVisualizationSpectrum::init_bars(void)
{
  if (!m_col || !m_ver || !m_idx)
    return;

  GLfloat x_offset, z_offset, r_base, b_base;
  for(int y = 0; y < 16; y++)
  {
    z_offset = -1.6 + ((15 - y) * 0.2);

    b_base = y * (1.0 / 15);
    r_base = 1.0 - b_base;

    for(int x = 0; x < 16; x++)
    {
      x_offset = -1.6 + ((float)x * 0.2);

      GLfloat red = r_base - (float(x) * (r_base / 15.0));
      GLfloat green = (float)x * (1.0 / 15);
      GLfloat blue = b_base;
      int index = 16*y+x;
      GLfloat *col = m_col + 3 * 8 * index;
      for (int i=0; i<8; i++)
      {
        float scale = 0.1f * i;
        *col++ = red   * scale;
        *col++ = green * scale;
        *col++ = blue  * scale;
      }
      GLfloat *ver = m_ver + 3 * 8 * index;
      for (int i=0; i<8; i++)
      {
        *ver++ = x_offset + (((i+1)>>1)&1) * 0.1f;
        *ver++ = 0; // height - filled in later
        *ver++ = z_offset + (((i+0)>>1)&1) * 0.1f;
      }
      GLushort *idx = m_idx + 36 * index;
      GLushort startidx = 8 * index;
      // Bottom
      *idx++ = startidx + 0; *idx++ = startidx + 1; *idx++ = startidx + 2;
      *idx++ = startidx + 0; *idx++ = startidx + 2; *idx++ = startidx + 3;
      // Left
      *idx++ = startidx + 0; *idx++ = startidx + 4; *idx++ = startidx + 7;
      *idx++ = startidx + 0; *idx++ = startidx + 7; *idx++ = startidx + 3;
      // Back
      *idx++ = startidx + 3; *idx++ = startidx + 7; *idx++ = startidx + 6;
      *idx++ = startidx + 3; *idx++ = startidx + 6; *idx++ = startidx + 2;
      // Right
      *idx++ = startidx + 1; *idx++ = startidx + 5; *idx++ = startidx + 6;
      *idx++ = startidx + 1; *idx++ = startidx + 6; *idx++ = startidx + 2;
      // Front
      *idx++ = startidx + 0; *idx++ = startidx + 4; *idx++ = startidx + 5;
      *idx++ = startidx + 0; *idx++ = startidx + 5; *idx++ = startidx + 1;
      // Top
      *idx++ = startidx + 4; *idx++ = startidx + 5; *idx++ = startidx + 6;
      *idx++ = startidx + 4; *idx++ = startidx + 6; *idx++ = startidx + 7;
    }
  }
}
#endif

void CVisualizationSpectrum::draw_bars(void)
{
  int x,y;
  GLfloat x_offset, z_offset, r_base, b_base;

  glClear(GL_DEPTH_BUFFER_BIT);
  glPushMatrix();
  glTranslatef(0.0,-0.5,-5.0);
  glRotatef(m_x_angle,1.0,0.0,0.0);
  glRotatef(m_y_angle,0.0,1.0,0.0);
  glRotatef(m_z_angle,0.0,0.0,1.0);
  
  glPolygonMode(GL_FRONT_AND_BACK, m_mode);
  glBegin(GL_TRIANGLES);
  
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
        (float)x * (1.0 / 15), b_base, 16*y+x);
    }
  }
  glEnd();
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glPopMatrix();
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
void CVisualizationSpectrum::Render()
{
  glDisable(GL_BLEND);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glFrustum(-1, 1, -1, 1, 1.5, 10);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glPolygonMode(GL_FRONT, GL_FILL);
  //glPolygonMode(GL_BACK, GL_FILL);
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
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
}

bool CVisualizationSpectrum::Start(int iChannels, int iSamplesPerSec, int iBitsPerSample, std::string szSongName)
{
#if defined(HAS_GLES2)
  if (!m_visShader->CompileAndLink())
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to create Open GL ES 2.0 visualization GUI shader");
    return false;
  }
  init_bars();
#endif

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
#if defined(HAS_OPENGL)
  switch (settingValue)
  {
    case 1:
      m_mode = GL_LINE;
      break;

    case 2:
      m_mode = GL_POINT;
      break;

    case 0:
    default:
      m_mode = GL_FILL;
      break;
  }
#else
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
#endif
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

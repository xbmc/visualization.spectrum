/*
 *  Copyright (C) 1998-2000 Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 2005-2022 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
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
#include <kodi/gui/gl/GL.h>
#include <kodi/gui/gl/Shader.h>

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <cstddef>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.141592654f
#endif

#define NUM_BANDS 16

class ATTR_DLL_LOCAL CVisualizationSpectrum
  : public kodi::addon::CAddonBase,
    public kodi::addon::CInstanceVisualization,
    public kodi::gui::gl::CShaderProgram
{
public:
  CVisualizationSpectrum();
  ~CVisualizationSpectrum() override = default;

  bool Start(int channels, int samplesPerSec, int bitsPerSample, const std::string& songName) override;
  void Stop() override;
  void Render() override;
  void AudioData(const float* audioData, size_t audioDataLength) override;

  ADDON_STATUS Create() override;
  ADDON_STATUS SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue) override;

  void OnCompiledAndLinked() override;
  bool OnEnabled() override;

private:
  void draw_bars(void);
  void draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue);

  bool m_glInitialized = false;
  bool m_shadersLoaded = false;

  GLfloat m_heights[NUM_BANDS][NUM_BANDS];
  GLfloat m_cHeights[NUM_BANDS][NUM_BANDS];

  GLfloat m_hScale = 1.0 / log(256.0);
  GLfloat m_hSpeed = 0.05f;

  GLfloat m_pointSize = 0.0f;

  GLfloat m_xAngle = 20.0f;
  GLfloat m_yAngle = 45.0f;
  GLfloat m_zAngle = 0.0f;

  GLfloat m_yFixedAngle = -15.0f;

  GLfloat m_ySpeed = 0.5f;

  GLenum m_drawMode = GL_TRIANGLES;

  // Shader related data

  glm::mat4 m_projMat;
  glm::mat4 m_modelMat;

  std::vector<glm::vec3> m_colorBufferData;
  std::vector<glm::vec3> m_vertexBufferData;

#ifdef HAS_GL
  GLuint m_vertexVBO[2] = {0};
#endif

  GLint m_uProjMatrix = -1;
  GLint m_uModelMatrix = -1;
  GLint m_uPointSize = -1;
  GLint m_hPos = -1;
  GLint m_hCol = -1;
};

CVisualizationSpectrum::CVisualizationSpectrum()
{
  m_vertexBufferData.resize(48);
  m_colorBufferData.resize(48);
}

bool CVisualizationSpectrum::Start(int channels, int samplesPerSec, int bitsPerSample, const std::string& songName)
{
  (void)channels;
  (void)samplesPerSec;
  (void)bitsPerSample;
  (void)songName;

  if (!m_shadersLoaded)
  {
    std::string fraqShader = kodi::addon::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/frag.glsl");
    std::string vertShader = kodi::addon::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/vert.glsl");
    if (!LoadShaderFiles(vertShader, fraqShader) || !CompileAndLink())
    {
      kodi::Log(ADDON_LOG_ERROR, "Failed to create or compile shader");
      return false;
    }
    m_shadersLoaded = true;
  }

  for (int x = 0; x < NUM_BANDS; x++)
  {
    for (int y = 0; y < NUM_BANDS; y++)
    {
      m_heights[y][x] = 0.0f;
      m_cHeights[y][x] = 0.0f;
    }
  }

  m_projMat = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.5f, 10.0f);

#ifdef HAS_GL
  glGenBuffers(2, m_vertexVBO);
#endif

  m_glInitialized = true;

  return true;
}

void CVisualizationSpectrum::Stop()
{
  if (!m_glInitialized)
    return;

  m_glInitialized = false;

#ifdef HAS_GL
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(2, m_vertexVBO);
  m_vertexVBO[0] = 0;
  m_vertexVBO[1] = 0;
#endif
}

//-- Render -------------------------------------------------------------------
// Called once per frame. Do all rendering here.
//-----------------------------------------------------------------------------
void CVisualizationSpectrum::Render()
{
  if (!m_glInitialized)
    return;

#ifdef HAS_GL
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[0]);
  glVertexAttribPointer(m_hPos, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, nullptr);
  glEnableVertexAttribArray(m_hPos);

  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[1]);
  glVertexAttribPointer(m_hCol, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, nullptr);
  glEnableVertexAttribArray(m_hCol);
#else
  // 1st attribute buffer : vertices
  glEnableVertexAttribArray(m_hPos);
  glVertexAttribPointer(m_hPos, 3, GL_FLOAT, GL_FALSE, 0, &m_vertexBufferData[0]);

  // 2nd attribute buffer : colors
  glEnableVertexAttribArray(m_hCol);
  glVertexAttribPointer(m_hCol, 3, GL_FLOAT, GL_FALSE, 0, &m_colorBufferData[0]);
#endif

  glDisable(GL_BLEND);
#ifdef HAS_GL
  glEnable(GL_PROGRAM_POINT_SIZE);
#endif
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  // Clear the screen
  glClear(GL_DEPTH_BUFFER_BIT);

  if (m_yFixedAngle < 0.0f)
    m_yAngle = std::fmod(m_yAngle + m_ySpeed, 360.0f);
  else
    m_yAngle = m_yFixedAngle;

  m_modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, -5.0f));
  m_modelMat = glm::rotate(m_modelMat, glm::radians(m_xAngle), glm::vec3(1.0f, 0.0f, 0.0f));
  m_modelMat = glm::rotate(m_modelMat, glm::radians(m_yAngle), glm::vec3(0.0f, 1.0f, 0.0f));
  m_modelMat = glm::rotate(m_modelMat, glm::radians(m_zAngle), glm::vec3(0.0f, 0.0f, 1.0f));

  EnableShader();

  draw_bars();

  DisableShader();

  glDisableVertexAttribArray(m_hPos);
  glDisableVertexAttribArray(m_hCol);

  glDisable(GL_DEPTH_TEST);
#ifdef HAS_GL
  glDisable(GL_PROGRAM_POINT_SIZE);
#endif
  glEnable(GL_BLEND);
}

void CVisualizationSpectrum::OnCompiledAndLinked()
{
  // Variables passed directly to the Vertex shader
  m_uProjMatrix = glGetUniformLocation(ProgramHandle(), "u_projectionMatrix");
  m_uModelMatrix = glGetUniformLocation(ProgramHandle(), "u_modelViewMatrix");
  m_uPointSize = glGetUniformLocation(ProgramHandle(), "u_pointSize");
  m_hPos = glGetAttribLocation(ProgramHandle(), "a_position");
  m_hCol = glGetAttribLocation(ProgramHandle(), "a_color");
}

bool CVisualizationSpectrum::OnEnabled()
{
  // This is called after glUseProgram()
  glUniformMatrix4fv(m_uProjMatrix, 1, GL_FALSE, glm::value_ptr(m_projMat));
  glUniformMatrix4fv(m_uModelMatrix, 1, GL_FALSE, glm::value_ptr(m_modelMat));

  if (m_drawMode == GL_POINTS)
    glUniform1f(m_uPointSize, m_pointSize);
  else
    glUniform1f(m_uPointSize, 0.0f);

  return true;
}

void CVisualizationSpectrum::draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue )
{
  GLfloat width = 0.1f;
  m_vertexBufferData =
  {
    // Bottom
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset },

    { x_offset,         0.0f,   z_offset + width },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset,         0.0f,   z_offset },

    // Side
    { x_offset,         0.0f,   z_offset },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset,         height, z_offset + width },
    { x_offset,         0.0f,   z_offset },
    { x_offset,         height, z_offset + width },
    { x_offset,         height, z_offset },

    { x_offset + width, height, z_offset },
    { x_offset,         0.0f,   z_offset },
    { x_offset,         height, z_offset },
    { x_offset + width, height, z_offset },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset,         0.0f,   z_offset },

    { x_offset,         height, z_offset + width },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset + width, height, z_offset + width },
    { x_offset,         height, z_offset + width },
    { x_offset + width, 0.0f,   z_offset + width },

    { x_offset + width, height, z_offset + width },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, height, z_offset },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, height, z_offset + width },
    { x_offset + width, 0.0f,   z_offset + width },

    // Top
    { x_offset + width, height, z_offset + width },
    { x_offset + width, height, z_offset },
    { x_offset,         height, z_offset },
    { x_offset + width, height, z_offset + width },
    { x_offset,         height, z_offset },
    { x_offset,         height, z_offset + width },

    { x_offset,         height, z_offset + width },
    { x_offset + width, height, z_offset },
    { x_offset,         height, z_offset },
    { x_offset + width, height, z_offset },
    { x_offset + width, height, z_offset + width },
    { x_offset,         height, z_offset + width }
  };

  float sideMlpy1, sideMlpy2, sideMlpy3, sideMlpy4;
  if (m_drawMode == GL_TRIANGLES)
  {
    sideMlpy1 = 0.5f;
    sideMlpy2 = 0.25f;
    sideMlpy3 = 0.75f;
    sideMlpy4 = 0.5f;
  }
  else
  {
    sideMlpy1 = sideMlpy2 = sideMlpy3 = sideMlpy4 = 1.0f;
  }

  // One color for each vertex. They were generated randomly.
  m_colorBufferData =
  {
    // Bottom
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },

    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },

    // Side
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },

    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },

    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },

    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },

    // Top
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },

    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
  };

#ifdef HAS_GL
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[0]);
  glBufferData(GL_ARRAY_BUFFER, m_vertexBufferData.size()*sizeof(glm::vec3), &m_vertexBufferData[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[1]);
  glBufferData(GL_ARRAY_BUFFER, m_colorBufferData.size()*sizeof(glm::vec3), &m_colorBufferData[0], GL_STATIC_DRAW);
#endif
  glDrawArrays(m_drawMode, 0, m_vertexBufferData.size()); /* 12*3 indices starting at 0 -> 12 triangles + 4*3 to have on lines show correct */
}

void CVisualizationSpectrum::draw_bars(void)
{
  int x, y;
  GLfloat x_offset, z_offset, r_base, b_base;

  for(y = 0; y < NUM_BANDS; y++)
  {
    z_offset = -1.6 + ((15 - y) * 0.2);

    b_base = y * (1.0 / 15);
    r_base = 1.0 - b_base;

    for(x = 0; x < NUM_BANDS; x++)
    {
      x_offset = -1.6 + ((float)x * 0.2);
      if (std::fabs(m_cHeights[y][x] - m_heights[y][x]) > m_hSpeed)
      {
        if (m_cHeights[y][x] < m_heights[y][x])
          m_cHeights[y][x] += m_hSpeed;
        else
          m_cHeights[y][x] -= m_hSpeed;
      }
      else
      {
        m_cHeights[y][x] = m_heights[y][x];
      }

      draw_bar(x_offset, z_offset, m_cHeights[y][x], r_base - (float(x) * (r_base / 15.0)), (float)x * (1.0 / 15), b_base);
    }
  }
}

void CVisualizationSpectrum::AudioData(const float* pAudioData, size_t iAudioDataLength)
{
  int i,c;
  int y=0;
  GLfloat val;

  int xscale[] = {0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 187, 255};

  for(y = NUM_BANDS - 1; y > 0; y--)
  {
    for(i = 0; i < NUM_BANDS; i++)
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
      val = (logf(y) * m_hScale);
    else
      val = 0;
    m_heights[0][i] = val;
  }
}

ADDON_STATUS CVisualizationSpectrum::Create()
{
  return ADDON_STATUS_NEED_SETTINGS;
}

ADDON_STATUS CVisualizationSpectrum::SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue)
{
  if (settingName.empty() || settingValue.empty())
    return ADDON_STATUS_UNKNOWN;

  int value = settingValue.GetInt();

  if (settingName == "bar_height")
  {
    switch (value)
    {
      case 0:
      {
        m_hScale = 0.5f; // small
        break;
      }
      case 1:
      default:
      {
        m_hScale = 1.0f; // default
        break;
      }
      case 2:
      {
        m_hScale = 2.0f; // big
        break;
      }
      case 3:
      {
        m_hScale = 3.0f; // very big
        break;
      }
      case 4:
      {
        m_hScale = 0.33f; // unused
        break;
      }
    }
    m_hScale /= log(256.0f);
  }
  else if (settingName == "mode")
  {
    switch (value)
    {
      case 0:
      default:
      {
        m_drawMode = GL_TRIANGLES;
        break;
      }
      case 1:
      {
        m_drawMode = GL_LINES;
        break;
      }
      case 2:
      {
        m_drawMode = GL_POINTS;
        break;
      }
    }
  }
  else if (settingName == "pointsize")
  {
    m_pointSize = value;
  }
  else if (settingName == "rotation_angle")
  {
    m_yFixedAngle = value;
  }
  else if (settingName == "speed")
  {
    switch (value)
    {
      case 0:
      {
        m_hSpeed = 0.0125f; // very slow
        break;
      }
      case 1:
      {
        m_hSpeed = 0.025f; // slow
        break;
      }
      case 2:
      default:
      {
        m_hSpeed = 0.05f; // default
        break;
      }
      case 3:
      {
        m_hSpeed = 0.1f; // fast
        break;
      }
      case 4:
      {
        m_hSpeed = 0.2f; // unused
        break;
      }
    }
  }
  else
    return ADDON_STATUS_UNKNOWN;

  return ADDON_STATUS_OK;
}

ADDONCREATOR(CVisualizationSpectrum)

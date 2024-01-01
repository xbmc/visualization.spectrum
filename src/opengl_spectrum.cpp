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

#include "kfftr.h"

#ifndef M_PI
#define M_PI 3.141592654f
#endif

#define NUM_BANDS 16

#define MIN_FREQS_PER_BAR 2 // use at least two freqs per bar

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
  void AddBar(GLfloat xMid, GLfloat zMid, GLfloat height, GLfloat red, GLfloat green, GLfloat blue);
  void AddQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color);
  void RenderBufferData();

  std::unique_ptr<KFFTR> m_transform;
  std::unique_ptr<float[]> m_freqData;

  bool m_glInitialized = false;
  bool m_shadersLoaded = false;

  int m_channels = 0;
  int m_dbRange = 48;

  size_t m_freqDataLength = 0;
  size_t m_prevFreqDataLength = 0;

  size_t m_samples = 0;

  size_t m_xScales[NUM_BANDS];

  GLfloat m_hScales[NUM_BANDS];
  GLfloat m_heights[NUM_BANDS][NUM_BANDS];
  GLfloat m_cHeights[NUM_BANDS][NUM_BANDS];

  GLfloat m_hScale = 1.0f;
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
  for (int x = 0; x < NUM_BANDS; x++)
  {
    m_xScales[x] = 0;
    m_hScales[x] = 0.0;
  }
}

bool CVisualizationSpectrum::Start(int channels, int samplesPerSec, int bitsPerSample, const std::string& songName)
{
  m_channels = channels;
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

  RenderBufferData();

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

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

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

#ifdef HAS_GL
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[0]);
  glBufferData(GL_ARRAY_BUFFER, m_vertexBufferData.size() * sizeof(glm::vec3), &m_vertexBufferData[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[1]);
  glBufferData(GL_ARRAY_BUFFER, m_colorBufferData.size() * sizeof(glm::vec3), &m_colorBufferData[0], GL_STATIC_DRAW);
#endif

  EnableShader();

  glDrawArrays(m_drawMode, 0, m_vertexBufferData.size());

  DisableShader();

  glDisableVertexAttribArray(m_hPos);
  glDisableVertexAttribArray(m_hCol);

  glDisable(GL_CULL_FACE);

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

void CVisualizationSpectrum::AddQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color)
{
  m_vertexBufferData.insert(m_vertexBufferData.end(),
  {
    a, b, // line-mode: 1st line
    c, c,
    d, a, // line-mode: 2nd line
  });
  m_colorBufferData.insert(m_colorBufferData.end(), 6, color);
}

void CVisualizationSpectrum::AddBar(GLfloat xMid, GLfloat zMid, GLfloat height, GLfloat red, GLfloat green, GLfloat blue)
{
  GLfloat wHalf = 0.1f * 0.5f;

  GLfloat lft = xMid - wHalf;
  GLfloat rgt = xMid + wHalf;

  GLfloat bck = zMid - wHalf;
  GLfloat fnt = zMid + wHalf;

  GLfloat top = height;
  GLfloat btm = 0.0f;

  glm::vec3 color = {red, green, blue};

  GLfloat sideMlpy1 = 1.0f;
  GLfloat sideMlpy2 = 1.0f;
  GLfloat sideMlpy3 = 1.0f;
  GLfloat sideMlpy4 = 1.0f;

  if (m_drawMode == GL_TRIANGLES)
  {
    sideMlpy1 = 0.5f;
    sideMlpy2 = 0.25f;
    sideMlpy3 = 0.75f;
    sideMlpy4 = 0.5f;
  }

  // notes:
  // Vertices must be in counter-clock-wise order for face-culling.
  // For lines-mode, only 1st <-> 2nd and 1st <-> last vertex are used.
  // Therefore the 1st vertices are choosen so that all 12 edges are drawn.

  // Bottom
  AddQuad({rgt, btm, fnt}, {lft, btm, fnt}, {lft, btm, bck}, {rgt, btm, bck}, color);
  // Left side
  AddQuad({lft, btm, fnt}, {lft, top, fnt}, {lft, top, bck}, {lft, btm, bck}, color * sideMlpy1);
  // Back
  AddQuad({lft, btm, bck}, {lft, top, bck}, {rgt, top, bck}, {rgt, btm, bck}, color * sideMlpy2);
  // Front
  AddQuad({rgt, top, fnt}, {lft, top, fnt}, {lft, btm, fnt}, {rgt, btm, fnt}, color * sideMlpy3);
  // Right side
  AddQuad({rgt, top, bck}, {rgt, top, fnt}, {rgt, btm, fnt}, {rgt, btm, bck}, color * sideMlpy4);
  // Top
  AddQuad({lft, top, bck}, {lft, top, fnt}, {rgt, top, fnt}, {rgt, top, bck}, color);
}

void CVisualizationSpectrum::RenderBufferData()
{
  m_colorBufferData.clear();
  m_vertexBufferData.clear();

  // Pre-allocate gl buffer memory
  size_t glBufferDataCapacity = NUM_BANDS * NUM_BANDS * 6 * 2 * 3; // 6 quads
  if (m_colorBufferData.capacity() < glBufferDataCapacity)
    m_colorBufferData.reserve(glBufferDataCapacity);
  if (m_vertexBufferData.capacity() < glBufferDataCapacity)
    m_vertexBufferData.reserve(glBufferDataCapacity);

  for (size_t y = 0; y < NUM_BANDS; y++)
  {
    GLfloat zMid = 3.0f * (0.5f - y / (NUM_BANDS - 1.0f));

    GLfloat blue = y / (NUM_BANDS - 1.0f);

    for (size_t x = 0; x < NUM_BANDS; x++)
    {
      GLfloat xMid = 3.0f * (-0.5f + x / (NUM_BANDS - 1.0f));

      GLfloat green = x / (NUM_BANDS - 1.0f);

      GLfloat red = (1.0f - blue) * (1.0f - green);

      if (m_hSpeed > 0.0f && std::fabs(m_cHeights[y][x] - m_heights[y][x]) > m_hSpeed)
      {
        if (m_cHeights[y][x] < m_heights[y][x])
          m_cHeights[y][x] += m_hSpeed;
        else
          m_cHeights[y][x] -= m_hSpeed;
      }
      else
        m_cHeights[y][x] = m_heights[y][x];

      AddBar(xMid, zMid, m_cHeights[y][x], red, green, blue);
    }
  }
}

void CVisualizationSpectrum::AudioData(const float* pAudioData, size_t iAudioDataLength)
{
  m_samples = 0;

  if (m_channels < 1)
    return;

  m_samples = iAudioDataLength / m_channels;

  size_t freqDataLength = m_samples / 2;

  if (freqDataLength < 1)
    return;

  // Update scale arrays
  if (m_prevFreqDataLength != freqDataLength)
  {
    size_t slope = MIN_FREQS_PER_BAR;
    size_t slopeAmount = slope * NUM_BANDS;

    size_t prevXScale = 0;
    for (size_t x = 0; x < NUM_BANDS; x++)
    {
      size_t xScale = slope * (x + 1);

      if (freqDataLength > slopeAmount)
        xScale += powf(2.0 * (freqDataLength - slopeAmount), (x + 1.0) / NUM_BANDS) * 0.5;

      if (xScale > freqDataLength)
        xScale = freqDataLength;

      size_t lowFreq = prevXScale + 1;
      size_t highFreq = xScale;

      // Calculate bars per octave = 1 / octaves
      m_hScales[x] = 1.0 / log2((highFreq + 0.5) / (lowFreq - 0.5));

      m_xScales[x] = xScale;
      prevXScale = xScale;
    }
  }

  // Update KFFTR
  if (m_prevFreqDataLength != freqDataLength || !m_transform)
  {
    if (freqDataLength > m_freqDataLength || !m_freqData)
    {
      m_freqData.reset(new float[freqDataLength]);
      m_freqDataLength = freqDataLength;
    }

    m_transform.reset(new KFFTR(freqDataLength * 2, m_channels, true));
    m_prevFreqDataLength = freqDataLength;
  }

  // FFT
  m_transform->calc(pAudioData, m_freqData);

  // Shift backwards by one row
  for (int x = 0; x < NUM_BANDS; x++)
  {

    for (int y = NUM_BANDS - 1; y > 0; y--)
    {
      m_heights[y][x] = m_heights[y - 1][x];
      m_cHeights[y][x] = m_cHeights[y - 1][x];
    }
  }

  // Make new heights front row
  size_t prevXScale = 0;
  for (size_t x = 0; x < NUM_BANDS; x++)
  {
    size_t xScale = m_xScales[x];
    GLfloat power = 0.0f;

    // Add up the resulting output power factor avarage over time for the sine waves sum of band
    // In other words: Calculate the square of the root mean square (RMS) value
    // avg( (sin(f1)*a + sin(f2)*b + sin(f3)*c + ... )^2 ) = 0.5 * ( a^2 + b^2 + c^2 + ... )
    //  where
    //   a,b,c,... are the amplitudes aka FreqData magnitudes,
    //   0.5 is the avg of arbitrary sin(f[i])*sin(f[i]) and
    //   0.0 is the avg of arbitrary sin(f[i])*sin(f[j]) nullifying a*b and friends
    for (size_t i = prevXScale; i < xScale && i < freqDataLength; i++)
      power += m_freqData[i] * m_freqData[i];
    power *= 0.5f;

    // Multiply with bands per octave to get power per octave (equalize pink noise)
    power *= m_hScales[x];

    // Calculate bar height
    GLfloat height = 0.0f;
    if (power > 0.0f && m_dbRange > 0)
      height = 10.0f * log10f(power) / m_dbRange + 1.0f;
    if (height < 0.0f)
      height = 0.0f;

    m_heights[0][x] = height * m_hScale;

    prevXScale = xScale;
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
  }
  else if (settingName == "db_range")
  {
    if (value < 1)
      m_dbRange = 48;
    else
      m_dbRange = value;
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
        m_hSpeed = 0.0f; // very fast (no delay)
        break;
      }
    }
  }
  else
    return ADDON_STATUS_UNKNOWN;

  return ADDON_STATUS_OK;
}

ADDONCREATOR(CVisualizationSpectrum)

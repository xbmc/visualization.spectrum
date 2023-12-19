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
#include <chrono>
#include <mutex>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "kfftr.h"

#ifndef M_PI
#define M_PI 3.141592654f
#endif

#define MIN_FREQS_PER_BAR 2 // use at least two freqs per bar

#define COLOR_HEIGHT_ADJUST 0.75f

#define NUM_BUFFER_ROWS 3 // assumed worst case: got three calls of AudioData, but non of Render
#define RENDER_DURATIONS_SIZE 30 // half of a second if @ 60 fps

#define NUM_PRESETS 9

// Templete thresholds - Keep in sync with settings.xml !
// minimum/option label=30100 | step   | name (basename) // member var
#define MIN_NUM_BANDS  (    0 +   16) // num_bands          m_numBands
#define MIN_W_SCALE    (   20 +    5) // bar_width          m_wScale
#define MIN_X_ANGLE    (   -5 +    5) // rotation_x         m_xAngle
#define MIN_Y_SPEED    (   -4 +    1) // rotation_speed     m_ySpeed
#define MIN_Y_OFFSET   ( -155 +    5) // offset_y           m_yOffset
#define MIN_FIELD_SIZE (   45 +    5) // field_size         m_fieldScale
#define MIN_DB_RANGE   (   21 +    3) // db_range           m_dbRange
#define MIN_POINT_SIZE (    0 +    1) // pointsize          m_pointSize
//      MIN_OTHER      (   -1 +    1) // [other]            [other]

enum {
  COLOR_LEGACY,
  COLOR_STACKED,
  COLOR_MONOCHROME,
};

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

  bool GetPresets(std::vector<std::string>& presets) override;
  bool LoadPreset(int select) override;
  bool PrevPreset() override;
  bool NextPreset() override;
  bool RandomPreset() override;
  bool LockPreset(bool lockUnlock) override;
  bool IsLocked() override;

  int GetActivePreset() override;

  ADDON_STATUS Create() override;
  ADDON_STATUS SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue) override;

  void OnCompiledAndLinked() override;
  bool OnEnabled() override;

private:
  void RenderBufferData();

  std::string SettingNameExtension(int presetIndex);

  std::mt19937 m_randomGenerator;

  std::mutex m_mutex;

  std::unique_ptr<KFFTR> m_transform;
  std::unique_ptr<float[]> m_freqData;

  std::vector<size_t> m_xScales;
  std::vector<GLfloat> m_hScales;
  std::vector<GLfloat> m_heights;
  std::vector<GLfloat> m_cHeights;
  std::vector<GLfloat> m_renderDurations;

  bool m_glInitialized = false;
  bool m_hScaleIsFlat = false;
  bool m_presetLocked = false;
  bool m_shadersLoaded = false;
  bool m_smoothAnim = false;

  int m_channels = 0;
  int m_colorMode = COLOR_STACKED;
  int m_dbRange = 48;
  int m_presetIndex = 0;
  int m_samplesPerSec = 0;

  size_t m_freqDataLength = 0;
  size_t m_prevFreqDataLength = 0;

  size_t m_heightsRingBufBegin = 0;

  size_t m_numBands = 0;

  size_t m_samples = 0;

  GLfloat m_fieldScale = 3.0f;

  GLfloat m_hScale = 1.0f;
  GLfloat m_hSpeed = 0.05f;

  GLfloat m_pointSize = 0.0f;

  GLfloat m_lastRenderTimestamp = 0.0f;
  GLfloat m_renderDuration = 0.0f;

  GLfloat m_wScale = 0.5f;

  GLfloat m_xAngle = 10.0f; // around 11° on 16:9 looks similar to the former 20° on squeezed 1:1
  GLfloat m_yAngle = 45.0f;
  GLfloat m_zAngle = 0.0f;

  GLfloat m_yOffset = -0.25f;
  GLfloat m_yRenderOffset = 0.0f;
  GLfloat m_yReverse = 1.0f;
  GLfloat m_ySpeed = 0.5f;

  GLenum m_drawMode = GL_TRIANGLES;

  // Shader related data

  glm::vec3 m_topColor = {1.0f, 1.0f, 1.0f};
  glm::vec3 m_bottomColor = {0.05f, 0.05f, 0.05f};

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
  std::unique_lock<std::mutex> lock(m_mutex);

  std::random_device rd;
  std::mt19937 m_randomGenerator(rd()); // random seed

  m_yAngle = kodi::addon::GetSettingInt("rotation_angle" + SettingNameExtension(-1), 45); // one time init from template
}

bool CVisualizationSpectrum::Start(int channels, int samplesPerSec, int bitsPerSample, const std::string& songName)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  kodi::Log(ADDON_LOG_DEBUG,
    "%s: m_yRenderOffset: %f - channels, samplesPerSec, bitsPerSample: %d %d %d",
    __func__, m_yRenderOffset, channels, samplesPerSec, bitsPerSample);

  m_channels = channels;
  m_samplesPerSec = samplesPerSec;
  (void)bitsPerSample;
  (void)songName;

  m_samples = 0;

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

#ifdef HAS_GL
  glGenBuffers(2, m_vertexVBO);
#endif

  m_glInitialized = true;

  return true;
}

void CVisualizationSpectrum::Stop()
{
  std::unique_lock<std::mutex> lock(m_mutex);

  kodi::Log(ADDON_LOG_DEBUG, "%s", __func__);

  m_channels = 0;
  m_samplesPerSec = 0;
  m_samples = 0;

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
  std::unique_lock<std::mutex> lock(m_mutex);

  if (!m_glInitialized)
    return;

  // Update m_renderDuration - Note: Also used with m_hSpeed
  {
    using namespace std::chrono;

    // Get monotonic time duration since last render (in seconds)
    GLfloat renderTimestamp = duration<GLfloat>(steady_clock::now().time_since_epoch()).count();
    GLfloat renderDuration = m_lastRenderTimestamp > 0.0 ? renderTimestamp - m_lastRenderTimestamp : 0.0;
    m_lastRenderTimestamp = renderTimestamp;

    if (renderDuration > 0.0)
    {
      m_renderDuration *= m_renderDurations.size();
      m_renderDuration += renderDuration;
      m_renderDurations.push_back(renderDuration);
      while (m_renderDurations.size() > RENDER_DURATIONS_SIZE)
      {
        m_renderDuration -= m_renderDurations[0];
        m_renderDurations.erase(m_renderDurations.begin());
      }
      m_renderDuration /= m_renderDurations.size();
    }
  }

  if (m_smoothAnim)
  {
    GLfloat audioDuration = 0.0;
    if (m_samples > 0 && m_samplesPerSec > 0)
    {
      audioDuration = 1.0 * m_samples / m_samplesPerSec;
      m_yRenderOffset -= m_renderDuration / audioDuration;
    }
    if (m_yRenderOffset < 0.0) // playback paused (or buffer underrun)
      m_yRenderOffset = 0.0;

    kodi::Log(ADDON_LOG_DEBUG,
      "%s: m_yRenderOffset: %f - m_renderDuration, audioDuration, m_samples/m_samplesPerSec: %f %f %d/%d",
      __func__, m_yRenderOffset, m_renderDuration, audioDuration, m_samples, m_samplesPerSec);
  }
  else
    m_yRenderOffset = 0.0;

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

  // Note: For flat bar mode, all surfaces need to be visible always
  if (!m_hScaleIsFlat)
  {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
  }

  // Clear the screen
  glClear(GL_DEPTH_BUFFER_BIT);

  GLfloat dar = (float)Width() / (float)Height();
  GLfloat par = PixelRatio();

  kodi::Log(ADDON_LOG_DEBUG, "%s: dar, par: %f %f", __func__, dar, par);

  m_yAngle = std::fmod(m_yAngle + m_ySpeed * m_yReverse, 360.0f);

  m_projMat = glm::frustum(-1.0f, 1.0f, -1.0f / dar / par, 1.0f / dar / par, 1.5f, 10.0f);

  m_modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, m_yOffset, -5.0f));

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

  if (!m_hScaleIsFlat)
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

void CVisualizationSpectrum::RenderBufferData()
{
  m_colorBufferData.clear();
  m_vertexBufferData.clear();

  if (m_numBands < 2) // lone bar not supported
    return;

  size_t xMax = m_numBands - 1;
  size_t yMax = m_numBands - 1 + NUM_BUFFER_ROWS;
  size_t iMax = m_numBands * yMax + xMax;

  if (iMax != m_cHeights.size() - 1 || iMax != m_heights.size() - 1)
    return;

  // Pre-allocate gl buffer memory
  size_t glBufferDataCapacity = m_numBands * (m_numBands + 1); // one more row for avoid jitter
  size_t stackHeight = m_colorMode == COLOR_STACKED ? 2 : 1;
  if (m_drawMode != GL_POINTS)
  {
    if (m_hScaleIsFlat)
      glBufferDataCapacity *= 1 * 2 * 3; // one quad only
    else if (m_drawMode == GL_TRIANGLES)
      glBufferDataCapacity *= (2 + stackHeight * 4) * 2 * 3; // 6 or 10 quads ...
    else if (m_drawMode == GL_LINES)
      glBufferDataCapacity *= (2 * 4 + stackHeight * 4) * 2; // ... or 2 lines-quads + 4 or 8 side lines
  }
  if (m_colorBufferData.capacity() < glBufferDataCapacity)
    m_colorBufferData.reserve(glBufferDataCapacity);
  if (m_vertexBufferData.capacity() < glBufferDataCapacity)
    m_vertexBufferData.reserve(glBufferDataCapacity);

  GLfloat halfBarWidth = m_fieldScale / m_numBands * m_wScale * 0.5f;

  GLfloat halfTopColorHeight = m_hScale * COLOR_HEIGHT_ADJUST * 0.5f;

  GLfloat lft = 0.0f;
  GLfloat rgt = 0.0f;
  GLfloat bck = 0.0f;
  GLfloat fnt = 0.0f;

  auto addLinesQuad = [&](GLfloat y, glm::vec3 color)
  {
    m_vertexBufferData.insert(m_vertexBufferData.end(),
    {
      {lft, y, fnt}, {rgt, y, fnt}, // front
      {lft, y, bck}, {rgt, y, bck}, // back
      {lft, y, fnt}, {lft, y, bck}, // left
      {rgt, y, fnt}, {rgt, y, bck}, // right
    });
    m_colorBufferData.insert(m_colorBufferData.end(), 8, color);
  };

  // Note: Triangle vertices must be in counter-clock-wise order for face-culling.

  auto addTop = [&](GLfloat top, glm::vec3 cTop)
  {
    if (m_drawMode == GL_TRIANGLES)
    {
      m_vertexBufferData.insert(m_vertexBufferData.end(),
      {
        {lft, top, bck}, {lft, top, fnt}, {rgt, top, fnt}, {rgt, top, fnt}, {rgt, top, bck}, {lft, top, bck}, // top
      });
      m_colorBufferData.insert(m_colorBufferData.end(), 6, cTop);
      return;
    }
    if (m_drawMode == GL_LINES)
    {
      addLinesQuad(top, cTop);
      return;
    }
    // m_drawMode == GL_POINTS
    m_vertexBufferData.push_back({(lft + rgt) * 0.5f, top, (fnt + bck) * 0.5f}); // top mid
    m_colorBufferData.push_back(cTop);
  };

  auto addBottom = [&](glm::vec3 cBtm)
  {
    if (m_drawMode == GL_TRIANGLES)
    {
      m_vertexBufferData.insert(m_vertexBufferData.end(),
      {
        {rgt, 0.0f, fnt}, {lft, 0.0f, fnt}, {lft, 0.0f, bck}, {lft, 0.0f, bck}, {rgt, 0.0f, bck}, {rgt, 0.0f, fnt}, // bottom
      });
      m_colorBufferData.insert(m_colorBufferData.end(), 6, cBtm);
      return;
    }
    if (m_drawMode == GL_LINES)
      addLinesQuad(0.0f, cBtm);
  };

  auto addSides = [&](GLfloat btm, glm::vec3 cBtm, GLfloat top, glm::vec3 cTop)
  {
    if (m_drawMode == GL_TRIANGLES)
    {
      m_vertexBufferData.insert(m_vertexBufferData.end(),
      {
        {rgt, top, fnt}, {lft, top, fnt}, {lft, btm, fnt}, {lft, btm, fnt}, {rgt, btm, fnt}, {rgt, top, fnt}, // front
        {lft, btm, bck}, {lft, top, bck}, {rgt, top, bck}, {rgt, top, bck}, {rgt, btm, bck}, {lft, btm, bck}, // back
        {lft, btm, fnt}, {lft, top, fnt}, {lft, top, bck}, {lft, top, bck}, {lft, btm, bck}, {lft, btm, fnt}, // left
        {rgt, top, bck}, {rgt, top, fnt}, {rgt, btm, fnt}, {rgt, btm, fnt}, {rgt, btm, bck}, {rgt, top, bck}, // right
      });

      glm::vec3 cTopFnt = cTop * 0.75f;
      glm::vec3 cBtmFnt = cBtm * 0.75f;
      glm::vec3 cTopBck = cTop * 0.25f;
      glm::vec3 cBtmBck = cBtm * 0.25f;
      glm::vec3 cTopSde = cTop * 0.50f;
      glm::vec3 cBtmSde = cBtm * 0.50f;

      m_colorBufferData.insert(m_colorBufferData.end(),
      {
        cTopFnt, cTopFnt, cBtmFnt, cBtmFnt, cBtmFnt, cTopFnt, // front
        cBtmBck, cTopBck, cTopBck, cTopBck, cBtmBck, cBtmBck, // back
        cBtmSde, cTopSde, cTopSde, cTopSde, cBtmSde, cBtmSde, // left
        cTopSde, cTopSde, cBtmSde, cBtmSde, cBtmSde, cTopSde, // right
      });
      return;
    }
    if (m_drawMode == GL_LINES)
    {
      m_vertexBufferData.insert(m_vertexBufferData.end(),
      {
        {lft, btm, fnt}, {lft, top, fnt}, // front left
        {rgt, btm, fnt}, {rgt, top, fnt}, // front right
        {lft, btm, bck}, {lft, top, bck}, // back left
        {rgt, btm, bck}, {rgt, top, bck}, // back right
      });
      m_colorBufferData.insert(m_colorBufferData.end(), {cBtm, cTop, cBtm, cTop, cBtm, cTop, cBtm, cTop,}); // four lines
    }
  };

  GLfloat xRenderMax = m_numBands - 1.0f;
  GLfloat yRenderMax = m_numBands - 1.0f;
  GLfloat yRenderMin = 0.0f;

  bool stopRender = false;

  for (size_t y = 0; y <= yMax && !stopRender; y++)
  {
    GLfloat yRender = y - m_yRenderOffset;

    // avoid jitter effect
    if (yRender < yRenderMin)
      if (y == 0)
        yRender = yRenderMin;
      else
        continue;
    else
      if (yRender >= yRenderMax)
      {
        yRender = yRenderMax;
        stopRender = true;
      }

    GLfloat zMid = m_fieldScale * (0.5f - (yRender + 0.5f) / m_numBands);

    bck = zMid - halfBarWidth;
    fnt = zMid + halfBarWidth;

    for (size_t x = 0; x <= xMax; x++)
    {
      GLfloat xMid = m_fieldScale * (-0.5f + (x + 0.5f) / m_numBands);

      lft = xMid - halfBarWidth;
      rgt = xMid + halfBarWidth;

      size_t i = (m_heightsRingBufBegin + m_numBands * y + x) % m_heights.size();

      GLfloat height = m_cHeights[i];
      GLfloat heightTarget = m_heights[i] * m_hScale;
      GLfloat heigthStep = m_hSpeed;
      if (m_renderDuration > 0.0f)
        heigthStep *= m_renderDuration * 60.0f; // take fps into account if other than 60

      // Animate height change
      if (heigthStep > 0.0f && heigthStep < std::fabs(height - heightTarget))
      {
        if (height < heightTarget)
          height += heigthStep;
        else
          height -= heigthStep;
      }
      else
        height = heightTarget;

      m_cHeights[i] = height;

      GLfloat colorRate = height / halfTopColorHeight;

      if (m_colorMode != COLOR_STACKED)
      {
        colorRate *= 0.5f; // full height bar
      }
      else if (height > halfTopColorHeight) // stack two bars
      {
        colorRate -= 1.0f;

        glm::vec3 topColor2 = {0.8f, 0.0f, 0.0f}; // darker red
        topColor2 = m_topColor + (topColor2 - m_topColor) * colorRate;

        if (m_hScaleIsFlat)
        {
          addTop(0.0f, topColor2); // note: face-culling is disabled
          continue;
        }

        addTop(height, topColor2);
        addSides(halfTopColorHeight, m_topColor, height, topColor2);
        addSides(0.0f, m_bottomColor, halfTopColorHeight, m_topColor);
        addBottom(m_bottomColor);
        continue;
      }

      if (m_colorMode != COLOR_LEGACY)
      {
        glm::vec3 topColor = m_bottomColor + (m_topColor - m_bottomColor) * colorRate;

        if (m_hScaleIsFlat)
        {
          addTop(0.0f, topColor); // note: face-culling is disabled
          continue;
        }

        addTop(height, topColor);
        addSides(0.0f, m_bottomColor, height, topColor);
        addBottom(m_bottomColor);
        continue;
      }

      // Legacy mode
      GLfloat green = x / xRenderMax;
      GLfloat blue = yRender / yRenderMax;
      GLfloat red = (1.0f - blue) * (1.0f - green);

      glm::vec3 topColor = {red, green, blue};
      glm::vec3 bottomColor = topColor;

      if (m_hScaleIsFlat)
      {
        bottomColor *= 0.05f;
        topColor = bottomColor + (topColor - bottomColor) * colorRate;

        addTop(0.0f, topColor); // note: face-culling is disabled
        continue;
      }

      addTop(height, topColor);
      addSides(0.0f, bottomColor, height, topColor);
      addBottom(bottomColor);
    }
  }
}

void CVisualizationSpectrum::AudioData(const float* pAudioData, size_t iAudioDataLength)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  // Update bar heights arrays
  size_t heightsSize = m_numBands * (m_numBands + NUM_BUFFER_ROWS);
  if (m_heights.size() != heightsSize || m_cHeights.size() != heightsSize)
  {
    m_heights.clear();
    m_cHeights.clear();

    m_heights.resize(heightsSize);
    m_cHeights.resize(heightsSize);
  }

  if (m_numBands < 2) // lone bar not supported
    return;

  m_samples = 0;

  if (m_channels < 1)
    return;

  m_samples = iAudioDataLength / m_channels;

  size_t freqDataLength = m_samples / 2;

  if (freqDataLength < 1)
    return;

  // Update scale arrays
  if (m_prevFreqDataLength != freqDataLength || m_hScales.size() != m_numBands || m_xScales.size() != m_numBands)
  {
    m_hScales.clear();
    m_xScales.clear();

    size_t slope = MIN_FREQS_PER_BAR;
    size_t slopeAmount = slope * m_numBands;

    size_t prevXScale = 0;
    for (size_t x = 0; x < m_numBands; x++)
    {
      size_t xScale = slope * (x + 1);

      if (freqDataLength > slopeAmount)
        xScale += powf(2.0 * (freqDataLength - slopeAmount), (x + 1.0) / m_numBands) * 0.5;

      if (xScale > freqDataLength)
        xScale = freqDataLength;

      size_t lowFreq = prevXScale + 1;
      size_t highFreq = xScale;

      // Calculate bars per octave = 1 / octaves
      m_hScales.push_back(1.0 / log2((highFreq + 0.5) / (lowFreq - 0.5)));

      m_xScales.push_back(xScale);
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

  // Step backwards by one row
  m_heightsRingBufBegin = (m_heightsRingBufBegin + (m_heights.size() - m_numBands)) % m_heights.size();

  // Duplicate cHeights front row
  for (size_t i = m_heightsRingBufBegin; i < m_heightsRingBufBegin + m_numBands; i++)
  {
    size_t j = i + m_numBands;
    m_cHeights[i % m_cHeights.size()] = m_cHeights[j % m_cHeights.size()];
  }

  // Make new heights front row
  size_t prevXScale = 0;
  for (size_t x = 0; x < m_numBands; x++)
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

    size_t i = (m_heightsRingBufBegin + x) % m_heights.size();
    m_heights[i] = height;

    prevXScale = xScale;
  }

  if (m_smoothAnim)
  {
    GLfloat yRenderOffsetMax = NUM_BUFFER_ROWS;
    bool overrun = false;

    m_yRenderOffset += 1.0;

    if (m_yRenderOffset > yRenderOffsetMax)
    {
      m_yRenderOffset = yRenderOffsetMax;
      overrun = true;
    }

    kodi::Log(ADDON_LOG_DEBUG, "%s: m_yRenderOffset: %f%s", __func__, m_yRenderOffset, overrun ? " overrun" : "");
  }
}

std::string CVisualizationSpectrum::SettingNameExtension(int presetIndex)
{
  if (presetIndex == 0) // suppress _1 extension on legacy preset
    return ("");
  else
    return ("_" + std::to_string(presetIndex + 1));
}

int CVisualizationSpectrum::GetActivePreset()
{
  return m_presetIndex;
}

bool CVisualizationSpectrum::GetPresets(std::vector<std::string>& presets)
{
  presets.clear();

  for (int i = 0; i < NUM_PRESETS; i++)
  {
    std::string n = std::to_string(i + 1);

    std::string ext = SettingNameExtension(i);

    presets.push_back("#" + n + " " + kodi::addon::GetSettingString("name" + ext, ""));
  }
  return true;
}

bool CVisualizationSpectrum::LockPreset(bool lockUnlock)
{
  m_presetLocked = lockUnlock;
  return true;
}

bool CVisualizationSpectrum::IsLocked()
{
  return(m_presetLocked);
}

bool CVisualizationSpectrum::LoadPreset(int select)
{
  kodi::Log(ADDON_LOG_DEBUG, "%s %d", __func__, select);

  if (select < 0 || select > NUM_PRESETS - 1)
    return false;

  // Note: On change Kodi will do SetSetting callbacks
  kodi::addon::SetSettingInt("active_preset", select);

  return true;
}

bool CVisualizationSpectrum::PrevPreset()
{
  if (m_presetLocked)
    return false;

  int prev = m_presetIndex - 1 + NUM_PRESETS;

  for (int i = prev; i > prev - NUM_PRESETS; i--)
  {
    std::string ext = SettingNameExtension(i % NUM_PRESETS);

    if (!kodi::addon::GetSettingBoolean("skip_preset" + ext, false))
      return LoadPreset(i % NUM_PRESETS);
  }
  return false;
}

bool CVisualizationSpectrum::NextPreset()
{
  if (m_presetLocked)
    return false;

  int next = m_presetIndex + 1;

  for (int i = next; i < next + NUM_PRESETS; i++)
  {
    std::string ext = SettingNameExtension(i % NUM_PRESETS);

    if (!kodi::addon::GetSettingBoolean("skip_preset" + ext, false))
      return LoadPreset(i % NUM_PRESETS);
  }
  return false;
}

bool CVisualizationSpectrum::RandomPreset()
{
  if (m_presetLocked)
    return false;

  std::vector<int> presetIndexes;

  for (int i = 0; i < NUM_PRESETS; i++)
  {
    if (i == m_presetIndex)
      continue;

    std::string ext = SettingNameExtension(i);

    if (!kodi::addon::GetSettingBoolean("skip_preset" + ext, false))
      presetIndexes.push_back(i);
  }
  if (presetIndexes.size() < 1)
    return false;

  std::uniform_int_distribution<>distrib(0, presetIndexes.size() - 1);

  return LoadPreset(presetIndexes[distrib(m_randomGenerator)]);
}

ADDON_STATUS CVisualizationSpectrum::Create()
{
  return ADDON_STATUS_NEED_SETTINGS;
}

ADDON_STATUS CVisualizationSpectrum::SetSetting(const std::string& settingName, const kodi::addon::CSettingValue& settingValue)
{
  if (settingName.empty() || settingValue.empty())
    return ADDON_STATUS_UNKNOWN;

  std::unique_lock<std::mutex> lock(m_mutex);

  for (int presetIndex : {-1, m_presetIndex}) // only match template and current preset
  {
    std::string ext = SettingNameExtension(presetIndex);

    if (settingName == "name" + ext)
    {
      // just help to debug
      kodi::Log(ADDON_LOG_DEBUG, "%s: %s: %s", __func__, settingName.c_str(), settingValue.GetString().c_str());
      return ADDON_STATUS_OK;
    }

    int value = settingValue.GetInt();

    if (settingName == "active_preset")
    {
      m_presetIndex = value;
    }
    else if (settingName == "bar_height" + ext && value >= 0)
    {
      m_hScale = 0.56f; // try to match avarage heights on former squeezed 1:1-frustum at least on 16:9 displays
      m_hScaleIsFlat = false;

      switch (value)
      {
        case 0:
        {
          m_hScale *= 0.5f; // small
          break;
        }
        case 1:
        default:
        {
          m_hScale *= 1.0f; // default
          break;
        }
        case 2:
        {
          m_hScale *= 2.0f; // big
          break;
        }
        case 3:
        {
          m_hScale *= 3.0f; // very big
          break;
        }
        case 4:
        {
          m_hScale *= 0.125f; // very small (used for heights animation)
          m_hScaleIsFlat = true; // flat
          break;
        }
      }
    }
    else if (settingName == "bar_width" + ext && value >= MIN_W_SCALE)
    {
      m_wScale = value / 100.0f;
    }
    else if (settingName == "color_mode" + ext && value >= 0)
    {
      m_colorMode = COLOR_MONOCHROME;

      switch (value)
      {
        case 0: // legacy mode
        {
          m_colorMode = COLOR_LEGACY;
          break;
        }
        case 1: // stack green->red bar on top of blue->green base bar
        default:
        {
          m_colorMode = COLOR_STACKED;

          m_topColor = {0.0f, 0.8f, 0.0f}; // darker green
          m_bottomColor = {0.0f, 0.0f, 1.0f}; // blue
          break;
        }
        case 2:
        {
          m_topColor = {1.0f, 0.8f, 0.0f}; // amber
          break;
        }
        case 3:
        {
          m_topColor = {0.0f, 0.0f, 1.0f}; // blue
          break;
        }
        case 4:
        {
          m_topColor = {0.0f, 1.0f, 0.0f}; // green
          break;
        }
      }
      if (m_colorMode == COLOR_MONOCHROME)
        m_bottomColor = m_topColor * 0.05f;
    }
    else if (settingName == "db_range" + ext && value >= MIN_DB_RANGE)
    {
      if (value < 1)
        m_dbRange = 48;
      else
        m_dbRange = value;
    }
    else if (settingName == "field_size" + ext && value >= MIN_FIELD_SIZE)
    {
      m_fieldScale = value / 100.0f * 3.0f;
    }
    else if (settingName == "mode" + ext && value >= 0)
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
    else if (settingName == "num_bands" + ext && value >= MIN_NUM_BANDS)
    {
      if (value < 2) // lone bar not supported
        m_numBands = 2;
      else
        m_numBands = value;
    }
    else if (settingName == "offset_y" + ext && value >= MIN_Y_OFFSET)
    {
      m_yOffset = value / 100.0f;
    }
    else if (settingName == "pointsize" + ext && value >= MIN_POINT_SIZE)
    {
      m_pointSize = value;
    }
    else if (settingName == "rotation_angle" + ext && value >= 0 && presetIndex >= 0)
    {
      m_yAngle = value; // override
    }
    else if (settingName == "rotation_reverse" + ext)
    {
      if (presetIndex == -1)
        m_yReverse = 1.0f; // init
      if (value == 1)
        m_yReverse *= -1.0f; // invert
    }
    else if (settingName == "rotation_speed" + ext && value >= MIN_Y_SPEED)
    {
      if (value == -3) // zero
        m_ySpeed = 0.0f;
      else
        m_ySpeed = powf(2.0f, value) * 0.5f;
    }
    else if (settingName == "rotation_x" + ext && value >= MIN_X_ANGLE)
    {
      m_xAngle = value;
    }
    else if (settingName == "smooth_anim" + ext && value >= 0)
    {
      switch (value)
      {
        case 0:
        {
          m_smoothAnim = false;
          break;
        }
        case 1:
        default:
        {
          m_smoothAnim = true;
          break;
        }
      }
    }
    else if (settingName == "speed" + ext && value >= 0)
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
      continue;

    kodi::Log(ADDON_LOG_DEBUG, "%s: %s: %d", __func__, settingName.c_str(), settingValue.GetInt());

    return ADDON_STATUS_OK;
  }

  // If no match, ignore setting
  return ADDON_STATUS_OK;
}

ADDONCREATOR(CVisualizationSpectrum)

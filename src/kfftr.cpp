/*
 *  Copyright (C) 2015-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "kfftr.h"

#if defined(TARGET_WINDOWS) && !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

KFFTR::KFFTR(int size, int channels, bool windowed) :
    m_size(size), m_channels(channels), m_windowed(windowed)
{
    m_cfg = kiss_fftr_alloc(m_size,0,nullptr,nullptr);
}

KFFTR::~KFFTR()
{
    // we don' use kiss_fftr_free here because
    // its hardcoded to free and doesn't pay attention
    // to SIMD (which might be used during kiss_fftr_alloc
    //in the C'tor).
    KISS_FFT_FREE(m_cfg);
}

void KFFTR::calc(const float* input, const std::unique_ptr<float[]>& output)
{
    // temporary buffers
    std::vector<kiss_fft_scalar> sumInput(m_size, 0.0);
    std::vector<kiss_fft_cpx> sumOutput(m_size / 2 + 1);

    for (size_t i = 0; i < m_size; ++i)
        for (size_t j = 0; j < m_channels; j++)
          sumInput[i] += input[m_channels * i + j];

    if (m_windowed)
        hann(sumInput);

    // transform channels
    kiss_fftr(m_cfg, &sumInput[0], &sumOutput[0]);

    auto&& filter = [&](kiss_fft_cpx& data)
    {
        return sqrt(data.r * data.r + data.i * data.i) * 2.0 / (m_size * m_channels) * (m_windowed ? sqrt(8.0 / 3.0) : 1.0);
    };

    // take magnitudes and normalize discarding DC bin
    for (size_t i = 0; i < m_size / 2; ++i)
        output[i] = filter(sumOutput[i + 1]);
}

#include <iostream>

void KFFTR::hann(std::vector<kiss_fft_scalar>& data)
{
    for (size_t i = 0; i < data.size(); ++i)
        data[i] *= 0.5 * (1.0 - cos(2.0 * M_PI * i / (data.size() - 1.0)));
}

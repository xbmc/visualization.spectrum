/*
 *  Copyright (C) 2015-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "kiss_fftr.h"
#include <memory>
#include <vector>

//! \brief Class performing a RFFT of sum data of interleaved multi-channel data.
class KFFTR
{
public:
    //! \brief The constructor creates a RFFT plan.
    //! \brief size Length of input data divided by channels.
    //! \brief channels Number of channels.
    //! \brief windowed Whether or not to apply a Hann window to data.
    KFFTR(int size, int channels, bool windowed=false);

    //! \brief Free the RFFT plan
    ~KFFTR();

    //! \brief Calculate FFTs
    //! \param input Input data of length 2 * size * channels.
    //! \param output Output data of length size with DC discarded.
    void calc(const float* input, const std::unique_ptr<float[]>& output);

protected:
    //! \brief Apply a Hann window to a buffer.
    //! \param data Vector with data to apply window to.
    static void hann(std::vector<kiss_fft_scalar>& data);

    size_t m_size;           //!< Length of input data divided by channels.
    size_t m_channels;       //!< Number of channels.
    bool m_windowed;         //!< Whether or not a Hann window is applied.
    kiss_fftr_cfg m_cfg;     //!< FFT plan
};

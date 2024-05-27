/*****************************************************************************
 * Copyright (c) 2014-2024 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "MiniAudio.h"

#include <memory>
#include <openrct2/audio/AudioChannel.h>
#include <openrct2/audio/AudioSource.h>
#include <openrct2/common.h>
#include <string>

struct SDL_RWops;
using ResamplerState = ma_resampler;

namespace OpenRCT2::Audio
{
    struct AudioFormat;
    struct IAudioContext;

    struct ISDLAudioChannel : public IAudioChannel
    {
        [[nodiscard]] virtual AudioFormat GetFormat() const abstract;
        [[nodiscard]] virtual ResamplerState* GetResampler() const abstract;
        virtual void SetResampler(ResamplerState* value) abstract;
    };

    namespace AudioChannel
    {
        ISDLAudioChannel* Create();
    }

    [[nodiscard]] std::unique_ptr<IAudioContext> CreateAudioContext();

} // namespace OpenRCT2::Audio

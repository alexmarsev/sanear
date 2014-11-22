#pragma once

#ifndef NOMINMAX
#   define NOMINMAX
#endif

#include <windows.h>

#include <streams.h>

#include <audioclient.h>
#include <comdef.h>
#include <malloc.h>
#include <mmdeviceapi.h>
#include <process.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <random>

#include "Utils.h"

namespace SaneAudioRenderer
{
    _COM_SMARTPTR_TYPEDEF(IMMDeviceEnumerator, __uuidof(IMMDeviceEnumerator));
    _COM_SMARTPTR_TYPEDEF(IMMDevice, __uuidof(IMMDevice));

    _COM_SMARTPTR_TYPEDEF(IAudioClient, __uuidof(IAudioClient));
    _COM_SMARTPTR_TYPEDEF(IAudioRenderClient, __uuidof(IAudioRenderClient));
    _COM_SMARTPTR_TYPEDEF(IAudioClock, __uuidof(IAudioClock));

    _COM_SMARTPTR_TYPEDEF(IMediaSample, __uuidof(IMediaSample));
    _COM_SMARTPTR_TYPEDEF(IBasicAudio, __uuidof(IBasicAudio));
}

﻿/*
 * Carla Plugin
 * Copyright (C) 2011-2013 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the doc/GPL.txt file.
 */

#ifndef CARLA_PLUGIN_INTERNAL_HPP_INCLUDED
#define CARLA_PLUGIN_INTERNAL_HPP_INCLUDED

#include "CarlaPlugin.hpp"
#include "CarlaPluginThread.hpp"
#include "CarlaEngine.hpp"

#include "CarlaBackendUtils.hpp"
#include "CarlaOscUtils.hpp"
#include "CarlaStateUtils.hpp"
#include "CarlaMutex.hpp"
#include "RtLinkedList.hpp"

#ifdef HAVE_JUCE
# include "juce_audio_basics.h"
using juce::FloatVectorOperations;
#endif

#include <cmath>

// -----------------------------------------------------------------------

#define CARLA_PROCESS_CONTINUE_CHECK if (! pData->enabled) { pData->engine->callback(ENGINE_CALLBACK_DEBUG, pData->id, 0, 0, 0.0f, "Processing while plugin is disabled!!"); return; }

// -----------------------------------------------------------------------
// Float operations

#ifdef HAVE_JUCE
# define FLOAT_ADD(bufDst, bufSrc, frames)  FloatVectorOperations::add(bufDst, bufSrc, frames)
# define FLOAT_COPY(bufDst, bufSrc, frames) FloatVectorOperations::copy(bufDst, bufSrc, frames)
# define FLOAT_CLEAR(buf, frames)           FloatVectorOperations::clear(buf, frames)
#else
# define FLOAT_ADD(bufDst, bufSrc, frames)  carla_addFloat(bufDst, bufSrc, frames)
# define FLOAT_COPY(bufDst, bufSrc, frames) carla_copyFloat(bufDst, bufSrc, frames)
# define FLOAT_CLEAR(buf, frames)           carla_zeroFloat(buf, frames)
#endif

CARLA_BACKEND_START_NAMESPACE

#if 0
} // Fix editor indentation
#endif

// -----------------------------------------------------------------------

const unsigned short kPluginMaxMidiEvents = 512;

const unsigned int PLUGIN_EXTRA_HINT_HAS_MIDI_IN   = 0x01;
const unsigned int PLUGIN_EXTRA_HINT_HAS_MIDI_OUT  = 0x02;
const unsigned int PLUGIN_EXTRA_HINT_CAN_RUN_RACK  = 0x04;

// -----------------------------------------------------------------------

/*!
 * Post-RT event type.\n
 * These are events postponned from within the process function,
 *
 * During process, we cannot lock, allocate memory or do UI stuff,\n
 * so events have to be postponned to be executed later, on a separate thread.
 */
enum PluginPostRtEventType {
    kPluginPostRtEventNull,
    kPluginPostRtEventDebug,
    kPluginPostRtEventParameterChange,   // param, SP (*), value (SP: if 1, don't report change to Callback and OSC)
    kPluginPostRtEventProgramChange,     // index
    kPluginPostRtEventMidiProgramChange, // index
    kPluginPostRtEventNoteOn,            // channel, note, velo
    kPluginPostRtEventNoteOff            // channel, note
};

/*!
 * A Post-RT event.
 * \see PluginPostRtEventType
 */
struct PluginPostRtEvent {
    PluginPostRtEventType type;
    int32_t value1;
    int32_t value2;
    float   value3;
};

struct ExternalMidiNote {
    int8_t  channel; // invalid if -1
    uint8_t note;    // 0 to 127
    uint8_t velo;    // note-off if 0
};

// -----------------------------------------------------------------------

struct PluginAudioPort {
    uint32_t rindex;
    CarlaEngineAudioPort* port;

    PluginAudioPort() noexcept
        : rindex(0),
          port(nullptr) {}

    ~PluginAudioPort()
    {
        CARLA_ASSERT(port == nullptr);
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginAudioPort)
};

struct PluginAudioData {
    uint32_t count;
    PluginAudioPort* ports;

    PluginAudioData() noexcept
        : count(0),
          ports(nullptr) {}

    ~PluginAudioData()
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT(ports == nullptr);
    }

    void createNew(const uint32_t newCount)
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT(ports == nullptr);
        CARLA_ASSERT_INT(newCount > 0, newCount);

        if (ports != nullptr || newCount == 0)
            return;

        ports = new PluginAudioPort[newCount];
        count = newCount;
    }

    void clear()
    {
        if (ports != nullptr)
        {
            for (uint32_t i=0; i < count; ++i)
            {
                if (ports[i].port != nullptr)
                {
                    delete ports[i].port;
                    ports[i].port = nullptr;
                }
            }

            delete[] ports;
            ports = nullptr;
        }

        count = 0;
    }

    void initBuffers()
    {
        for (uint32_t i=0; i < count; ++i)
        {
            if (ports[i].port != nullptr)
                ports[i].port->initBuffer();
        }
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginAudioData)
};

// -----------------------------------------------------------------------

struct PluginCVPort {
    uint32_t rindex;
    uint32_t param;
    CarlaEngineCVPort* port;

    PluginCVPort() noexcept
        : rindex(0),
          param(0),
          port(nullptr) {}

    ~PluginCVPort()
    {
        CARLA_ASSERT(port == nullptr);
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginCVPort)
};

struct PluginCVData {
    uint32_t count;
    PluginCVPort* ports;

    PluginCVData() noexcept
        : count(0),
          ports(nullptr) {}

    ~PluginCVData()
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT(ports == nullptr);
    }

    void createNew(const uint32_t newCount)
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT(ports == nullptr);
        CARLA_ASSERT_INT(newCount > 0, newCount);

        if (ports != nullptr || newCount == 0)
            return;

        ports = new PluginCVPort[newCount];
        count = newCount;
    }

    void clear()
    {
        if (ports != nullptr)
        {
            for (uint32_t i=0; i < count; ++i)
            {
                if (ports[i].port != nullptr)
                {
                    delete ports[i].port;
                    ports[i].port = nullptr;
                }
            }

            delete[] ports;
            ports = nullptr;
        }

        count = 0;
    }

    void initBuffers()
    {
        for (uint32_t i=0; i < count; ++i)
        {
            if (ports[i].port != nullptr)
                ports[i].port->initBuffer();
        }
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginCVData)
};

// -----------------------------------------------------------------------

struct PluginEventData {
    CarlaEngineEventPort* portIn;
    CarlaEngineEventPort* portOut;

    PluginEventData() noexcept
        : portIn(nullptr),
          portOut(nullptr) {}

    ~PluginEventData()
    {
        CARLA_ASSERT(portIn == nullptr);
        CARLA_ASSERT(portOut == nullptr);
    }

    void clear()
    {
        if (portIn != nullptr)
        {
            delete portIn;
            portIn = nullptr;
        }

        if (portOut != nullptr)
        {
            delete portOut;
            portOut = nullptr;
        }
    }

    void initBuffers()
    {
        if (portIn != nullptr)
            portIn->initBuffer();

        if (portOut != nullptr)
            portOut->initBuffer();
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginEventData)
};

// -----------------------------------------------------------------------

enum SpecialParameterType {
    PARAMETER_SPECIAL_NULL          = 0,
    PARAMETER_SPECIAL_LATENCY       = 1,
    PARAMETER_SPECIAL_SAMPLE_RATE   = 2,
    PARAMETER_SPECIAL_LV2_FREEWHEEL = 3,
    PARAMETER_SPECIAL_LV2_TIME      = 4
};

struct PluginParameterData {
    uint32_t count;
    ParameterData* data;
    ParameterRanges* ranges;
    SpecialParameterType* special;

    PluginParameterData() noexcept
        : count(0),
          data(nullptr),
          ranges(nullptr),
          special(nullptr) {}

    ~PluginParameterData()
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT(data == nullptr);
        CARLA_ASSERT(ranges == nullptr);
        CARLA_ASSERT(special == nullptr);
    }

    void createNew(const uint32_t newCount, const bool withSpecial)
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT(data == nullptr);
        CARLA_ASSERT(ranges == nullptr);
        CARLA_ASSERT(special == nullptr);
        CARLA_ASSERT_INT(newCount > 0, newCount);

        if (data != nullptr || ranges != nullptr || newCount == 0)
            return;

        data   = new ParameterData[newCount];
        ranges = new ParameterRanges[newCount];
        count  = newCount;

        if (withSpecial)
            special = new SpecialParameterType[newCount];
    }

    void clear()
    {
        if (data != nullptr)
        {
            delete[] data;
            data = nullptr;
        }

        if (ranges != nullptr)
        {
            delete[] ranges;
            ranges = nullptr;
        }

        if (special != nullptr)
        {
            delete[] special;
            special = nullptr;
        }

        count = 0;
    }

    float getFixedValue(const uint32_t parameterId, const float& value) const
    {
        CARLA_SAFE_ASSERT_RETURN(parameterId < count, 0.0f);
        return ranges[parameterId].getFixedValue(value);
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginParameterData)
};

// -----------------------------------------------------------------------

typedef const char* ProgramName;

struct PluginProgramData {
    uint32_t count;
    int32_t  current;
    ProgramName* names;

    PluginProgramData() noexcept
        : count(0),
          current(-1),
          names(nullptr) {}

    ~PluginProgramData()
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT_INT(current == -1, current);
        CARLA_ASSERT(names == nullptr);
    }

    void createNew(const uint32_t newCount)
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT_INT(current == -1, current);
        CARLA_ASSERT(names == nullptr);
        CARLA_ASSERT_INT(newCount > 0, newCount);

        if (names != nullptr || newCount == 0)
            return;

        names = new ProgramName[newCount];
        count = newCount;

        for (uint32_t i=0; i < newCount; ++i)
            names[i] = nullptr;
    }

    void clear()
    {
        if (names != nullptr)
        {
            for (uint32_t i=0; i < count; ++i)
            {
                if (names[i] != nullptr)
                {
                    delete[] names[i];
                    names[i] = nullptr;
                }
            }

            delete[] names;
            names = nullptr;
        }

        count = 0;
        current = -1;
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginProgramData)
};

// -----------------------------------------------------------------------

struct PluginMidiProgramData {
    uint32_t count;
    int32_t  current;
    MidiProgramData* data;

    PluginMidiProgramData() noexcept
        : count(0),
          current(-1),
          data(nullptr) {}

    ~PluginMidiProgramData()
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT_INT(current == -1, current);
        CARLA_ASSERT(data == nullptr);
    }

    void createNew(const uint32_t newCount)
    {
        CARLA_ASSERT_INT(count == 0, count);
        CARLA_ASSERT_INT(current == -1, current);
        CARLA_ASSERT(data == nullptr);
        CARLA_ASSERT_INT(newCount > 0, newCount);

        if (data != nullptr || newCount == 0)
            return;

        data  = new MidiProgramData[newCount];
        count = newCount;

        for (uint32_t i=0; i < count; ++i)
        {
            data[i].bank    = 0;
            data[i].program = 0;
            data[i].name    = nullptr;
        }
    }

    void clear()
    {
        if (data != nullptr)
        {
            for (uint32_t i=0; i < count; ++i)
            {
                if (data[i].name != nullptr)
                {
                    delete[] data[i].name;
                    data[i].name = nullptr;
                }
            }

            delete[] data;
            data = nullptr;
        }

        count = 0;
        current = -1;
    }

    const MidiProgramData& getCurrent() const
    {
        CARLA_ASSERT_INT2(current >= 0 && current < static_cast<int32_t>(count), current, count);
        return data[current];
    }

    CARLA_DECLARE_NON_COPY_STRUCT(PluginMidiProgramData)
};

// -----------------------------------------------------------------------

struct CarlaPluginProtectedData {
    CarlaEngine* const engine;
    CarlaEngineClient* client;

    unsigned int id;
    unsigned int hints;
    unsigned int options;

    bool active;
    bool enabled;
    bool needsReset;

    void* lib;
    void* uiLib;

    // misc
    int8_t ctrlChannel;
    uint   extraHints;
    int    patchbayClientId;

    // latency
    uint32_t latency;
    float**  latencyBuffers;

    // data 1
    const char* name;
    const char* filename;
    const char* iconName;
    const char* identifier; // used for save/restore settings per plugin

    // data 2
    PluginAudioData audioIn;
    PluginAudioData audioOut;
    PluginEventData event;
    PluginParameterData param;
    PluginProgramData prog;
    PluginMidiProgramData midiprog;
    LinkedList<CustomData> custom;

    SaveState saveState;

    CarlaMutex masterMutex; // global master lock
    CarlaMutex singleMutex; // small lock used only in processSingle()

    struct ExternalNotes {
        CarlaMutex mutex;
        RtLinkedList<ExternalMidiNote>::Pool dataPool;
        RtLinkedList<ExternalMidiNote> data;

        ExternalNotes()
            : dataPool(32, 152),
              data(dataPool) {}

        ~ExternalNotes()
        {
            mutex.lock();
            data.clear();
            mutex.unlock();
        }

        void append(const ExternalMidiNote& note)
        {
            mutex.lock();
            data.append_sleepy(note);
            mutex.unlock();
        }

        CARLA_DECLARE_NON_COPY_STRUCT(ExternalNotes)

    } extNotes;

    struct PostRtEvents {
        CarlaMutex mutex;
        RtLinkedList<PluginPostRtEvent>::Pool dataPool;
        RtLinkedList<PluginPostRtEvent> data;
        RtLinkedList<PluginPostRtEvent> dataPendingRT;

        PostRtEvents()
            : dataPool(128, 128),
              data(dataPool),
              dataPendingRT(dataPool) {}

        ~PostRtEvents()
        {
            clear();
        }

        void appendRT(const PluginPostRtEvent& event)
        {
            dataPendingRT.append(event);
        }

        void trySplice()
        {
            if (mutex.tryLock())
            {
                dataPendingRT.spliceAppend(data);
                mutex.unlock();
            }
        }

        void clear()
        {
            mutex.lock();
            data.clear();
            dataPendingRT.clear();
            mutex.unlock();
        }

        CARLA_DECLARE_NON_COPY_STRUCT(PostRtEvents)

    } postRtEvents;

#ifndef BUILD_BRIDGE
    struct PostProc {
        float dryWet;
        float volume;
        float balanceLeft;
        float balanceRight;
        float panning;

        PostProc() noexcept
            : dryWet(1.0f),
              volume(1.0f),
              balanceLeft(-1.0f),
              balanceRight(1.0f),
              panning(0.0f) {}

        CARLA_DECLARE_NON_COPY_STRUCT(PostProc)

    } postProc;
#endif

    struct OSC {
        CarlaOscData data;
        CarlaPluginThread thread;

        OSC(CarlaEngine* const engine, CarlaPlugin* const plugin)
            : thread(engine, plugin) {}

#ifdef CARLA_PROPER_CPP11_SUPPORT
        OSC() = delete;
        CARLA_DECLARE_NON_COPY_STRUCT(OSC)
#endif
    } osc;

    CarlaPluginProtectedData(CarlaEngine* const eng, const unsigned int idx, CarlaPlugin* const self)
        : engine(eng),
          client(nullptr),
          id(idx),
          hints(0x0),
          options(0x0),
          active(false),
          enabled(false),
          needsReset(false),
          lib(nullptr),
          uiLib(nullptr),
          ctrlChannel(0),
          extraHints(0x0),
          patchbayClientId(0),
          latency(0),
          latencyBuffers(nullptr),
          name(nullptr),
          filename(nullptr),
          iconName(nullptr),
          identifier(nullptr),
          osc(eng, self) {}

#ifdef CARLA_PROPER_CPP11_SUPPORT
    CarlaPluginProtectedData() = delete;
    CARLA_DECLARE_NON_COPY_STRUCT(CarlaPluginProtectedData)
#endif

    ~CarlaPluginProtectedData()
    {
        CARLA_SAFE_ASSERT(! needsReset);

        if (name != nullptr)
        {
            delete[] name;
            name = nullptr;
        }

        if (filename != nullptr)
        {
            delete[] filename;
            filename = nullptr;
        }

        if (iconName != nullptr)
        {
            delete[] iconName;
            iconName = nullptr;
        }

        if (identifier != nullptr)
        {
            delete[] identifier;
            identifier = nullptr;
        }

        {
            // mutex MUST have been locked before
            const bool lockMaster(masterMutex.tryLock());
            const bool lockSingle(singleMutex.tryLock());
            CARLA_SAFE_ASSERT(! lockMaster);
            CARLA_SAFE_ASSERT(! lockSingle);
        }

        if (client != nullptr)
        {
            if (client->isActive())
            {
                // must not happen
                carla_safe_assert("client->isActive()", __FILE__, __LINE__);
                client->deactivate();
            }

            clearBuffers();

            delete client;
            client = nullptr;
        }

        for (LinkedList<CustomData>::Itenerator it = custom.begin(); it.valid(); it.next())
        {
            CustomData& cData(it.getValue());

            if (cData.type != nullptr)
            {
                delete[] cData.type;
                cData.type = nullptr;
            }
            else
                carla_safe_assert("cData.type != nullptr", __FILE__, __LINE__);

            if (cData.key != nullptr)
            {
                delete[] cData.key;
                cData.key = nullptr;
            }
            else
                carla_safe_assert("cData.key != nullptr", __FILE__, __LINE__);

            if (cData.value != nullptr)
            {
                delete[] cData.value;
                cData.value = nullptr;
            }
            else
                carla_safe_assert("cData.value != nullptr", __FILE__, __LINE__);
        }

        prog.clear();
        midiprog.clear();
        custom.clear();

        // MUST have been locked before
        masterMutex.unlock();
        singleMutex.unlock();

        if (lib != nullptr)
            libClose();

        CARLA_ASSERT(uiLib == nullptr);
    }

    // -------------------------------------------------------------------
    // Buffer functions

    void clearBuffers();

    void recreateLatencyBuffers();

    // -------------------------------------------------------------------
    // Post-poned events

    void postponeRtEvent(const PluginPostRtEventType type, const int32_t value1, const int32_t value2, const float value3);

    // -------------------------------------------------------------------
    // Library functions, see CarlaPlugin.cpp

    const char* libError(const char* const filename);

    bool  libOpen(const char* const filename);
    bool  libClose();
    void* libSymbol(const char* const symbol);

    bool  uiLibOpen(const char* const filename);
    bool  uiLibClose();
    void* uiLibSymbol(const char* const symbol);

    // -------------------------------------------------------------------
    // Settings functions, see CarlaPlugin.cpp

    void         saveSetting(const unsigned int option, const bool yesNo);
    unsigned int loadSettings(const unsigned int options, const unsigned int availOptions);
};

CARLA_BACKEND_END_NAMESPACE

#endif // CARLA_PLUGIN_INTERNAL_HPP_INCLUDED

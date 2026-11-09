#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <vector>
#include "Categories.h"

// Every embedded sound, decoded once.
struct SampleBank
{
    struct Sound { juce::AudioBuffer<float> audio; double rate = 44100; };
    std::array<std::vector<Sound>, categoryCount> sounds;

    // Decodes the FLAC files embedded by Tools/pack_samples.py. 'getResource' is BinaryData::getNamedResource.
    static std::unique_ptr<SampleBank> load (const char* (*getResource) (const char*, int&))
    {
        auto bank = std::make_unique<SampleBank>();
        juce::FlacAudioFormat flac;
        for (int c = 0; c < categoryCount; ++c)
        {
            const auto& list = sampleLists[c];
            bank->sounds[(size_t) c].resize ((size_t) list.count);
            for (int i = 0; i < list.count; ++i)
            {
                int size = 0;
                const char* data = getResource (list.entries[i].resource, size);
                if (data == nullptr) continue;
                std::unique_ptr<juce::AudioFormatReader> reader (
                    flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
                if (reader == nullptr) continue;
                auto& s = bank->sounds[(size_t) c][(size_t) i];
                s.rate = reader->sampleRate;
                s.audio.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (&s.audio, 0, (int) reader->lengthInSamples, 0, true, true);
            }
        }
        return bank;
    }
};

struct PlaySettings { float tune = 0, decay = 100; };

// Plays sounds from the bank. Runs on the audio thread and never allocates.
class SampleEngine
{
public:
    void prepare (double rate) { sampleRate = rate; stopAll(); }
    void stopAll() { for (auto& v : voices) v.sound = nullptr; }

    void trigger (const SampleBank::Sound& sound, const PlaySettings& s, float velocity, int offsetSamples)
    {
        if (sound.audio.getNumSamples() == 0) return;
        Voice* slot = &voices[0];
        for (auto& v : voices)
        {
            if (v.sound == nullptr) { slot = &v; break; }
            if (v.position > slot->position) slot = &v; // all busy: take the one furthest along
        }
        Voice& v = *slot;
        v.sound = &sound;
        v.position = 0;
        v.wait = offsetSamples;
        v.step = sound.rate / sampleRate * std::pow (2.0, s.tune / 12.0);
        v.gain = std::pow (velocity, 1.4f);
        // Decay below 100 % fades the sound out before its natural end.
        const double length = sound.audio.getNumSamples();
        v.end = length * juce::jlimit (0.1, 1.0, s.decay / 100.0);
        v.fadeFrom = s.decay >= 99.5f ? length : v.end * .35;
    }

    void render (juce::AudioBuffer<float>& out)
    {
        auto* left = out.getWritePointer (0);
        auto* right = out.getWritePointer (1);
        const int count = out.getNumSamples();
        for (auto& v : voices)
        {
            if (v.sound == nullptr) continue;
            const auto& audio = v.sound->audio;
            const float* l = audio.getReadPointer (0);
            const float* r = audio.getReadPointer (audio.getNumChannels() > 1 ? 1 : 0);
            const int last = audio.getNumSamples() - 1;
            for (int n = 0; n < count; ++n)
            {
                if (v.wait > 0) { --v.wait; continue; }
                if (v.position >= v.end || v.position >= last) { v.sound = nullptr; break; }
                const int i = (int) v.position;
                const float f = (float) (v.position - i);
                float g = v.gain;
                if (v.position > v.fadeFrom)
                {
                    const float t = (float) ((v.position - v.fadeFrom) / (v.end - v.fadeFrom));
                    g *= (1 - t) * (1 - t);
                }
                left[n] += g * (l[i] + f * (l[i + 1] - l[i]));
                right[n] += g * (r[i] + f * (r[i + 1] - r[i]));
                v.position += v.step;
            }
        }
    }

private:
    struct Voice
    {
        const SampleBank::Sound* sound = nullptr;
        double position = 0, step = 1, end = 0, fadeFrom = 0;
        float gain = 1;
        int wait = 0;
    };

    double sampleRate = 48000;
    std::array<Voice, 64> voices;
};


#include "recorder.hpp"
#include <portaudio.h>
#include <iostream>

using namespace score;

struct Recorder::Pimpl {

    Pimpl(std::int32_t sample_rate, std::int8_t channels, int device_index, std::size_t frames_per_buffer) :
        device_index_(device_index),
        channels_(channels),
        frames_per_buffer_(static_cast<size_t>(frames_per_buffer ? frames_per_buffer : sample_rate / 100)),
        sample_rate_(sample_rate),
        record_buffer_(sample_rate, channels, frames_per_buffer)
        {
        restart();
    }

    static int PortAudioCallback(const void *inputBuffer,
                                 void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo *timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData) {
        return ((Pimpl*)userData)->bufferReady(inputBuffer, outputBuffer, framesPerBuffer, timeInfo, statusFlags);
    }

    void setOnRecordingStarted(const std::function<void()>& callback) {
        on_recording_started_ = callback;
    }

    void setOnRecordingStopped(const std::function<void()>& callback) {
        on_recording_stopped_ = callback;
    }

    void setOnProcessingBufferReady(const std::function<void(AudioBuffer& buffer)>& callback) {
        on_buffer_ready_ = callback;
    }

    void restart() {
        input_params_.channelCount = static_cast<int>(channels_);
        input_params_.device = device_index_;
        input_params_.sampleFormat = paFloat32 ;
        input_params_.suggestedLatency = Pa_GetDeviceInfo(device_index_)->defaultLowInputLatency;
        input_params_.hostApiSpecificStreamInfo = nullptr;

        const PaError err = Pa_OpenStream(&stream_, &input_params_, nullptr, sample_rate_,
                frames_per_buffer_, paClipOff, &Pimpl::PortAudioCallback, this);

        if (err != paNoError) {
            throw std::runtime_error(Pa_GetErrorText(err));
        }
    }

    bool record() {
        if (isRunning()) {
            return true;
        }

        if (auto err = Pa_StartStream(stream_) != paNoError) {
            throw std::runtime_error(Pa_GetErrorText(err));
        }

        const bool recording = isRunning();
        if (recording) {
            on_recording_started_();
        }
        return recording;
    }

    bool stop() {
        if (!isRunning()) {
            return true;
        }

        if (auto err =  Pa_StopStream(stream_) != paNoError ) {
            throw std::runtime_error(Pa_GetErrorText(err));
        }

        const bool stoped = !isRunning();
        if (stoped) {
            on_recording_stopped_();
        }
        return stoped;
    }


    int bufferReady(const void *inputBuffer,
                     void *outputBuffer,
                     unsigned long framesPerBuffer,
                     const PaStreamCallbackTimeInfo *timeInfo,
                     PaStreamCallbackFlags statusFlags) {

        auto *ptr = (const float *) inputBuffer;
        record_buffer_.fromInterleave(channels_, frames_per_buffer_, ptr);
        record_buffer_.setTimestamp(timeInfo->currentTime + timeInfo->inputBufferAdcTime);
        on_buffer_ready_(record_buffer_);
        return paContinue;
    }

    bool isSampleRateSupported(std::int32_t sample_rate) {
        PaStreamParameters output_params;
        output_params.channelCount = 2;
        output_params.device = Pa_GetDefaultOutputDevice();
        output_params.sampleFormat = paFloat32 ;
        output_params.suggestedLatency = Pa_GetDeviceInfo(output_params.device)->defaultLowInputLatency;
        output_params.hostApiSpecificStreamInfo = nullptr;
        return static_cast<bool>(Pa_IsFormatSupported(&input_params_, &output_params, sample_rate));
    }

    void setSampleRate(std::int32_t sample_rate) {
        if (!isSampleRateSupported(sample_rate)) {
            throw std::invalid_argument("Sample rate not supported.");
        }
        sample_rate_ = sample_rate;
    }


    void setDeviceIndex(PaDeviceIndex index) {
        if (index > Pa_GetDeviceCount()) {
            throw std::invalid_argument("Index out of bounds. Available devices: "
            + std::to_string(Pa_GetDeviceCount()));
        }

        if (Pa_GetDeviceInfo(index)->maxInputChannels <= 0) {
            throw std::runtime_error("Expected an input device. Device index: " + std::to_string(index));
        }

        device_index_ = index;
    }

    void setFramesPerBuffer(std::size_t frames_per_buffer) {
        frames_per_buffer_ = frames_per_buffer;
        record_buffer_.resize(channels_, frames_per_buffer);
    }

    bool isRunning() const {
        return static_cast<bool>(Pa_IsStreamActive(stream_));
    }

    double timestamp() const {
        return Pa_GetStreamTime(stream_);
    }

    PaStream*               stream_{nullptr};
    PaStreamParameters      input_params_{};
    PaDeviceIndex device_index_;

    std::int8_t channels_;
    std::size_t frames_per_buffer_;
    std::int32_t sample_rate_;


    AudioBuffer record_buffer_;
    std::function<void()> on_recording_started_{nullptr};
    std::function<void()> on_recording_stopped_{nullptr};
    std::function<void(AudioBuffer& buffer)> on_buffer_ready_{nullptr};

};

Recorder::Recorder(std::int32_t sample_rate, std::int8_t channels,  int device_index, std::size_t frames_per_buffer) :
    pimpl_(std::make_unique<Pimpl>(sample_rate, channels, device_index, frames_per_buffer)) {

}

Recorder::Recorder(std::int32_t sample_rate, std::int8_t channels) :
        Recorder(sample_rate, channels, DefaultInputDevice()){

}

Recorder::Recorder(std::int8_t channels) :
        Recorder(static_cast<float>(Pa_GetDeviceInfo(Pa_GetDefaultInputDevice())->defaultSampleRate), channels, DefaultInputDevice()){

}

Recorder::~Recorder() = default;

void Recorder::reset() {
    pimpl_->restart();
}

void Recorder::setOnRecordingStarted(const std::function<void()> &callback) {
    pimpl_->setOnRecordingStarted(callback);
}

void Recorder::setOnRecordingStopped(const std::function<void()> &callback) {
    pimpl_->setOnRecordingStopped(callback);
}

void Recorder::setOnProcessingBufferReady(const std::function<void(AudioBuffer& buffer)> &callback) {
    pimpl_->setOnProcessingBufferReady(callback);
}

int Recorder::deviceIndex() const {
    return pimpl_->device_index_;
}

void Recorder::setDeviceIndex(int index) {
    pimpl_->setDeviceIndex(index);
}

void Recorder::setFramesPerBuffer(std::size_t number) {
    pimpl_->setFramesPerBuffer(number);
}

std::size_t Recorder::framesPerBuffer() const {
    return pimpl_->frames_per_buffer_;
}

float Recorder::sampleRate() const {
    return pimpl_->sample_rate_;
}

void Recorder::setSampleRate(std::int32_t sample_rate) {
    pimpl_->setSampleRate(sample_rate);
}

bool Recorder::isRecording() {
    return pimpl_->isRunning();
}

bool Recorder::record() {
    return pimpl_->record();
}

bool Recorder::stop() {
    return pimpl_->stop();
}

float Recorder::timestamp() const {
    return static_cast<float>(pimpl_->timestamp());
}

std::string Recorder::deviceName() const {
    return  Pa_GetDeviceInfo(pimpl_->device_index_)->name;;
}

int score::Recorder::DefaultInputDevice() {
    const auto index = Pa_GetDefaultInputDevice();
    if (index == paNoDevice) {
        throw std::runtime_error("Non input device found");
    }
    return index;
}

void score::Recorder::Initialize() {
    const auto err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error(Pa_GetErrorText(err));
    }

}


Recorder::DeviceInfo score::Recorder::DeviceInformation(std::int32_t index) {
    const auto* device = Pa_GetDeviceInfo(index);
    return DeviceInfo{name : device->name,
            maxInputChannels : device->maxInputChannels,
            maxOutputChannels : device->maxOutputChannels,
            defaultSampleRate : device->defaultSampleRate};
}

void score::Recorder::AvailableDevices() {
    for (auto i = 0ul; i < Pa_GetDeviceCount(); ++i) {
        const auto* device = Pa_GetDeviceInfo(i);
        const auto info = DeviceInfo{name : device->name,
                maxInputChannels : device->maxInputChannels,
                maxOutputChannels : device->maxOutputChannels,
                defaultSampleRate : device->defaultSampleRate};

        std::cout << "Device Information" << std::endl;
        std::cout << "Index: " << i << std::endl;
        std::cout << "Name: " << info.name << std::endl;
        std::cout << "Available Input Channels: " << info.maxInputChannels << std::endl;
        std::cout << "Available Output Channels: " << info.maxOutputChannels << std::endl;
        std::cout << "Default Sample rate: " << info.defaultSampleRate << std::endl;
        std::cout << std::endl << std::endl;
    }
}


#include <recorder.hpp>
#include <doa.hpp>
#include <gain.hpp>
#include <encoder.hpp>
#include <pixel_ring.hpp>

#include <iostream>
#include <chrono>
#include <boost/program_options.hpp>

using namespace score;
namespace po = boost::program_options;

int main(int ac, char* av[]) {
    Recorder::Initialize();
    Recorder::AvailableDevices();

    std::string filename;
    std::int32_t sample_rate;
    std::int32_t device_index;
    std::int32_t channels;
    std::uint16_t vid, pid;
    std::size_t duration;

    po::options_description desc("Recording options");
    desc.add_options()
            ("help, h", "Help message")
            ("duration, d", po::value<std::size_t >(&duration)->default_value(10000),
                    "Duration of the recording in milliseconds.")
            ("sample-rate, sr", po::value<std::int32_t>(&sample_rate)->default_value(16000),
                    "Sampling frequency in Hz. Default: 16000Hz")
            ("channels, ch", po::value<std::int32_t >(&channels)->default_value(5),
                    "Number of channels. Default: Mono (1)")
            ("index, i", po::value<std::int32_t >(&device_index)->default_value(Recorder::DefaultInputDevice()),
                    "Device index. Default: default system input device.")
            ("vendor, v", po::value<std::uint16_t>(&vid)->default_value(0x2886), "Vendor ID")
            ("product, p", po::value<std::uint16_t >(&pid)->default_value(0x0018), "Product ID");

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    po::notify(vm);

    const auto info = Recorder::DeviceInformation(device_index);
    std::cout << "Device Information:" << std::endl;
    std::cout << "Name: " << info.name << std::endl;
    std::cout << "Available Input Channels: " << info.maxInputChannels << std::endl;
    std::cout << "Available Output Channels: " << info.maxOutputChannels << std::endl;
    std::cout << "Default Sample rate: " << info.defaultSampleRate << std::endl;


    AudioBuffer data(sample_rate), downmixed;
    const auto valid_channels = channels - 1;
    const auto frames_per_buffer = 0.01 * sample_rate;

    const std::vector<std::pair<std::size_t, std::size_t>> groups = {{0, 2}, {1, 3}};
    auto doa = std::make_unique<DOA>(sample_rate, valid_channels);
    doa->setGroupMicrophones(groups);

    auto gain = std::make_unique<Gain>(3);
    auto pixel = std::make_unique<PixelRing>(vid, pid);
    auto encoder = std::make_unique<Encoder>("recorded.wav", sample_rate, channels);
    volatile auto iterations = static_cast<std::uint64_t >(duration) / 10;
    const auto processing = [&](AudioBuffer& recorded) {
        data.fromInterleave(valid_channels, recorded.framesPerChannel(), recorded.data() + recorded.framesPerChannel());
        gain->process(data, data);
        pixel->setAngle(doa->process(data), 255, 255, 255);
        encoder->process(recorded);
        --iterations;
    };

    auto recorder = std::make_unique<Recorder>(sample_rate, channels, device_index, frames_per_buffer);
    recorder->setOnRecordingStarted([](){ std::cout << "Recording started" << std::endl;  });
    recorder->setOnRecordingStopped([]() {  std::cout << "Recording stopped" << std::endl;  });
    recorder->setOnProcessingBufferReady(processing);
    recorder->record();

    while (iterations) {
    }
    return 0;
}
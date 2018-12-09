#include <memory>

#include "noise_suppression.hpp"
#include "band_extractor.hpp"
#include <noise_suppression.h>



using namespace score;

struct Handler {
    explicit Handler(float sample_rate) : handle_(WebRtcNs_Create()) {
        if (handle_ == nullptr) {
            throw std::bad_alloc();
        } else {
            const auto error = WebRtcNs_Init(handle_, static_cast<uint32_t>(sample_rate));
            if (error != 0) {
                throw std::runtime_error("Error while initializing the noise suppression block.");
            }
        }
    }

    ~Handler() {
        WebRtcNs_Free(handle_);
    }

    NsHandle *core() { return handle_; }

private:
    NsHandle *handle_{nullptr};
};



struct NoiseSuppression::Pimpl {


    Pimpl(std::size_t channels, float sample_rate, Policy policy) :
        channels_(channels),
        sample_rate_(sample_rate),
        input_bands_(10, std::vector<float>(160,0)),
        output_bands_(10, std::vector<float>(160,0)),
        estimated_noise_(WebRtcNs_num_freq(), 0),
        handlers_(channels)
    {
        for (auto& smart_pointer : handlers_) {
            smart_pointer = std::make_unique<Handler>(sample_rate);
        }
        setPolicy(policy_);
    }

    ~Pimpl() = default;

    void setPolicy(NoiseSuppression::Policy policy) {
        const auto p = static_cast<std::underlying_type<NoiseSuppression::Policy>::type>(policy);
        for (const auto& handler : handlers_) {
            const auto error = WebRtcNs_set_policy(handler->core(), p);
            if (error != 0) {
                throw std::runtime_error("Error while updating noise suppression policy.");
            }
        }
        policy_ = policy;
    }

    std::size_t numberFrequencyBins() const {
        return WebRtcNs_num_freq();
    }

    // Compute the different bands
    void updateBands(const std::vector<float>& frame) {

    }

    const std::vector<float>& estimatedNoise() {
        float fraction = 1.0f / channels_;
        for (auto i = 0; i < channels_; ++i) {
            const auto* noise_bands  = WebRtcNs_noise_estimate(handlers_[i]->core());
            for (auto j = 0ul, size = estimated_noise_.size(); j < size; ++j) {
                estimated_noise_[j] += fraction * noise_bands[j];
            }
        }
        return estimated_noise_;
    }

    void reset() {

    }

    float speechProbability() {
        auto probability = 0.0f;
        for (auto i = 0; i < channels_; ++i) {
            probability  += WebRtcNs_prior_speech_probability(handlers_[i]->core());
        }

        if (channels_) {
            probability /= static_cast<float>(channels_);
        }

        return probability;
    }


    void process(const std::vector<std::vector<float>>& inputs, std::vector<std::vector<float>>& outputs) {
        if (inputs.size() != channels_) {
            throw std::invalid_argument("Expected an input frame with " + std::to_string(channels_) + " channels.");
        }
        for (auto i = 0; i < channels_; ++i) {
            updateBands(inputs[i]);
            WebRtcNs_Analyze(handlers_[i]->core(), input_bands_.front().data());
            //WebRtcNs_Process(handlers_[i]->core(), input_bands_.data(), input_bands_.size(), output_bands_.data());
        }
    }



private:
    Policy policy_;
    std::size_t channels_{};
    float sample_rate_{};
    std::vector<float> estimated_noise_;
    std::vector<std::vector<float>> input_bands_;
    std::vector<std::vector<float>> output_bands_;
    std::vector<std::unique_ptr<Handler>> handlers_{};
};

NoiseSuppression::NoiseSuppression(std::size_t channels, float sample_rate, Policy policy)
: pimpl_(std::make_unique<Pimpl>(channels, sample_rate, policy)) {}

NoiseSuppression::~NoiseSuppression() = default;


void NoiseSuppression::reset() {
    pimpl_->reset();
}

void NoiseSuppression::process(const std::vector<std::vector<float>> &input,
                                      std::vector<std::vector<float>> &output) {
    pimpl_->process(input, output);

}

float NoiseSuppression::speechProbability() const {
    return pimpl_->speechProbability();
}

std::size_t score::NoiseSuppression::numberFrequencyBins() const {
    return pimpl_->numberFrequencyBins();
}

const std::vector<float> &score::NoiseSuppression::estimatedNoise() const {
    return pimpl_->estimatedNoise();
}

void NoiseSuppression::setPolicy(NoiseSuppression::Policy policy) {
    pimpl_->setPolicy(policy);
}
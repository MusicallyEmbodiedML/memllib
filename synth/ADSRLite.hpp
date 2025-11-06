#ifndef MEMLLIB_SYNTH_ADSRLITE_HPP
#define MEMLLIB_SYNTH_ADSRLITE_HPP

class ADSRLite {
    public:
    
    enum envStage{WAITTOTRIG, ATTACK, DECAY, SUSTAIN, RELEASE};

    void setup(float attackTimeMs, float decayTimeMs, float newSustainLevel, float releaseTimeMs, const float kSampleRate) {
        attackIncFull = 1.f/((attackTimeMs * 0.001f) * kSampleRate);
        attackInc = attackIncFull;
        decayInc = 1.f/((decayTimeMs * 0.001f) * kSampleRate);
        sustainLevel = newSustainLevel;
        decayInc *= (1.f - sustainLevel);
        releaseMs = releaseTimeMs;
    }

    float play() {

        switch(stage) {
            case envStage::WAITTOTRIG:
            {
                envelopeValue = 0.f;
                break;
            }
            case envStage::ATTACK:
            {
                envelopeValue += attackInc;
                if (envelopeValue >= 1.f) {
                    stage = envStage::DECAY;
                }
                break;
            }
            case envStage::DECAY:
            {
                envelopeValue -= decayInc;
                if (envelopeValue<=sustainLevel) {
                    stage = envStage::SUSTAIN;
                }
                break;
            }
            case envStage::SUSTAIN:
            {
                // envelopeValue = sustainLevel;
                break;
            }
            case envStage::RELEASE:
            {
                envelopeValue -= relInc;
                if (envelopeValue <= 0.f) {
                    stage = envStage::WAITTOTRIG;
                    envelopeValue=0.f;
                }
                break;
            }
        }
        return envelopeValue * velocity;
    }

    void reset() {
        stage = envStage::WAITTOTRIG;
        envelopeValue=0;

    }

    void trigger(float vel) {
        stage = envStage::ATTACK;
        attackInc = attackIncFull * (1.f - envelopeValue);
        velocity = vel;
    }

    void release() {
        if (releaseMs > 0) {
            relInc = 1.f/((releaseMs / 1000.f) * kSampleRate);
            relInc *= envelopeValue;
            stage = envStage::RELEASE;
        }
        else {
            relInc = 0.f;
            stage = envStage::WAITTOTRIG;            
        }
    }

private:
    envStage stage = envStage::WAITTOTRIG;
    float attackInc=0, attackIncFull=0, decayInc=0, sustainLevel=0,relInc=0, releaseMs;
    float envelopeValue=0;
    float velocity = 1.f;
};


#endif // MEMLLIB_SYNTH_ADSRLITE_HPP
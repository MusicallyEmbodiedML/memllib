#ifndef MEMLLIB_SYNTH_ADSRLITE_HPP
#define MEMLLIB_SYNTH_ADSRLITE_HPP

class ADSRLite {
    public:
    
    enum envStage{WAITTOTRIG, ATTACK, DECAY, SUSTAIN, RELEASE};

    __attribute__((always_inline)) void setAttackTime(float attackTimeMs, const float kSampleRate) {
        if (attackTimeMs > 0.f) {
            attackIncFull = 1.f/((attackTimeMs * 0.001f) * kSampleRate);
        }else{
            attackIncFull = 0.f;
        }
        attackInc = attackIncFull;
    }

    inline void setReleaseTime(float releaseTimeMs, const float kSampleRate) {
        releaseMs = releaseTimeMs;
    }

    void setup(float attackTimeMs, float decayTimeMs, float newSustainLevel, float releaseTimeMs, const float kSampleRate) {
        setAttackTime(attackTimeMs, kSampleRate);
        if (decayTimeMs > 0.f) {
            decayInc = 1.f/((decayTimeMs * 0.001f) * kSampleRate);
        }else{
            decayInc = 0.f;
        }
        sustainLevel = newSustainLevel;
        decayInc *= (1.f - sustainLevel);
        // releaseMs = releaseTimeMs;
        setReleaseTime(releaseTimeMs, kSampleRate);
    }

    __attribute__((hot)) __attribute__((always_inline)) float play() {

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

    __attribute__((always_inline)) void reset() {
        stage = envStage::WAITTOTRIG;
        envelopeValue=0;

    }

    __attribute__((always_inline)) void trigger(float vel) {
        stage = envStage::ATTACK;
        attackInc = attackIncFull * (1.f - envelopeValue);
        velocity = vel;
    }

    __attribute__((always_inline)) void triggerIfReady(float vel) {
        if (stage == envStage::WAITTOTRIG) {
            trigger(vel);
        }
    }

    __attribute__((always_inline)) void release() {
        if (releaseMs > 0) {
            relInc = 1.f/((releaseMs / 1000.f) * maxiSettings::sampleRate);
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
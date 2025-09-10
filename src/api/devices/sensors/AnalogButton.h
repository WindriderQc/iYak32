#pragma once

#include "ISensor.h"

namespace Sensor {

    class AnalogButton : public Sensor::ISensor
    {
    public:
        AnalogButton(int threshold = 2000) : threshold_(threshold), lastValue_(0) {}

        ~AnalogButton() {}

        String loopImpl() override
        {
            String msg = "";
            int value = analogRead(pin_id());

            // Trigger only when the value crosses the threshold
            if (value > threshold_ && lastValue_ <= threshold_) {
                msg = message(value);
            }
            lastValue_ = value;
            return msg;
        }

        void setThreshold(int threshold) {
            threshold_ = threshold;
        }

        int getThreshold() const {
            return threshold_;
        }

        int getValue() const {
            return lastValue_;
        }

    private:
        int threshold_;
        int lastValue_;
    };
}

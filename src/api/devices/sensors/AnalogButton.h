#pragma once

#include "ISensor.h"

namespace Sensor {

    class AnalogButton : public Sensor::ISensor
    {
    public:
        enum EdgeMode { ANALOG_RISING, ANALOG_FALLING, ANALOG_CHANGE };

        AnalogButton(int threshold = 2000, EdgeMode mode = ANALOG_RISING) : threshold_(threshold), mode_(mode), lastValue_(0) {}

        ~AnalogButton() {}

        String loopImpl() override
        {
            String msg = "";
            int value = analogRead(pin_id());
            bool rising_edge = value > threshold_ && lastValue_ <= threshold_;
            bool falling_edge = value < threshold_ && lastValue_ >= threshold_;

            bool triggered = false;
            String edge_type = "";

            switch (mode_) {
                case ANALOG_RISING:
                    if (rising_edge) {
                        triggered = true;
                    }
                    break;
                case ANALOG_FALLING:
                    if (falling_edge) {
                        triggered = true;
                    }
                    break;
                case ANALOG_CHANGE:
                    if (rising_edge) {
                        triggered = true;
                        edge_type = "rising";
                    } else if (falling_edge) {
                        triggered = true;
                        edge_type = "falling";
                    }
                    break;
            }

            if (triggered) {
                if (mode_ == ANALOG_CHANGE) {
                    // Manually construct the JSON to avoid nesting and add edge_type
                    msg = "{\"device\":\"" + device_ + "\", \"io\":\"" + String(pin_) + "\", \"name\":\"" + name_ + "\", \"value\": \"" + String(value) + "\", \"edge\": \"" + edge_type + "\"}";
                } else {
                    msg = message(value);
                }
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

        void setMode(EdgeMode mode) {
            mode_ = mode;
        }

        EdgeMode getMode() const {
            return mode_;
        }

    private:
        int threshold_;
        EdgeMode mode_;
        int lastValue_;
    };
}

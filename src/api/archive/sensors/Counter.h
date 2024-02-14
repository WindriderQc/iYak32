#pragma once

#include "ISensor.h"

namespace Sensor {

    class Counter : public Sensor::ISensor
    {
    public:
        Counter()
        : isr_(0)
        , counter_(0)
        {}

        ~Counter()
        {}

        String loopImpl() override
        {
            String msg = "";

            if(isr_ > 0)
            {
                counter_ += isr_;
                isr_= 0;
                msg = message(counter_);  
            } 
            
            return msg;
        }        
      
        inline void setInterrupted() override
        {
            ++isr_;

        }

        void increment()
        {
            ++counter_;
        }

        void decrement()
        {
            --counter_;
        }

        long getCount() const
        {
            return counter_;
        }

        void reset()
        {
            isr_ = 0;
            counter_ = 0;
        }

    private:
        volatile int isr_;
        long counter_;
    };
}
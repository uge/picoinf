#include "KTime.h"

#include "Timeline.h"


void KTime::SetScalingFactor(double scalingFactor)
{
    Timeline::Global().Event("KTIME_SET_SCALING_FACTOR");
    
    scalingFactor_ = scalingFactor;

    for (auto &cb : cbList_)
    {
        cb();
    }
}

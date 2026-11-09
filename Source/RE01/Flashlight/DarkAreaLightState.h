#pragma once

// Kept independent of UObject/physics so the timing contract can be tested exactly.
// Now is game time: pausing the game also pauses the off grace period.
struct FDarkAreaLightState
{
    bool bLightOn = false;
    bool bWaitingToTurnOff = false;
    double OutsideSince = 0.0;

    bool Update(bool bInsideAnyBox, bool bAllowed, double Now, double OffDelay)
    {
        if (!bAllowed)
        {
            bLightOn = false;
            bWaitingToTurnOff = false;
        }
        else if (bInsideAnyBox)
        {
            bLightOn = true;
            bWaitingToTurnOff = false;
        }
        else if (bLightOn)
        {
            if (!bWaitingToTurnOff)
            {
                bWaitingToTurnOff = true;
                OutsideSince = Now;
            }
            if (Now - OutsideSince >= OffDelay)
            {
                bLightOn = false;
                bWaitingToTurnOff = false;
            }
        }
        return bLightOn;
    }
};

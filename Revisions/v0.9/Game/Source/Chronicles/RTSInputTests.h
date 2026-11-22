#pragma once

class ARTSController;

// Opt-in end-to-end mouse regression. Call from PlayerTick; inactive without
// -RTSInputTest. Results go to Saved/Tests/input-tests.json, then the app exits.
void RunRTSInputProbe(ARTSController* Controller);

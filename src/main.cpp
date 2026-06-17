// retip - ReXGlue Recompiled Project

#include "generated/default/retip_init.h"

#include "retip_app.h"

REX_DEFINE_APP(retip, RetipApp::Create)


bool SkipIntroVideos_hook()
{
    return false;
}
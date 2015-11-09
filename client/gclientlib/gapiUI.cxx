#include "config.h"
#include "gapiUI.h"
#include "gclient.h"

static void* sUI=0;

const string gbboxEnvFilename() {
  string fname="";
  if(getenv("GBBOX_ENVFILE") == NULL) {
    char gbboxenv[1024];
    sprintf(gbboxenv,"/tmp/gbbox_%d",getppid());
    fname = gbboxenv;
  }  else {
    fname=getenv("GBBOX_ENVFILE");
  }
  return fname;
}


void GapiUI::SetGapiUI(GapiUI* ui) {

  if (ui) {
    sUI=(void*)ui;
  }
}

GapiUI *GapiUI::GetGapiUI() {
  return (GapiUI*)sUI;
}

GapiUI *GapiUI::MakeGapiUI(bool storeToken) {
  if (sUI) {
    return (GapiUI*)sUI;
  } else {
    GapiUI* newui = new gclient(storeToken);
    return newui;
  }
}

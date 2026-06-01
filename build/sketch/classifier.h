#line 1 "C:\\Users\\raven\\OneDrive\\Documents\\Firmwares\\NetProfiler\\classifier.h"
/*
  Motor de clasificacion - scoring ponderado
*/
#ifndef CLASSIFIER_H
#define CLASSIFIER_H

#include "conn_profile.h"

void classifier_run(ConnProfile* p);
const char* verdict_to_str(Verdict v);

#endif

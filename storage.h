/*
  Storage - SD logging JSON/CSV
*/
#ifndef STORAGE_H
#define STORAGE_H

#include "conn_profile.h"

bool storage_init();
bool storage_log_profile(const ConnProfile* p);
bool storage_load_config();

#endif

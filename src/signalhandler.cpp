/**
 * @file signalhandler.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

///

#include "signalhandler.h"

SignalHandler<SIGHUP> _sighup_handler;
SignalHandler<SIGUSR1> _sigusr1_handler;
SignalHandler<SIGUSR2> _sigusr2_handler;


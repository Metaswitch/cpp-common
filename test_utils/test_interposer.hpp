/**
 * @file test_interposer.hpp Unit test interposer header - hooks various calls that are useful for UT.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#pragma once

#include <string>

void cwtest_add_host_mapping(std::string host, std::string target);
void cwtest_clear_host_mapping();
void cwtest_advance_time_ms(long delta_ms);
void cwtest_reset_time();
void cwtest_completely_control_time(bool start_of_epoch = false);

// Control file manipulation.
void cwtest_control_fopen(FILE* fd);
void cwtest_release_fopen();

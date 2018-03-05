/**
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef MOCK_SNMP_CONTINUOUS_ACCUMULATOR_TABLE_H__
#define MOCK_SNMP_CONTINUOUS_ACCUMULATOR_TABLE_H__

class MockContinuousAccumulatorTable : public SNMP::ContinuousAccumulatorTable
{
public:
  MOCK_METHOD1(accumulate, void(uint32_t));
};

#endif

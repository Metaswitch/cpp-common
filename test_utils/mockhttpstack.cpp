#include "mockhttpstack.hpp"

MockHttpStack::MockHttpStack():
  HttpStack(1, nullptr, nullptr, nullptr, nullptr)
{}

MockHttpStack::~MockHttpStack() {}


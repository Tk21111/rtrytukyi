#pragma once

#include <string>
#include <windows.h>

std::string HResultToString(HRESULT hr);
std::string PropVariantTypeToString(VARTYPE vt);
bool LogIfFailed(HRESULT hr, const std::string& context);
void ThrowIfFailed(HRESULT hr, const std::string& context);

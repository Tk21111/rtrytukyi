#include "hresult_utils.h"
#include "logger.h"

#include <comdef.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0) {
        std::string fallback;
        fallback.reserve(value.size());
        for (wchar_t c : value) {
            fallback.push_back(c <= 0x7f ? static_cast<char>(c) : '?');
        }
        return fallback;
    }

    std::string result(size, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr
    );

    return result;
}
}

std::string HResultToString(HRESULT hr) {
    _com_error error(hr);

    std::ostringstream ss;
    ss << "HRESULT 0x" << std::uppercase << std::hex << std::setw(8)
       << std::setfill('0') << static_cast<unsigned long>(hr);

    const TCHAR* message = error.ErrorMessage();
    if (message) {
#ifdef UNICODE
        std::wstring wideMessage(message);
        ss << " (" << WideToUtf8(wideMessage) << ")";
#else
        ss << " (" << message << ")";
#endif
    }

    return ss.str();
}

std::string PropVariantTypeToString(VARTYPE vt) {
    switch (vt) {
    case VT_EMPTY:
        return "VT_EMPTY";
    case VT_NULL:
        return "VT_NULL";
    case VT_I2:
        return "VT_I2";
    case VT_I4:
        return "VT_I4";
    case VT_R4:
        return "VT_R4";
    case VT_R8:
        return "VT_R8";
    case VT_BSTR:
        return "VT_BSTR";
    case VT_BOOL:
        return "VT_BOOL";
    case VT_UI4:
        return "VT_UI4";
    case VT_LPWSTR:
        return "VT_LPWSTR";
    default:
        return "VT_" + std::to_string(vt);
    }
}

bool LogIfFailed(HRESULT hr, const std::string& context) {
    if (SUCCEEDED(hr)) {
        return false;
    }

    LOG_ERROR(context + ": " + HResultToString(hr));
    return true;
}

void ThrowIfFailed(HRESULT hr, const std::string& context) {
    if (SUCCEEDED(hr)) {
        return;
    }

    const std::string message = context + ": " + HResultToString(hr);
    LOG_ERROR(message);
    throw std::runtime_error(message);
}

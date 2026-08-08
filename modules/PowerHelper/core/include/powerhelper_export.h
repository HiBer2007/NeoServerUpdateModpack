#pragma once

#if defined(POWERHELPER_STATIC)
#  define PH_API
#else
#  if defined(PowerHelperCore_EXPORTS)
#    define PH_API __declspec(dllexport)
#  else
#    define PH_API __declspec(dllimport)
#  endif
#endif

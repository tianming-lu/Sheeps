#ifndef _CONFIG_H_
#define _CONFIG_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#if defined COMMON_LIB || defined STRESS_EXPORTS
#define Config_API __declspec(dllexport)
#else
#define Config_API __declspec(dllimport)
#endif // COMMON_LIB
#else
#define Config_API
#endif // __WINDOWS__

#ifdef __cplusplus
extern "C"
{
#endif

	Config_API int		config_init(const char* file);
	Config_API int		netaddr_in_config(const char* ip, int port, const char* section, const char* key);
	Config_API const char* config_get_string_value(const char* section, const char* key, const char* def);
	Config_API int		config_get_int_value(const char* section, const char* key, int def);
	Config_API bool		config_get_bool_value(const char* section, const char* key, bool def);

#ifdef __cplusplus
}
#endif

#endif // !_CONFIG_H_
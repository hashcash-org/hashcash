#if !defined( _MYDLL )
#if defined( __cplusplus )
    #define cppfudge "C"
#else
    #define cppfudge
#endif

#ifdef BUILD_DLL
    #define EXPORT __declspec(dllexport)
#elif USE_DLL
    #define EXPORT extern cppfudge __declspec(dllimport)
#else
    #define EXPORT
#endif

#endif


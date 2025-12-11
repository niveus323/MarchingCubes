#include "pch.h"

#ifdef compiling_libMC33
#undef compiling_libMC33
#endif

#ifdef __cplusplus
extern "C" {
#endif
#pragma warning(push)
#pragma warning(disable: 4251 4244 26451 26495) // 무시할 경고 번호들
#include "MC33_c/marching_cubes_33.h"
#pragma warning(pop)

#ifdef __cplusplus
}
#endif
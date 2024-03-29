# Copyright 2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

add_subdirectory(xrt)

add_library(xrt-interfaces INTERFACE)
target_include_directories(
	xrt-interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	)

if(XRT_FEATURE_OPENXR_BODY_TRACKING_FULL_BODY_META
   OR XRT_FEATURE_OPENXR_BODY_TRACKING_FIDELITY_META
	)
	set(OCULUS_MOBILE_OPENXR_SDK_URL
	    "https://securecdn.oculus.com/binaries/download/?id=7329852993719891"
		)
	include(FetchContent)
	if(CMAKE_VERSION VERSION_LESS "3.24.0")
		FetchContent_Declare(oculussdk URL ${OCULUS_MOBILE_OPENXR_SDK_URL} EXCLUDE_FROM_ALL)
	else()
		FetchContent_Declare(
			oculussdk URL ${OCULUS_MOBILE_OPENXR_SDK_URL} DOWNLOAD_EXTRACT_TIMESTAMP
				      TRUE EXCLUDE_FROM_ALL
			)
	endif()
	FetchContent_GetProperties(oculussdk)
	if(NOT oculussdk_POPULATED)
		FetchContent_Populate(oculussdk)
		add_subdirectory(
			${oculussdk_SOURCE_DIR}/OpenXR ${oculussdk_BINARY_DIR}/OpenXR
			EXCLUDE_FROM_ALL
			)
	endif()
	target_include_directories(
		xrt-interfaces INTERFACE "${oculussdk_SOURCE_DIR}/OpenXR/Include"
		)
	target_compile_definitions(xrt-interfaces INTERFACE XRT_USE_META_EXT_HEADERS)
endif()

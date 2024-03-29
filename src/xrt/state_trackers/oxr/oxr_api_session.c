// Copyright 2019-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Session entrypoints for the OpenXR state tracker.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup oxr_api
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "xrt/xrt_compiler.h"

#include "util/u_debug.h"
#include "util/u_trace_marker.h"

#include "math/m_api.h"
#include "math/m_mathinclude.h"
#include "math/m_space.h"

#include "oxr_objects.h"
#include "oxr_logger.h"
#include "oxr_two_call.h"

#include "oxr_api_funcs.h"
#include "oxr_api_verify.h"
#include "oxr_handle.h"
#include "oxr_chain.h"
#include "oxr_conversions.h"


XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateSession(XrInstance instance, const XrSessionCreateInfo *createInfo, XrSession *out_session)
{
	OXR_TRACE_MARKER();

	XrResult ret;
	struct oxr_instance *inst;
	struct oxr_session *sess;
	struct oxr_session **link;
	struct oxr_logger log;
	OXR_VERIFY_INSTANCE_AND_INIT_LOG(&log, instance, inst, "xrCreateSession");

	ret = oxr_verify_XrSessionCreateInfo(&log, inst, createInfo);
	if (ret != XR_SUCCESS) {
		return ret;
	}

	ret = oxr_session_create(&log, &inst->system, createInfo, &sess);
	if (ret != XR_SUCCESS) {
		return ret;
	}

	*out_session = oxr_session_to_openxr(sess);

	/* Add to session list */
	link = &inst->sessions;
	while (*link) {
		link = &(*link)->next;
	}
	*link = sess;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroySession(XrSession session)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_session **link;
	struct oxr_instance *inst;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrDestroySession");

	/* Remove from session list */
	inst = sess->sys->inst;
	link = &inst->sessions;
	while (*link != sess) {
		link = &(*link)->next;
	}
	*link = sess->next;

	return oxr_handle_destroy(&log, &sess->handle);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrBeginSession(XrSession session, const XrSessionBeginInfo *beginInfo)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrBeginSession");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, beginInfo, XR_TYPE_SESSION_BEGIN_INFO);
	OXR_VERIFY_VIEW_CONFIG_TYPE(&log, sess->sys->inst, beginInfo->primaryViewConfigurationType);

	if (sess->has_begun) {
		return oxr_error(&log, XR_ERROR_SESSION_RUNNING, "Session is already running");
	}

	return oxr_session_begin(&log, sess, beginInfo);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEndSession(XrSession session)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrEndSession");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_SESSION_RUNNING(&log, sess);

	return oxr_session_end(&log, sess);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrWaitFrame(XrSession session, const XrFrameWaitInfo *frameWaitInfo, XrFrameState *frameState)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrWaitFrame");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_SESSION_RUNNING(&log, sess);
	OXR_VERIFY_ARG_TYPE_CAN_BE_NULL(&log, frameWaitInfo, XR_TYPE_FRAME_WAIT_INFO);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, frameState, XR_TYPE_FRAME_STATE);
	OXR_VERIFY_ARG_NOT_NULL(&log, frameState);

	return oxr_session_frame_wait(&log, sess, frameState);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrBeginFrame(XrSession session, const XrFrameBeginInfo *frameBeginInfo)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrBeginFrame");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_SESSION_RUNNING(&log, sess);
	// NULL explicitly allowed here because it's a basically empty struct.
	OXR_VERIFY_ARG_TYPE_CAN_BE_NULL(&log, frameBeginInfo, XR_TYPE_FRAME_BEGIN_INFO);

	XrResult res = oxr_session_frame_begin(&log, sess);

#ifdef XRT_FEATURE_RENDERDOC
	if (sess->sys->inst->rdoc_api) {
#ifndef XR_USE_PLATFORM_ANDROID
		sess->sys->inst->rdoc_api->StartFrameCapture(NULL, NULL);
#endif
	}
#endif

	return res;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEndFrame(XrSession session, const XrFrameEndInfo *frameEndInfo)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrEndFrame");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_SESSION_RUNNING(&log, sess);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, frameEndInfo, XR_TYPE_FRAME_END_INFO);

#ifdef XRT_FEATURE_RENDERDOC
	if (sess->sys->inst->rdoc_api) {
#ifndef XR_USE_PLATFORM_ANDROID
		sess->sys->inst->rdoc_api->EndFrameCapture(NULL, NULL);
#endif
	}
#endif

	XrResult res = oxr_session_frame_end(&log, sess, frameEndInfo);

	return res;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrRequestExitSession(XrSession session)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrRequestExitSession");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_SESSION_RUNNING(&log, sess);

	return oxr_session_request_exit(&log, sess);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateViews(XrSession session,
                  const XrViewLocateInfo *viewLocateInfo,
                  XrViewState *viewState,
                  uint32_t viewCapacityInput,
                  uint32_t *viewCountOutput,
                  XrView *views)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_space *spc;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrLocateViews");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, viewLocateInfo, XR_TYPE_VIEW_LOCATE_INFO);
	OXR_VERIFY_SPACE_NOT_NULL(&log, viewLocateInfo->space, spc);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, viewState, XR_TYPE_VIEW_STATE);
	OXR_VERIFY_VIEW_CONFIG_TYPE(&log, sess->sys->inst, viewLocateInfo->viewConfigurationType);

	if (viewCapacityInput == 0) {
		OXR_VERIFY_ARG_NOT_NULL(&log, viewCountOutput);
	} else {
		OXR_VERIFY_ARG_NOT_NULL(&log, views);
	}

	for (uint32_t i = 0; i < viewCapacityInput; i++) {
		OXR_VERIFY_ARG_ARRAY_ELEMENT_TYPE(&log, views, i, XR_TYPE_VIEW);
	}

	if (viewLocateInfo->displayTime <= (XrTime)0) {
		return oxr_error(&log, XR_ERROR_TIME_INVALID, "(time == %" PRIi64 ") is not a valid time.",
		                 viewLocateInfo->displayTime);
	}

	if (viewLocateInfo->viewConfigurationType != sess->sys->view_config_type) {
		return oxr_error(&log, XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED,
		                 "(viewConfigurationType == 0x%08x) "
		                 "unsupported view configuration type",
		                 viewLocateInfo->viewConfigurationType);
	}

	return oxr_session_locate_views( //
	    &log,                        //
	    sess,                        //
	    viewLocateInfo,              //
	    viewState,                   //
	    viewCapacityInput,           //
	    viewCountOutput,             //
	    views);                      //
}


/*
 *
 * XR_KHR_visibility_mask
 *
 */

#ifdef OXR_HAVE_KHR_visibility_mask
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetVisibilityMaskKHR(XrSession session,
                           XrViewConfigurationType viewConfigurationType,
                           uint32_t viewIndex,
                           XrVisibilityMaskTypeKHR visibilityMaskType,
                           XrVisibilityMaskKHR *visibilityMask)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess = NULL;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrGetVisibilityMaskKHR");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);

	OXR_VERIFY_EXTENSION(&log, sess->sys->inst, KHR_visibility_mask);

	visibilityMask->vertexCountOutput = 0;
	visibilityMask->indexCountOutput = 0;

	OXR_VERIFY_VIEW_CONFIG_TYPE(&log, sess->sys->inst, viewConfigurationType);
	if (viewConfigurationType != sess->sys->view_config_type) {
		return oxr_error(&log, XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED,
		                 "(viewConfigurationType == 0x%08x) unsupported view configuration type",
		                 viewConfigurationType);
	}

	OXR_VERIFY_VIEW_INDEX(&log, viewIndex);

	if (visibilityMaskType != XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR &&
	    visibilityMaskType != XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR &&
	    visibilityMaskType != XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR) {
		return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE, "(visibilityMaskType == %d) is invalid",
		                 visibilityMaskType);
	}

	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, visibilityMask, XR_TYPE_VISIBILITY_MASK_KHR);

	if (visibilityMask->vertexCapacityInput != 0) {
		OXR_VERIFY_ARG_NOT_NULL(&log, visibilityMask->vertices);
	}

	if (visibilityMask->indexCapacityInput != 0) {
		OXR_VERIFY_ARG_NOT_NULL(&log, visibilityMask->indices);
	}

	return oxr_session_get_visibility_mask(&log, sess, visibilityMaskType, viewIndex, visibilityMask);
}
#endif // OXR_HAVE_KHR_visibility_mask


/*
 *
 * XR_EXT_performance_settings
 *
 */

#ifdef OXR_HAVE_EXT_performance_settings
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrPerfSettingsSetPerformanceLevelEXT(XrSession session,
                                         XrPerfSettingsDomainEXT domain,
                                         XrPerfSettingsLevelEXT level)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrPerfSettingsSetPerformanceLevelEXT");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_EXTENSION(&log, sess->sys->inst, EXT_performance_settings);
	// check parameters
	if (domain != XR_PERF_SETTINGS_DOMAIN_CPU_EXT && domain != XR_PERF_SETTINGS_DOMAIN_GPU_EXT) {
		return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE, "Invalid domain %d, must be 1(CPU) or 2(GPU)",
		                 domain);
	}

	if (level != XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT && level != XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT &&
	    level != XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT && level != XR_PERF_SETTINGS_LEVEL_BOOST_EXT) {
		return oxr_error(
		    &log, XR_ERROR_VALIDATION_FAILURE,
		    "Invalid level %d, must be 0(POWER SAVE), 25(SUSTAINED LOW), 50(SUSTAINED_HIGH) or 75(BOOST)",
		    level);
	}

	return oxr_session_set_perf_level(&log, sess, domain, level);
}
#endif // OXR_HAVE_EXT_performance_settings


/*
 *
 * XR_EXT_thermal_query
 *
 */

#ifdef XR_EXT_thermal_query

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrThermalGetTemperatureTrendEXT(XrSession session,
                                    XrPerfSettingsDomainEXT domain,
                                    XrPerfSettingsNotificationLevelEXT *notificationLevel,
                                    float *tempHeadroom,
                                    float *tempSlope)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrThermalGetTemperatureTrendEXT");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);

	return oxr_error(&log, XR_ERROR_HANDLE_INVALID, "Not implemented");
}

#endif

/*
 *
 * XR_EXT_hand_tracking
 *
 */

#ifdef XR_EXT_hand_tracking

static XrResult
oxr_hand_tracker_destroy_cb(struct oxr_logger *log, struct oxr_handle_base *hb)
{
	struct oxr_hand_tracker *hand_tracker = (struct oxr_hand_tracker *)hb;

	free(hand_tracker);

	return XR_SUCCESS;
}

XrResult
oxr_hand_tracker_create(struct oxr_logger *log,
                        struct oxr_session *sess,
                        const XrHandTrackerCreateInfoEXT *createInfo,
                        struct oxr_hand_tracker **out_hand_tracker)
{
	if (!oxr_system_get_hand_tracking_support(log, sess->sys->inst)) {
		return oxr_error(log, XR_ERROR_FEATURE_UNSUPPORTED, "System does not support hand tracking");
	}

	struct oxr_hand_tracker *hand_tracker = NULL;
	OXR_ALLOCATE_HANDLE_OR_RETURN(log, hand_tracker, OXR_XR_DEBUG_HTRACKER, oxr_hand_tracker_destroy_cb,
	                              &sess->handle);

	hand_tracker->sess = sess;
	hand_tracker->hand = createInfo->hand;
	hand_tracker->hand_joint_set = createInfo->handJointSet;

	// Find the assigned device.
	struct xrt_device *xdev = NULL;
	if (createInfo->hand == XR_HAND_LEFT_EXT) {
		xdev = GET_XDEV_BY_ROLE(sess->sys, hand_tracking_left);
	} else if (createInfo->hand == XR_HAND_RIGHT_EXT) {
		xdev = GET_XDEV_BY_ROLE(sess->sys, hand_tracking_right);
	}

	// Find the correct input on the device.
	if (xdev != NULL && xdev->hand_tracking_supported) {
		for (uint32_t j = 0; j < xdev->input_count; j++) {
			struct xrt_input *input = &xdev->inputs[j];

			if ((input->name == XRT_INPUT_GENERIC_HAND_TRACKING_LEFT &&
			     createInfo->hand == XR_HAND_LEFT_EXT) ||
			    (input->name == XRT_INPUT_GENERIC_HAND_TRACKING_RIGHT &&
			     createInfo->hand == XR_HAND_RIGHT_EXT)) {
				hand_tracker->xdev = xdev;
				hand_tracker->input_name = input->name;
				break;
			}
		}
	}

	// Consistency checking.
	if (xdev != NULL && hand_tracker->xdev == NULL) {
		oxr_warn(log, "We got hand tracking xdev but it didn't have a hand tracking input.");
	}

	*out_hand_tracker = hand_tracker;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateHandTrackerEXT(XrSession session,
                           const XrHandTrackerCreateInfoEXT *createInfo,
                           XrHandTrackerEXT *handTracker)
{
	OXR_TRACE_MARKER();

	struct oxr_hand_tracker *hand_tracker = NULL;
	struct oxr_session *sess = NULL;
	struct oxr_logger log;
	XrResult ret;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrCreateHandTrackerEXT");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, createInfo, XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT);
	OXR_VERIFY_ARG_NOT_NULL(&log, handTracker);

	OXR_VERIFY_EXTENSION(&log, sess->sys->inst, EXT_hand_tracking);

	if (createInfo->hand != XR_HAND_LEFT_EXT && createInfo->hand != XR_HAND_RIGHT_EXT) {
		return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE, "Invalid hand value %d\n", createInfo->hand);
	}

	ret = oxr_hand_tracker_create(&log, sess, createInfo, &hand_tracker);
	if (ret != XR_SUCCESS) {
		return ret;
	}

	*handTracker = oxr_hand_tracker_to_openxr(hand_tracker);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyHandTrackerEXT(XrHandTrackerEXT handTracker)
{
	OXR_TRACE_MARKER();

	struct oxr_hand_tracker *hand_tracker;
	struct oxr_logger log;
	OXR_VERIFY_HAND_TRACKER_AND_INIT_LOG(&log, handTracker, hand_tracker, "xrDestroyHandTrackerEXT");

	return oxr_handle_destroy(&log, &hand_tracker->handle);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateHandJointsEXT(XrHandTrackerEXT handTracker,
                          const XrHandJointsLocateInfoEXT *locateInfo,
                          XrHandJointLocationsEXT *locations)
{
	OXR_TRACE_MARKER();

	struct oxr_hand_tracker *hand_tracker;
	struct oxr_space *spc;
	struct oxr_logger log;
	OXR_VERIFY_HAND_TRACKER_AND_INIT_LOG(&log, handTracker, hand_tracker, "xrLocateHandJointsEXT");
	OXR_VERIFY_SESSION_NOT_LOST(&log, hand_tracker->sess);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, locateInfo, XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, locations, XR_TYPE_HAND_JOINT_LOCATIONS_EXT);
	OXR_VERIFY_ARG_NOT_NULL(&log, locations->jointLocations);
	OXR_VERIFY_SPACE_NOT_NULL(&log, locateInfo->baseSpace, spc);


	if (locateInfo->time <= (XrTime)0) {
		return oxr_error(&log, XR_ERROR_TIME_INVALID, "(time == %" PRIi64 ") is not a valid time.",
		                 locateInfo->time);
	}

	if (hand_tracker->hand_joint_set == XR_HAND_JOINT_SET_DEFAULT_EXT) {
		if (locations->jointCount != XR_HAND_JOINT_COUNT_EXT) {
			return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE, "joint count must be %d, not %d\n",
			                 XR_HAND_JOINT_COUNT_EXT, locations->jointCount);
		}
	};

	XrHandJointVelocitiesEXT *vel =
	    OXR_GET_OUTPUT_FROM_CHAIN(locations, XR_TYPE_HAND_JOINT_VELOCITIES_EXT, XrHandJointVelocitiesEXT);
	if (vel) {
		if (vel->jointCount <= 0) {
			return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE,
			                 "XrHandJointVelocitiesEXT joint count "
			                 "must be >0, is %d\n",
			                 vel->jointCount);
		}
		if (hand_tracker->hand_joint_set == XR_HAND_JOINT_SET_DEFAULT_EXT) {
			if (vel->jointCount != XR_HAND_JOINT_COUNT_EXT) {
				return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE,
				                 "XrHandJointVelocitiesEXT joint count must "
				                 "be %d, not %d\n",
				                 XR_HAND_JOINT_COUNT_EXT, locations->jointCount);
			}
		}
	}

	return oxr_session_hand_joints(&log, hand_tracker, locateInfo, locations);
}

#endif

/*
 *
 * XR_MNDX_force_feedback_curl
 *
 */

#ifdef XR_MNDX_force_feedback_curl

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrApplyForceFeedbackCurlMNDX(XrHandTrackerEXT handTracker, const XrForceFeedbackCurlApplyLocationsMNDX *locations)
{
	OXR_TRACE_MARKER();

	struct oxr_hand_tracker *hand_tracker;
	struct oxr_logger log;
	OXR_VERIFY_HAND_TRACKER_AND_INIT_LOG(&log, handTracker, hand_tracker, "xrApplyForceFeedbackCurlMNDX");
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, locations, XR_TYPE_FORCE_FEEDBACK_CURL_APPLY_LOCATIONS_MNDX);

	return oxr_session_apply_force_feedback(&log, hand_tracker, locations);
}

#endif

/*
 *
 * XR_FB_display_refresh_rate
 *
 */

#ifdef OXR_HAVE_FB_display_refresh_rate

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrEnumerateDisplayRefreshRatesFB(XrSession session,
                                     uint32_t displayRefreshRateCapacityInput,
                                     uint32_t *displayRefreshRateCountOutput,
                                     float *displayRefreshRates)
{
	struct oxr_session *sess = NULL;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrEnumerateDisplayRefreshRatesFB");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);

	// headless
	if (!sess->sys->xsysc) {
		*displayRefreshRateCountOutput = 0;
		return XR_SUCCESS;
	}

	OXR_TWO_CALL_HELPER(&log, displayRefreshRateCapacityInput, displayRefreshRateCountOutput, displayRefreshRates,
	                    sess->sys->xsysc->info.refresh_rate_count, sess->sys->xsysc->info.refresh_rates_hz,
	                    XR_SUCCESS);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetDisplayRefreshRateFB(XrSession session, float *displayRefreshRate)
{
	struct oxr_session *sess = NULL;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrGetDisplayRefreshRateFB");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);

	// headless
	if (!sess->sys->xsysc) {
		*displayRefreshRate = 0.0f;
		return XR_SUCCESS;
	}

	if (sess->sys->xsysc->info.refresh_rate_count < 1) {
		return XR_ERROR_RUNTIME_FAILURE;
	}

	return oxr_session_get_display_refresh_rate(&log, sess, displayRefreshRate);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrRequestDisplayRefreshRateFB(XrSession session, float displayRefreshRate)
{
	struct oxr_session *sess = NULL;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrRequestDisplayRefreshRateFB");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);

	if (displayRefreshRate == 0.0f) {
		return XR_SUCCESS;
	}

	/*
	 * For the requested display refresh rate, truncating to two decimal
	 * places and checking if it's in the supported refresh rates.
	 */
	bool found = false;
	for (int i = 0; i < (int)sess->sys->xsysc->info.refresh_rate_count; ++i) {
		if ((int)(displayRefreshRate * 100.0f) == (int)(sess->sys->xsysc->info.refresh_rates_hz[i] * 100.0f)) {
			found = true;
			break;
		}
	}
	if (!found) {
		return XR_ERROR_DISPLAY_REFRESH_RATE_UNSUPPORTED_FB;
	}

	return oxr_session_request_display_refresh_rate(&log, sess, displayRefreshRate);
}

#endif

/*
 *
 * XR_KHR_android_thread_settings
 *
 */

#ifdef OXR_HAVE_KHR_android_thread_settings

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrSetAndroidApplicationThreadKHR(XrSession session, XrAndroidThreadTypeKHR threadType, uint32_t threadId)
{
	OXR_TRACE_MARKER();

	struct oxr_session *sess = NULL;
	struct oxr_logger log;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrSetAndroidApplicationThreadKHR");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);

	if (threadType != XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR &&
	    threadType != XR_ANDROID_THREAD_TYPE_APPLICATION_WORKER_KHR &&
	    threadType != XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR &&
	    threadType != XR_ANDROID_THREAD_TYPE_RENDERER_WORKER_KHR) {
		return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE, "(threadType == %d) is invalid", threadType);
	}

	OXR_VERIFY_EXTENSION(&log, sess->sys->inst, KHR_android_thread_settings);

	return oxr_session_android_thread_settings(&log, sess, threadType, threadId);
}

#endif

#ifdef OXR_HAVE_HTC_facial_tracking

static enum xrt_facial_tracking_type_htc
oxr_to_xrt_facial_tracking_type_htc(enum XrFacialTrackingTypeHTC ft_type)
{
	return (enum xrt_facial_tracking_type_htc)ft_type;
}

static enum xrt_input_name
oxr_facial_tracking_type_htc_to_input_name(enum xrt_facial_tracking_type_htc ft_type)
{
	switch (ft_type) {
	case XRT_FACIAL_TRACKING_TYPE_LIP_DEFAULT_HTC: return XRT_INPUT_HTC_LIP_FACE_TRACKING;
	case XRT_FACIAL_TRACKING_TYPE_EYE_DEFAULT_HTC:
	default: return XRT_INPUT_HTC_EYE_FACE_TRACKING;
	}
}

static XrResult
oxr_facial_tracker_htc_destroy_cb(struct oxr_logger *log, struct oxr_handle_base *hb)
{
	struct oxr_facial_tracker_htc *face_tracker_htc = (struct oxr_facial_tracker_htc *)hb;
	free(face_tracker_htc);
	return XR_SUCCESS;
}

XrResult
oxr_facial_tracker_htc_create(struct oxr_logger *log,
                              struct oxr_session *sess,
                              const XrFacialTrackerCreateInfoHTC *createInfo,
                              struct oxr_facial_tracker_htc **out_face_tracker_htc)
{
	bool supports_eye = false;
	bool supports_lip = false;
	oxr_system_get_face_tracking_htc_support(log, sess->sys->inst, &supports_eye, &supports_lip);

	const enum xrt_facial_tracking_type_htc facial_tracking_type =
	    oxr_to_xrt_facial_tracking_type_htc(createInfo->facialTrackingType);

	if (facial_tracking_type == XRT_FACIAL_TRACKING_TYPE_EYE_DEFAULT_HTC && !supports_eye) {
		return oxr_error(log, XR_ERROR_FEATURE_UNSUPPORTED, "System does not support HTC eye facial tracking");
	}
	if (facial_tracking_type == XRT_FACIAL_TRACKING_TYPE_LIP_DEFAULT_HTC && !supports_lip) {
		return oxr_error(log, XR_ERROR_FEATURE_UNSUPPORTED, "System does not support HTC lip facial tracking");
	}

	struct xrt_device *xdev = GET_XDEV_BY_ROLE(sess->sys, face);
	if (xdev == NULL) {
		return oxr_error(log, XR_ERROR_FEATURE_UNSUPPORTED, "No device found for face tracking role");
	}

	if (!xdev->face_tracking_supported) {
		return oxr_error(log, XR_ERROR_FEATURE_UNSUPPORTED, "Device does not support HTC facial tracking");
	}

	struct oxr_facial_tracker_htc *face_tracker_htc = NULL;
	OXR_ALLOCATE_HANDLE_OR_RETURN(log, face_tracker_htc, OXR_XR_DEBUG_FTRACKER, oxr_facial_tracker_htc_destroy_cb,
	                              &sess->handle);

	face_tracker_htc->sess = sess;
	face_tracker_htc->xdev = xdev;
	face_tracker_htc->facial_tracking_type = facial_tracking_type;

	*out_face_tracker_htc = face_tracker_htc;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateFacialTrackerHTC(XrSession session,
                             const XrFacialTrackerCreateInfoHTC *createInfo,
                             XrFacialTrackerHTC *facialTracker)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	XrResult ret = XR_SUCCESS;
	struct oxr_session *sess = NULL;
	struct oxr_facial_tracker_htc *facial_tracker_htc = NULL;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrCreateFacialTrackerHTC");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, createInfo, XR_TYPE_FACIAL_TRACKER_CREATE_INFO_HTC);
	OXR_VERIFY_EXTENSION(&log, sess->sys->inst, HTC_facial_tracking);

	ret = oxr_facial_tracker_htc_create(&log, sess, createInfo, &facial_tracker_htc);
	if (ret != XR_SUCCESS) {
		return ret;
	}

	OXR_VERIFY_ARG_NOT_NULL(&log, facial_tracker_htc);
	*facialTracker = oxr_facial_tracker_htc_to_openxr(facial_tracker_htc);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyFacialTrackerHTC(XrFacialTrackerHTC facialTracker)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	struct oxr_facial_tracker_htc *facial_tracker_htc = NULL;
	OXR_VERIFY_FACE_TRACKER_HTC_AND_INIT_LOG(&log, facialTracker, facial_tracker_htc, "xrDestroyFacialTrackerHTC");

	return oxr_handle_destroy(&log, &facial_tracker_htc->handle);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetFacialExpressionsHTC(XrFacialTrackerHTC facialTracker, XrFacialExpressionsHTC *facialExpressions)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	struct oxr_facial_tracker_htc *facial_tracker_htc = NULL;
	OXR_VERIFY_FACE_TRACKER_HTC_AND_INIT_LOG(&log, facialExpressions, facial_tracker_htc,
	                                         "xrGetFacialExpressionsHTC");
	OXR_VERIFY_SESSION_NOT_LOST(&log, facial_tracker_htc->sess);
	OXR_VERIFY_ARG_NOT_NULL(&log, facial_tracker_htc->xdev);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, facialExpressions, XR_TYPE_FACIAL_EXPRESSIONS_HTC);
	OXR_VERIFY_ARG_NOT_NULL(&log, facialExpressions->expressionWeightings);

#define OXR_VERIFY_FACE_EXPRESSION_COUNT(fttype)                                                                       \
	if (facial_tracker_htc->facial_tracking_type == XRT_FACIAL_TRACKING_TYPE_##fttype##_DEFAULT_HTC &&             \
	    facialExpressions->expressionCount < XRT_FACIAL_EXPRESSION_##fttype##_COUNT_HTC) {                         \
		return oxr_error(                                                                                      \
		    &log, XR_ERROR_SIZE_INSUFFICIENT,                                                                  \
		    "\"expressionCount\" (%d) size is less than the minimum size (%d) required for " #fttype           \
		    " expressions.\n",                                                                                 \
		    facialExpressions->expressionCount, XRT_FACIAL_EXPRESSION_##fttype##_COUNT_HTC);                   \
	}

	OXR_VERIFY_FACE_EXPRESSION_COUNT(EYE)
	OXR_VERIFY_FACE_EXPRESSION_COUNT(LIP)
#undef OXR_VERIFY_FACE_EXPRESSION_COUNT

	const bool is_eye_tracking =
	    facial_tracker_htc->facial_tracking_type == XRT_FACIAL_TRACKING_TYPE_EYE_DEFAULT_HTC;
	const size_t expression_count =
	    is_eye_tracking ? XRT_FACIAL_EXPRESSION_EYE_COUNT_HTC : XRT_FACIAL_EXPRESSION_LIP_COUNT_HTC;

	struct xrt_facial_expression_set facial_expression_set_result = {0};
	float *expression_weights = is_eye_tracking
	                                ? facial_expression_set_result.eye_expression_set_htc.expression_weights
	                                : facial_expression_set_result.lip_expression_set_htc.expression_weights;
	memset(expression_weights, 0, sizeof(float) * expression_count);

	const enum xrt_input_name ft_input_name =
	    oxr_facial_tracking_type_htc_to_input_name(facial_tracker_htc->facial_tracking_type);

	xrt_device_get_face_tracking(facial_tracker_htc->xdev, ft_input_name, &facial_expression_set_result);

	facialExpressions->isActive = facial_expression_set_result.base_expression_set_htc.is_active;
	if (facialExpressions->isActive == XR_FALSE)
		return XR_SUCCESS;

	const struct oxr_instance *inst = facial_tracker_htc->sess->sys->inst;
	facialExpressions->sampleTime = time_state_monotonic_to_ts_ns(
	    inst->timekeeping, facial_expression_set_result.base_expression_set_htc.sample_time_ns);

	memcpy(facialExpressions->expressionWeightings, expression_weights, sizeof(float) * expression_count);

	return XR_SUCCESS;
}
#endif

#ifdef OXR_HAVE_FB_body_tracking
static enum xrt_body_joint_set_type_fb
oxr_to_xrt_body_joint_set_type_fb(XrBodyJointSetFB joint_set_type)
{
#ifdef OXR_HAVE_META_body_tracking_full_body
	if (joint_set_type == XR_BODY_JOINT_SET_FULL_BODY_META) {
		return XRT_BODY_JOINT_SET_FULL_BODY_META;
	}
#endif
	if (joint_set_type == XR_BODY_JOINT_SET_DEFAULT_FB) {
		return XRT_BODY_JOINT_SET_DEFAULT_FB;
	}
	return XRT_BODY_JOINT_SET_UNKNOWN;
}

static XrResult
oxr_body_tracker_fb_destroy_cb(struct oxr_logger *log, struct oxr_handle_base *hb)
{
	struct oxr_body_tracker_fb *body_tracker_fb = (struct oxr_body_tracker_fb *)hb;
	free(body_tracker_fb);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrCreateBodyTrackerFB(XrSession session, const XrBodyTrackerCreateInfoFB *createInfo, XrBodyTrackerFB *bodyTracker)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	XrResult ret = XR_SUCCESS;
	struct oxr_session *sess = NULL;
	OXR_VERIFY_SESSION_AND_INIT_LOG(&log, session, sess, "xrCreateBodyTrackerFB");
	OXR_VERIFY_SESSION_NOT_LOST(&log, sess);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, createInfo, XR_TYPE_BODY_TRACKER_CREATE_INFO_FB);
	OXR_VERIFY_EXTENSION(&log, sess->sys->inst, FB_body_tracking);
#ifdef OXR_HAVE_META_body_tracking_full_body
	if (createInfo->bodyJointSet == XR_BODY_JOINT_SET_FULL_BODY_META) {
		OXR_VERIFY_EXTENSION(&log, sess->sys->inst, META_body_tracking_full_body);
	}
#endif

	if (!oxr_system_get_body_tracking_fb_support(&log, sess->sys->inst)) {
		return oxr_error(&log, XR_ERROR_FEATURE_UNSUPPORTED, "System does not support FB body tracking");
	}

	const enum xrt_body_joint_set_type_fb joint_set_type =
	    oxr_to_xrt_body_joint_set_type_fb(createInfo->bodyJointSet);

	if (joint_set_type == XRT_BODY_JOINT_SET_UNKNOWN) {
		return oxr_error(&log, XR_ERROR_FEATURE_UNSUPPORTED,
		                 "\"bodyJointSet\" set to an unknown body joint set type");
	}

#ifdef OXR_HAVE_META_body_tracking_full_body
	if (joint_set_type == XRT_BODY_JOINT_SET_FULL_BODY_META) {
		if (!oxr_system_get_full_body_tracking_meta_support(&log, sess->sys->inst)) {
			return oxr_error(&log, XR_ERROR_FEATURE_UNSUPPORTED,
			                 "System does not support META full body tracking");
		}
	}
#endif

	struct xrt_device *xdev = GET_XDEV_BY_ROLE(sess->sys, body);
	if (xdev == NULL || !xdev->body_tracking_supported) {
		return oxr_error(&log, XR_ERROR_FEATURE_UNSUPPORTED, "No device found for body tracking role");
	}

	struct oxr_body_tracker_fb *body_tracker_fb = NULL;
	OXR_ALLOCATE_HANDLE_OR_RETURN(&log, body_tracker_fb, OXR_XR_DEBUG_BTRACKER, oxr_body_tracker_fb_destroy_cb,
	                              &sess->handle);

	body_tracker_fb->sess = sess;
	body_tracker_fb->xdev = xdev;
	body_tracker_fb->joint_set_type = joint_set_type;

	OXR_VERIFY_ARG_NOT_NULL(&log, body_tracker_fb);
	*bodyTracker = oxr_body_tracker_fb_to_openxr(body_tracker_fb);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrDestroyBodyTrackerFB(XrBodyTrackerFB bodyTracker)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	struct oxr_body_tracker_fb *body_tracker_fb = NULL;
	OXR_VERIFY_BODY_TRACKER_FB_AND_INIT_LOG(&log, bodyTracker, body_tracker_fb, "xrDestroyBodyTrackerFB");

	return oxr_handle_destroy(&log, &body_tracker_fb->handle);
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrGetBodySkeletonFB(XrBodyTrackerFB bodyTracker, XrBodySkeletonFB *skeleton)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	struct oxr_body_tracker_fb *body_tracker_fb = NULL;
	OXR_VERIFY_BODY_TRACKER_FB_AND_INIT_LOG(&log, bodyTracker, body_tracker_fb, "xrGetBodySkeletonFB");
	OXR_VERIFY_SESSION_NOT_LOST(&log, body_tracker_fb->sess);
	OXR_VERIFY_ARG_NOT_NULL(&log, body_tracker_fb->xdev);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, skeleton, XR_TYPE_BODY_SKELETON_FB);

	if (body_tracker_fb->xdev == NULL || !body_tracker_fb->xdev->body_tracking_supported) {
		return oxr_error(&log, XR_ERROR_FUNCTION_UNSUPPORTED,
		                 "Device not found or does not support body tracking.");
	}

	const bool is_meta_full_body = body_tracker_fb->joint_set_type == XRT_BODY_JOINT_SET_FULL_BODY_META;
	const uint32_t body_joint_count = is_meta_full_body ? XRT_FULL_BODY_JOINT_COUNT_META : XRT_BODY_JOINT_COUNT_FB;

	if (skeleton->jointCount < body_joint_count) {
		return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE, "joint count is too small");
	}

	struct xrt_body_skeleton body_skeleton_result = {0};
	struct xrt_body_skeleton_joint_fb *src_skeleton_joints =
	    is_meta_full_body ? body_skeleton_result.full_body_skeleton_meta.joints
	                      : body_skeleton_result.body_skeleton_fb.joints;

	const enum xrt_input_name input_name =
	    is_meta_full_body ? XRT_INPUT_META_FULL_BODY_TRACKING : XRT_INPUT_FB_BODY_TRACKING;

	if (xrt_device_get_body_skeleton(body_tracker_fb->xdev, input_name, &body_skeleton_result) != XRT_SUCCESS) {
		return oxr_error(&log, XR_ERROR_RUNTIME_FAILURE, "Failed to get body skeleton");
	}

	for (size_t joint_index = 0; joint_index < body_joint_count; ++joint_index) {
		const struct xrt_body_skeleton_joint_fb *src_skeleton_joint = &src_skeleton_joints[joint_index];
		XrBodySkeletonJointFB *dst_skeleton_joint = &skeleton->joints[joint_index];
		OXR_XRT_POSE_TO_XRPOSEF(src_skeleton_joint->pose, dst_skeleton_joint->pose);
		dst_skeleton_joint->joint = src_skeleton_joint->joint;
		dst_skeleton_joint->parentJoint = src_skeleton_joint->parent_joint;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrLocateBodyJointsFB(XrBodyTrackerFB bodyTracker,
                         const XrBodyJointsLocateInfoFB *locateInfo,
                         XrBodyJointLocationsFB *locations)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	struct oxr_space *base_spc = NULL;
	struct oxr_body_tracker_fb *body_tracker_fb = NULL;
	OXR_VERIFY_BODY_TRACKER_FB_AND_INIT_LOG(&log, bodyTracker, body_tracker_fb, "xrLocateBodyJointsFB");
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, locateInfo, XR_TYPE_BODY_JOINTS_LOCATE_INFO_FB);
	OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(&log, locations, XR_TYPE_BODY_JOINT_LOCATIONS_FB);
	OXR_VERIFY_SESSION_NOT_LOST(&log, body_tracker_fb->sess);
	OXR_VERIFY_ARG_NOT_NULL(&log, body_tracker_fb->xdev);
	OXR_VERIFY_ARG_NOT_NULL(&log, locations->jointLocations);
	OXR_VERIFY_SPACE_NOT_NULL(&log, locateInfo->baseSpace, base_spc);

	if (body_tracker_fb->xdev == NULL || !body_tracker_fb->xdev->body_tracking_supported) {
		return oxr_error(&log, XR_ERROR_FUNCTION_UNSUPPORTED,
		                 "Device not found or does not support body tracking.");
	}

	const bool is_meta_full_body = body_tracker_fb->joint_set_type == XRT_BODY_JOINT_SET_FULL_BODY_META;
	const uint32_t body_joint_count = is_meta_full_body ? XRT_FULL_BODY_JOINT_COUNT_META : XRT_BODY_JOINT_COUNT_FB;

	if (locations->jointCount < body_joint_count) {
		return oxr_error(&log, XR_ERROR_VALIDATION_FAILURE, "joint count is too small");
	}

	if (locateInfo->time <= (XrTime)0) {
		return oxr_error(&log, XR_ERROR_TIME_INVALID, "(time == %" PRIi64 ") is not a valid time.",
		                 locateInfo->time);
	}

	const struct oxr_instance *inst = body_tracker_fb->sess->sys->inst;
	const uint64_t at_timestamp_ns = time_state_ts_to_monotonic_ns(inst->timekeeping, locateInfo->time);

	struct xrt_body_joint_set body_joint_set_result = {0};
	const struct xrt_body_joint_location_fb *src_body_joints =
	    is_meta_full_body ? body_joint_set_result.full_body_joint_set_meta.joint_locations
	                      : body_joint_set_result.body_joint_set_fb.joint_locations;

	const enum xrt_input_name input_name =
	    is_meta_full_body ? XRT_INPUT_META_FULL_BODY_TRACKING : XRT_INPUT_FB_BODY_TRACKING;

	if (xrt_device_get_body_joints(body_tracker_fb->xdev, input_name, at_timestamp_ns, &body_joint_set_result) !=
	    XRT_SUCCESS) {
		return oxr_error(&log, XR_ERROR_RUNTIME_FAILURE, "Failed to get FB body joint set");
	}

	// Get the body pose in the base space.
	struct xrt_space_relation T_base_body;
	const XrResult ret = oxr_get_base_body_pose(&log, &body_joint_set_result, base_spc, body_tracker_fb->xdev,
	                                            locateInfo->time, &T_base_body);
	if (ret != XR_SUCCESS) {
		locations->isActive = XR_FALSE;
		return ret;
	}

	const struct xrt_base_body_joint_set_meta *body_joint_set_fb = &body_joint_set_result.base_body_joint_set_meta;

	locations->isActive = body_joint_set_fb->is_active;
	if (!body_joint_set_fb->is_active || T_base_body.relation_flags == 0) {
		locations->isActive = XR_FALSE;
		for (size_t joint_index = 0; joint_index < XRT_BODY_JOINT_COUNT_FB; ++joint_index) {
			locations->jointLocations[joint_index].locationFlags = XRT_SPACE_RELATION_BITMASK_NONE;
		}
		return XR_SUCCESS;
	}

	locations->time = time_state_monotonic_to_ts_ns(inst->timekeeping, body_joint_set_fb->sample_time_ns);

	locations->confidence = body_joint_set_fb->confidence;
	locations->skeletonChangedCount = body_joint_set_fb->skeleton_changed_count;

	for (size_t joint_index = 0; joint_index < body_joint_count; ++joint_index) {
		const struct xrt_body_joint_location_fb *src_joint = &src_body_joints[joint_index];
		XrBodyJointLocationFB *dst_joint = &locations->jointLocations[joint_index];

		dst_joint->locationFlags = xrt_to_xr_space_location_flags(src_joint->relation.relation_flags);

		struct xrt_space_relation result;
		struct xrt_relation_chain chain = {0};
		m_relation_chain_push_relation(&chain, &src_joint->relation);
		m_relation_chain_push_relation(&chain, &T_base_body);
		m_relation_chain_resolve(&chain, &result);
		OXR_XRT_POSE_TO_XRPOSEF(result.pose, dst_joint->pose);
	}

#ifdef OXR_HAVE_META_body_tracking_fidelity
	XrBodyTrackingFidelityStatusMETA *fidelity_status = OXR_GET_OUTPUT_FROM_CHAIN(
	    locations, XR_TYPE_BODY_TRACKING_FIDELITY_STATUS_META, XrBodyTrackingFidelityStatusMETA);
	if (fidelity_status != NULL) {
		OXR_VERIFY_EXTENSION(&log, body_tracker_fb->sess->sys->inst, META_body_tracking_fidelity);
		if (!body_tracker_fb->xdev->body_tracking_fidelity_supported) {
			return oxr_error(&log, XR_ERROR_FEATURE_UNSUPPORTED,
			                 "body tracking device does not support XR_META_body_tracking_fidelity");
		}
		fidelity_status->fidelity = (XrBodyTrackingFidelityMETA)body_joint_set_fb->exts.fidelity_status;
	}
#endif

	return XR_SUCCESS;
}

#ifdef OXR_HAVE_META_body_tracking_fidelity
XRAPI_ATTR XrResult XRAPI_CALL
oxr_xrRequestBodyTrackingFidelityMETA(XrBodyTrackerFB bodyTracker, const XrBodyTrackingFidelityMETA fidelity)
{
	OXR_TRACE_MARKER();

	struct oxr_logger log;
	struct oxr_body_tracker_fb *body_tracker_fb = NULL;
	OXR_VERIFY_BODY_TRACKER_FB_AND_INIT_LOG(&log, bodyTracker, body_tracker_fb,
	                                        "xrRequestBodyTrackingFidelityMETA");
	OXR_VERIFY_SESSION_NOT_LOST(&log, body_tracker_fb->sess);
	OXR_VERIFY_ARG_NOT_NULL(&log, body_tracker_fb->xdev);
	OXR_VERIFY_EXTENSION(&log, body_tracker_fb->sess->sys->inst, META_body_tracking_fidelity);

	if (!body_tracker_fb->xdev->body_tracking_fidelity_supported) {
		return oxr_error(&log, XR_ERROR_FEATURE_UNSUPPORTED,
		                 "Body tracking device does not support this operation");
	}
	return xrt_device_set_body_tracking_fidelity_meta(body_tracker_fb->xdev,
	                                                  (enum xrt_body_tracking_fidelity_meta)fidelity);
}
#endif

#endif

/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "SignalPhaseAndTiming"
 * 	found in "SignalPhaseAndTiming.asn"
 */

#ifndef	_SPAT_H_
#define	_SPAT_H_


#include <asn_application.h>

/* Including external dependencies */
#include "MsgCount.h"
#include "MinuteOfTheYear.h"
#include "DSecond.h"
#include "DescriptiveName.h"
#include "IntersectionStateList.h"
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SPAT */
typedef struct SPAT {
	MsgCount_t	 msgCnt;
	MinuteOfTheYear_t	*moy	/* OPTIONAL */;
	DSecond_t	*timeStamp	/* OPTIONAL */;
	DescriptiveName_t	*name	/* OPTIONAL */;
	IntersectionStateList_t	 intersections;
	/*
	 * This type is extensible,
	 * possible extensions are below.
	 */
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} SPAT_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_SPAT;
extern asn_SEQUENCE_specifics_t asn_SPC_SPAT_specs_1;
extern asn_TYPE_member_t asn_MBR_SPAT_1[5];

#ifdef __cplusplus
}
#endif

#endif	/* _SPAT_H_ */
#include <asn_internal.h>

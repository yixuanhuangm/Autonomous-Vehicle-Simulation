/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "RSI"
 * 	found in "RSI.asn"
 */

#ifndef	_RTSList_H_
#define	_RTSList_H_


#include <asn_application.h>

/* Including external dependencies */
#include <asn_SEQUENCE_OF.h>
#include <constr_SEQUENCE_OF.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct RTSData;

/* RTSList */
typedef struct RTSList {
	A_SEQUENCE_OF(struct RTSData) list;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} RTSList_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_RTSList;
extern asn_SET_OF_specifics_t asn_SPC_RTSList_specs_1;
extern asn_TYPE_member_t asn_MBR_RTSList_1[1];
extern asn_per_constraints_t asn_PER_type_RTSList_constr_1;

#ifdef __cplusplus
}
#endif

/* Referred external types */
#include "RTSData.h"

#endif	/* _RTSList_H_ */
#include <asn_internal.h>

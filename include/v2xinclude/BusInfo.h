/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "BusStation"
 * 	found in "BusStation.asn"
 */

#ifndef	_BusInfo_H_
#define	_BusInfo_H_


#include <asn_application.h>

/* Including external dependencies */
#include <OCTET_STRING.h>
#include <NativeInteger.h>
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BusInfo */
typedef struct BusInfo {
	OCTET_STRING_t	 busNumber;
	OCTET_STRING_t	*departureStation	/* OPTIONAL */;
	OCTET_STRING_t	*terminalStation	/* OPTIONAL */;
	long	 ingressPathId;
	long	 stopAreaId;
	long	 waitingNumber;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} BusInfo_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_BusInfo;
extern asn_SEQUENCE_specifics_t asn_SPC_BusInfo_specs_1;
extern asn_TYPE_member_t asn_MBR_BusInfo_1[6];

#ifdef __cplusplus
}
#endif

#endif	/* _BusInfo_H_ */
#include <asn_internal.h>

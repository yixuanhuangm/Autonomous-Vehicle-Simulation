/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "BusStation"
 * 	found in "BusStation.asn"
 */

#ifndef	_BusStationNum_H_
#define	_BusStationNum_H_


#include <asn_application.h>

/* Including external dependencies */
#include <NativeInteger.h>
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BusStationNum */
typedef struct BusStationNum {
	long	*regionId	/* OPTIONAL */;
	long	 stationId;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} BusStationNum_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_BusStationNum;
extern asn_SEQUENCE_specifics_t asn_SPC_BusStationNum_specs_1;
extern asn_TYPE_member_t asn_MBR_BusStationNum_1[2];

#ifdef __cplusplus
}
#endif

#endif	/* _BusStationNum_H_ */
#include <asn_internal.h>